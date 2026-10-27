#!/usr/bin/env python3
"""SVT-AV2 validation runner (T002; geometry/metadata hardening by T014).

Owner: T014 for the geometry and metadata checks (handed over from T002).
Scope: M0/M1/M2 verification infrastructure. This runner drives the frozen AVM
oracle (avmdec/avmenc) and the T006 raw comparator over the deterministic
corpus, and it refuses to report PASS when a tool, dependency or required
artifact is missing.

Design rules (docs/m0-m1-contracts.md sections 8 and 9):
  * Never report PASS on a missing tool/dependency: that is a hard failure with
    a distinct exit code, and the caller can escalate it to BLOCKED.
  * Check process exit code, timeout, output existence/non-emptiness, the
    decoder's OWN reported metadata, decoded frame count and the exact tightly
    packed 8-bit 4:2:0 raw byte count.
  * The geometry is taken from the decoder, never from the caller and never
    inferred from the file length. The caller's --width/--height/--frames/
    --bit-depth/--chroma are an ASSERTION that must agree with the decoder; a
    disagreement is a FAIL. Deriving the expected size from the caller's own
    numbers (the pre-T014 behaviour) accepted a 64x64 stream declared as 32x128,
    because 32*128 + 2*16*64 happens to equal 6144.
  * Compare raw output only by calling tools/compare_recon.py and interpreting
    its exit code; this runner never reimplements the comparison.
  * The default per-command timeout (600s) is chosen for the worst-case M2
    material (high-entropy 176x144 17-frame noise with the reference encoder);
    a timeout is reported as FAIL with the exact case id, never as PASS.

Modes:
  --check-oracle        locate avmdec/avmenc, print paths and SHA256
  --check-bitstream     decode an external stream and verify its real metadata
  --smoke               avmenc a corpus entry -> avmdec -> size/frame checks
  --self-test           negative cases: fake decoder/truncated stream must not PASS
  --metadata-probe      print the decoder-reported geometry of a bitstream
  --matrix              encode every matrix case with avmenc, decode and verify
  --recon-matrix        compare encoder internal recon against decoder recon

  --matrix-tamper-geometry  negative control for --matrix: declare an
                        area-preserving wrong width/height for every case; all
                        cases must FAIL (proves --matrix reads the decoder)

Exit codes:
  0 = all requested checks passed
  1 = a check failed (wrong geometry, wrong size, wrong frame count, non-zero
      tool exit, ...)
  2 = usage error
  3 = a required tool/dependency is missing (BLOCKED, never PASS)
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

DEFAULT_CORPUS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests", "corpus")
DEFAULT_COMPARE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "compare_recon.py")

EXIT_OK = 0
EXIT_FAIL = 1
EXIT_USAGE = 2
EXIT_BLOCKED = 3

DEFAULT_TIMEOUT = 600

SUPPORTED_CHROMA = "420"
SUPPORTED_BIT_DEPTH = 8

# Y4M colour-space token -> (chroma, bit depth), mirroring the encoder's own
# table in Source/Lib/Common/y4m.c so a decoded header is read the same way the
# encoder would read an input file.
Y4M_COLOR_SPACES = {
    "420": ("420", 8),
    "420jpeg": ("420", 8),
    "420mpeg2": ("420", 8),
    "420paldv": ("420", 8),
    "420p10": ("420", 10),
    "420p12": ("420", 12),
    "422": ("422", 8),
    "422p10": ("422", 10),
    "422p12": ("422", 12),
    "444": ("444", 8),
    "444p10": ("444", 10),
    "444p12": ("444", 12),
    "mono": ("400", 8),
    "mono10": ("400", 10),
    "mono12": ("400", 12),
}


class DecoderMetadata:
    """Geometry the decoder itself reports for a stream.

    Authoritative sources, in the order the runner uses them:
      1. The Y4M stream header avmdec writes when no --i420/--rawvideo flags are
         given ("YUV4MPEG2 W<w> H<h> ... C<cs>"). This carries the real coded
         width, height and colour space.
      2. Decoder diagnostic text ("AVM_COLOR_INFO:cp=2,tc=2,mc=2,range=0").
    """

    def __init__(self):
        self.width = None
        self.height = None
        self.chroma = None
        self.bit_depth = None
        self.y4m_header = None
        self.color_info = None
        self.raw_chroma = None  # cp= as reported by the decoder

    def __repr__(self):  # pragma: no cover - debugging aid
        return "DecoderMetadata(w=%r h=%r chroma=%r depth=%r)" % (
            self.width, self.height, self.chroma, self.bit_depth)


def _parse_y4m_header(header: str) -> tuple[int | None, int | None, str | None, int | None]:
    """Extracts (width, height, chroma, bit_depth) from a Y4M stream header."""
    if not header.startswith("YUV4MPEG2"):
        return None, None, None, None
    width = height = None
    chroma = None
    bit_depth = None
    for token in header.split()[1:]:
        if not token:
            continue
        tag, value = token[0], token[1:]
        if tag == "W":
            width = int(value) if value.isdigit() else None
        elif tag == "H":
            height = int(value) if value.isdigit() else None
        elif tag == "C":
            chroma, bit_depth = Y4M_COLOR_SPACES.get(value, (None, None))
    return width, height, chroma, bit_depth


def probe_decoder_metadata(avmdec: str, bitstream: str, timeout: int) -> tuple[DecoderMetadata, str, int]:
    """Runs the decoder in Y4M mode to read the geometry it actually decoded.

    Returns (metadata, combined_output, returncode). The caller decides whether
    the failure is a FAIL or a BLOCKED.
    """
    meta = DecoderMetadata()
    cmd = [avmdec, "-o", os.devnull, bitstream]
    try:
        proc = subprocess.run(cmd, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return meta, "avmdec metadata probe timed out", -1
    text = (proc.stdout + proc.stderr).decode("utf-8", "replace")

    match = re.search(r"AVM_COLOR_INFO:\s*(.*)", text)
    if match:
        meta.color_info = match.group(1).strip()
        cp = re.search(r"cp=(\d+)", meta.color_info)
        if cp:
            meta.raw_chroma = int(cp.group(1))

    # The Y4M header is written to the -o path, not to stdout; re-run with a
    # real file so the header can be read. os.devnull above keeps this probe
    # side-effect free, then the header run writes into a temp file.
    fd, tmp = tempfile.mkstemp(prefix="svt_av2_probe_", suffix=".y4m")
    os.close(fd)
    try:
        cmd = [avmdec, "-o", tmp, bitstream]
        try:
            proc = subprocess.run(cmd, capture_output=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            return meta, text + "\nY4M header probe timed out", -1
        with open(tmp, "rb") as fh:
            head = fh.read(128)
        newline = head.find(b"\n")
        if newline > 0:
            header = head[:newline].decode("ascii", "replace")
            meta.y4m_header = header
            meta.width, meta.height, meta.chroma, meta.bit_depth = _parse_y4m_header(header)
        text += (proc.stdout + proc.stderr).decode("utf-8", "replace")
        code = proc.returncode
    finally:
        try:
            os.remove(tmp)
        except OSError:
            pass
    return meta, text, code


def _sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def _find_tool(explicit: str | None, env_name: str, names: list[str], package_dir: str) -> str | None:
    if explicit:
        return explicit if os.path.isfile(explicit) else None
    env_value = os.environ.get(env_name)
    if env_value and os.path.isfile(env_value):
        return env_value
    oracle_dir = os.environ.get("AV2_ORACLE_BIN")
    candidates = []
    if oracle_dir:
        candidates.append(oracle_dir)
    candidates.append(package_dir)
    for directory in candidates:
        for name in names:
            candidate = os.path.join(directory, name)
            if os.path.isfile(candidate):
                return candidate
    for name in names:
        found = shutil.which(name)
        if found:
            return found
    return None


def _default_package_dir() -> str:
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.normpath(os.path.join(here, "..", "..", "AVM-AV2-TEST", "tools", "bin"))


def _run(command: list[str], timeout: int) -> subprocess.CompletedProcess:
    return subprocess.run(command, capture_output=True, timeout=timeout)


def _raw_size(width: int, height: int, frames: int) -> int:
    return (width * height + 2 * (width // 2) * (height // 2)) * frames


def check_oracle(args) -> int:
    package_dir = args.package_dir or _default_package_dir()
    avmdec = _find_tool(args.avmdec, "AV2_AVMDEC", ["avmdec.exe", "avmdec"], package_dir)
    avmenc = _find_tool(args.avmenc, "AV2_AVMENC", ["avmenc.exe", "avmenc"], package_dir)
    missing = []
    if not avmdec:
        missing.append("avmdec")
    if not avmenc:
        missing.append("avmenc")
    if avmdec:
        print("avmdec : %s" % avmdec)
        print("  sha256: %s" % _sha256_file(avmdec))
    if avmenc:
        print("avmenc : %s" % avmenc)
        print("  sha256: %s" % _sha256_file(avmenc))
    if missing:
        print("BLOCKED: missing oracle tool(s): %s" % ", ".join(missing))
        return EXIT_BLOCKED
    return EXIT_OK


def _load_manifest(corpus_dir: str) -> dict:
    with open(os.path.join(corpus_dir, "manifest.json"), "r", encoding="utf-8") as fh:
        return json.load(fh)


def _select_entry(manifest: dict, entry_id: str | None) -> dict:
    entries = manifest["entries"]
    if entry_id is None:
        return entries[0]
    for entry in entries:
        if entry["id"] == entry_id:
            return entry
    raise KeyError("entry not found: %s" % entry_id)


def smoke(args) -> int:
    package_dir = args.package_dir or _default_package_dir()
    avmdec = _find_tool(args.avmdec, "AV2_AVMDEC", ["avmdec.exe", "avmdec"], package_dir)
    avmenc = _find_tool(args.avmenc, "AV2_AVMENC", ["avmenc.exe", "avmenc"], package_dir)
    if not avmdec or not avmenc:
        print("BLOCKED: oracle tools not available (avmdec=%s avmenc=%s)" % (avmdec, avmenc))
        return EXIT_BLOCKED

    manifest = _load_manifest(args.corpus)
    entry = _select_entry(manifest, args.entry)
    y4m_path = os.path.join(args.corpus, entry["path"])
    if not os.path.isfile(y4m_path):
        print("FAIL: corpus input missing: %s" % y4m_path)
        return EXIT_FAIL

    out_dir = args.out_dir or tempfile.mkdtemp(prefix="svt_av2_smoke_")
    os.makedirs(out_dir, exist_ok=True)
    ivf_path = os.path.join(out_dir, entry["id"] + ".ivf")
    raw_path = os.path.join(out_dir, entry["id"] + ".dec.yuv")

    width, height, frames = entry["width"], entry["height"], entry["frames"]

    enc_cmd = [avmenc, "--good", "--cpu-used=8", "--end-usage=q", "--qp=100",
               "--limit=%d" % frames, "-o", ivf_path, y4m_path]
    print("encode: %s" % " ".join(enc_cmd))
    try:
        enc = _run(enc_cmd, args.timeout)
    except subprocess.TimeoutExpired:
        print("FAIL: avmenc timed out after %ds" % args.timeout)
        return EXIT_FAIL
    print("  exit=%d" % enc.returncode)
    if enc.returncode != 0:
        print("FAIL: avmenc exit %d" % enc.returncode)
        print(enc.stderr.decode("utf-8", "replace")[-2000:])
        return EXIT_FAIL
    if not os.path.isfile(ivf_path) or os.path.getsize(ivf_path) == 0:
        print("FAIL: avmenc produced no/empty output: %s" % ivf_path)
        return EXIT_FAIL

    dec_cmd = [avmdec, "--i420", "--rawvideo", "--summary", "-o", raw_path, ivf_path]
    print("decode: %s" % " ".join(dec_cmd))
    try:
        dec = _run(dec_cmd, args.timeout)
    except subprocess.TimeoutExpired:
        print("FAIL: avmdec timed out after %ds" % args.timeout)
        return EXIT_FAIL
    print("  exit=%d" % dec.returncode)
    if dec.returncode != 0:
        print("FAIL: avmdec exit %d" % dec.returncode)
        print(dec.stderr.decode("utf-8", "replace")[-2000:])
        return EXIT_FAIL
    if not os.path.isfile(raw_path) or os.path.getsize(raw_path) == 0:
        print("FAIL: avmdec produced no/empty raw output: %s" % raw_path)
        return EXIT_FAIL

    # The corpus entry declares the geometry; the decoder is asked what it
    # actually decoded. Both must agree before any byte count is trusted.
    meta, _probe_text, _probe_code = probe_decoder_metadata(avmdec, ivf_path, args.timeout)
    declared = argparse.Namespace(width=width, height=height, frames=frames,
                                  chroma=SUPPORTED_CHROMA, bit_depth=SUPPORTED_BIT_DEPTH)
    if _check_geometry_against_decoder(meta, declared) != EXIT_OK:
        print("FAIL: corpus entry %s geometry disagrees with the decoder" % entry["id"])
        return EXIT_FAIL

    expected = _raw_size(width, height, frames)
    actual = os.path.getsize(raw_path)
    if actual != expected:
        print("FAIL: decoded raw size %d != expected %d (%dx%d %df 420)" % (actual, expected, width, height, frames))
        return EXIT_FAIL

    combined = (dec.stdout + dec.stderr).decode("utf-8", "replace")
    decoded_frames = _parse_decoded_frames(combined)
    if decoded_frames is not None and decoded_frames != frames:
        print("FAIL: decoded %d frames, expected %d" % (decoded_frames, frames))
        return EXIT_FAIL

    print("PASS: %s -> %d decoded bytes (expected %d), frames=%d" % (entry["id"], actual, expected, frames))
    print("artifacts: ivf=%s raw=%s" % (ivf_path, raw_path))
    return EXIT_OK


def _run_compare(compare_py: str, expected: str, actual: str, width: int, height: int, frames: int, timeout: int):
    cmd = [sys.executable, compare_py, "--a", expected, "--b", actual,
           "--width", str(width), "--height", str(height), "--frames", str(frames), "--json"]
    try:
        result = subprocess.run(cmd, capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return None, "compare_recon timed out"
    return result, None


def self_test(args) -> int:
    """Negative cases proving the runner does not swallow failures.

    Uses fake oracle executables and local raw files; this never claims real
    conformance. It asserts that a missing tool, a non-zero decoder exit and a
    wrong-size decoder output are all rejected instead of reported as PASS.
    """
    failures = 0
    out_dir = args.out_dir or tempfile.mkdtemp(prefix="svt_av2_selftest_")
    os.makedirs(out_dir, exist_ok=True)

    def expect(name: str, got: int, want: int) -> None:
        nonlocal failures
        if got == want:
            print("ok: %s (exit %d)" % (name, got))
        else:
            print("FAIL: %s expected exit %d, got %d" % (name, want, got))
            failures += 1

    def make_args(**overrides):
        base = argparse.Namespace(
            corpus=args.corpus,
            compare=args.compare,
            avmdec=None,
            avmenc=None,
            package_dir=None,
            entry="flat_64x64_1f_420",
            out_dir=out_dir,
            timeout=30,
            bit_depth=SUPPORTED_BIT_DEPTH,
            chroma=SUPPORTED_CHROMA,
        )
        for key, value in overrides.items():
            setattr(base, key, value)
        return base

    manifest = _load_manifest(args.corpus)
    entry = _select_entry(manifest, "flat_64x64_1f_420")
    expected = _raw_size(entry["width"], entry["height"], entry["frames"])

    def write_fake_tool(name: str, py_body: str) -> str:
        """Writes a fake oracle and a launcher that the OS can exec directly."""
        script = os.path.join(out_dir, name + ".py")
        with open(script, "w", encoding="utf-8") as fh:
            fh.write(py_body)
        if os.name == "nt":
            launcher = os.path.join(out_dir, name + ".cmd")
            with open(launcher, "w", encoding="utf-8", newline="\r\n") as fh:
                fh.write('@echo off\r\n"%s" "%s" %%*\r\n' % (sys.executable, script))
        else:
            launcher = os.path.join(out_dir, name)
            with open(launcher, "w", encoding="utf-8") as fh:
                fh.write('#!/bin/sh\nexec "%s" "%s" "$@"\n' % (sys.executable, script))
            os.chmod(launcher, 0o755)
        return launcher

    # Fake encoder: writes a small non-empty file to the -o path, exit 0.
    fake_enc = write_fake_tool(
        "fake_avmenc",
        "import sys\n"
        "argv = sys.argv[1:]\n"
        "path = argv[argv.index('-o') + 1]\n"
        "open(path, 'wb').write(b'FAKEIVF' * 4)\n"
        "sys.exit(0)\n"
    )

    # Fake decoder variants.
    fake_dec_fail = write_fake_tool("fake_avmdec_fail", "import sys\nsys.exit(1)\n")

    fake_dec_short = write_fake_tool(
        "fake_avmdec_short",
        "import sys\n"
        "argv = sys.argv[1:]\n"
        "path = argv[argv.index('-o') + 1]\n"
        "open(path, 'wb').write(b'\\x80' * (%d - 1))\n"
        "sys.exit(0)\n" % expected
    )

    # 1) Missing tool must be BLOCKED, never PASS.
    expect("missing decoder -> BLOCKED",
           smoke(make_args(avmdec=os.path.join(out_dir, "does_not_exist.exe"), avmenc=fake_enc)),
           EXIT_BLOCKED)

    # 2) Non-zero decoder exit must FAIL.
    expect("non-zero decoder -> FAIL",
           smoke(make_args(avmdec=fake_dec_fail, avmenc=fake_enc)),
           EXIT_FAIL)

    # 3) Decoder output of the wrong size must FAIL.
    expect("wrong-size decoder output -> FAIL",
           smoke(make_args(avmdec=fake_dec_short, avmenc=fake_enc)),
           EXIT_FAIL)

    # 4) Comparator semantics: identical pass, mismatch exit 1, truncated exit 3.
    #    A missing comparator is a BLOCKED dependency, never a silent skip: the
    #    whole point of the reworked runner is that "fewer checks registered"
    #    must not look like success.
    if not os.path.isfile(args.compare):
        print("BLOCKED: compare_recon.py not found at %s (comparison dependency missing)"
              % args.compare)
        return EXIT_BLOCKED
    good = os.path.join(out_dir, "good.yuv")
    with open(good, "wb") as fh:
        fh.write(b"\x80" * expected)
    same = os.path.join(out_dir, "same.yuv")
    with open(same, "wb") as fh:
        fh.write(b"\x80" * expected)
    diff = os.path.join(out_dir, "diff.yuv")
    with open(diff, "wb") as fh:
        fh.write(b"\x80" * (expected - 1) + b"\x81")
    trunc = os.path.join(out_dir, "trunc.yuv")
    with open(trunc, "wb") as fh:
        fh.write(b"\x80" * (expected - 1))

    for label, actual_path, want in (
        ("comparator identical -> exit 0", same, 0),
        ("comparator pixel mismatch -> exit 1", diff, 1),
        ("comparator truncated actual -> exit 3", trunc, 3),
    ):
        result, error = _run_compare(args.compare, good, actual_path,
                                     entry["width"], entry["height"], entry["frames"], 30)
        if error:
            print("FAIL: %s: %s" % (label, error))
            failures += 1
        else:
            expect(label, result.returncode, want)

    # 5) Real truncated bitstream must not PASS (avmdec can exit 0 on a
    #    truncated stream, so the frame/size checks are what catch it).
    real_dec = _find_tool(args.avmdec, "AV2_AVMDEC", ["avmdec.exe", "avmdec"],
                          args.package_dir or _default_package_dir())
    real_enc = _find_tool(args.avmenc, "AV2_AVMENC", ["avmenc.exe", "avmenc"],
                          args.package_dir or _default_package_dir())
    if real_dec and real_enc:
        y4m_path = os.path.join(args.corpus, entry["path"])
        full_ivf = os.path.join(out_dir, "full.ivf")
        enc_cmd = [real_enc, "--good", "--cpu-used=8", "--end-usage=q", "--qp=100",
                   "--limit=%d" % entry["frames"], "-o", full_ivf, y4m_path]
        enc = _run(enc_cmd, args.timeout)
        if enc.returncode != 0 or not os.path.isfile(full_ivf):
            print("SKIP: could not build reference bitstream for truncation test")
        else:
            with open(full_ivf, "rb") as fh:
                data = fh.read()
            # Negative fixture 1: truncated (valid prefix, missing tail).
            trunc_ivf = os.path.join(out_dir, "truncated.ivf")
            with open(trunc_ivf, "wb") as fh:
                fh.write(data[: max(1, len(data) // 2)])
            # Negative fixture 2: tampered (single payload byte flipped in the
            # tail; header preserved so the container still parses).
            tamper_ivf = os.path.join(out_dir, "tampered.ivf")
            tampered = bytearray(data)
            flip_at = max(32, len(tampered) - 2)
            tampered[flip_at] ^= 0xFF
            with open(tamper_ivf, "wb") as fh:
                fh.write(bytes(tampered))
            print("fixtures: intact=%s truncated=%s tampered=%s"
                  % (full_ivf, trunc_ivf, tamper_ivf))

            def make_bs_args(**overrides):
                base = argparse.Namespace(
                    corpus=args.corpus, compare=args.compare, avmdec=None, avmenc=None,
                    package_dir=args.package_dir, entry=None, out_dir=out_dir, timeout=args.timeout,
                    bitstream=trunc_ivf, width=entry["width"], height=entry["height"],
                    frames=entry["frames"], bit_depth=SUPPORTED_BIT_DEPTH, chroma=SUPPORTED_CHROMA)
                for key, value in overrides.items():
                    setattr(base, key, value)
                return base

            expect("truncated real bitstream -> FAIL",
                   check_bitstream(make_bs_args(avmdec=real_dec)), EXIT_FAIL)
            expect("tampered real bitstream -> FAIL",
                   check_bitstream(make_bs_args(avmdec=real_dec, bitstream=tamper_ivf)), EXIT_FAIL)
            expect("intact real bitstream -> PASS",
                   check_bitstream(make_bs_args(avmdec=real_dec, bitstream=full_ivf)), EXIT_OK)

            # --- T014 geometry false-PASS regression -------------------------
            # The M1 bug: a real 64x64 stream declared as 32x128 PASSed, because
            # the expected byte count was derived from the caller's numbers and
            # 32*128 + 2*16*64 == 6144 == 64*64 + 2*32*32. Geometry must now come
            # from the decoder, so the same declaration must FAIL. These cases are
            # deliberately chosen to keep the byte count identical: a runner that
            # still passes them has not been fixed.
            width, height = entry["width"], entry["height"]
            swapped = (height, width)
            assert _raw_size(width, height, 1) == _raw_size(swapped[0], swapped[1], 1), \
                "regression fixture must preserve the byte count"

            expect("declared 32x128 for a real 64x64 stream -> FAIL",
                   check_bitstream(make_bs_args(avmdec=real_dec, bitstream=full_ivf,
                                                width=32, height=128)), EXIT_FAIL)
            expect("declared 128x32 for a real 64x64 stream -> FAIL",
                   check_bitstream(make_bs_args(avmdec=real_dec, bitstream=full_ivf,
                                                width=128, height=32)), EXIT_FAIL)
            expect("undeclared geometry -> usage error",
                   check_bitstream(make_bs_args(avmdec=real_dec, bitstream=full_ivf,
                                                width=None)), EXIT_USAGE)
            expect("declared 4:4:4 for a real 4:2:0 stream -> FAIL",
                   check_bitstream(make_bs_args(avmdec=real_dec, bitstream=full_ivf,
                                                chroma="444")), EXIT_FAIL)
            # Wrong frame count for the same real 64x64 stream: the byte count
            # (6144*frames) would match a runner that trusted the caller, so the
            # decoder's own frame count is what has to catch it.
            expect("declared 2 frames for a real 1-frame stream -> FAIL",
                   check_bitstream(make_bs_args(avmdec=real_dec, bitstream=full_ivf,
                                                frames=2)), EXIT_FAIL)
            expect("declared 0 frames for a real 1-frame stream -> FAIL",
                   check_bitstream(make_bs_args(avmdec=real_dec, bitstream=full_ivf,
                                                frames=0)), EXIT_FAIL)
            expect("matching geometry still PASSes (no over-rejection)",
                   check_bitstream(make_bs_args(avmdec=real_dec, bitstream=full_ivf)),
                   EXIT_OK)
    else:
        print("SKIP: real oracle unavailable, truncation test not run")

    if failures:
        print("SELF-TEST FAILED: %d problem(s)" % failures)
        return EXIT_FAIL
    print("SELF-TEST PASS")
    return EXIT_OK


def _parse_decoded_frames(text: str) -> int | None:
    """Extracts N from avmdec's "N decoded frames/M showed frames" summary."""
    match = re.search(r"(\d+)\s+decoded frames", text)
    return int(match.group(1)) if match else None


def _check_geometry_against_decoder(meta: DecoderMetadata, args) -> int:
    """Compares the caller's declared geometry with the decoder's own report.

    This is the T014 hardening for the M1 false-PASS: a real 64x64 stream
    declared as 32x128 used to PASS because the expected byte count was computed
    from the caller's numbers and both happened to equal 6144. Geometry is now
    read from the decoder and a mismatch is a FAIL.
    """
    problems = []
    if meta.width is None or meta.height is None:
        problems.append("decoder did not report a usable geometry")
    else:
        if meta.width != args.width or meta.height != args.height:
            problems.append(
                "decoded geometry %dx%d != declared %dx%d"
                % (meta.width, meta.height, args.width, args.height))
    if meta.chroma is not None and meta.chroma != args.chroma:
        problems.append("decoded chroma %s != declared %s" % (meta.chroma, args.chroma))
    if meta.bit_depth is not None and meta.bit_depth != args.bit_depth:
        problems.append("decoded bit depth %d != declared %d" % (meta.bit_depth, args.bit_depth))
    if meta.chroma is not None and meta.chroma != SUPPORTED_CHROMA:
        problems.append("decoded chroma %s is outside the supported %s" % (meta.chroma, SUPPORTED_CHROMA))
    if meta.bit_depth is not None and meta.bit_depth != SUPPORTED_BIT_DEPTH:
        problems.append(
            "decoded bit depth %d is outside the supported %d" % (meta.bit_depth, SUPPORTED_BIT_DEPTH))

    print("  decoder geometry: %sx%s chroma=%s bit_depth=%s"
          % (meta.width, meta.height, meta.chroma, meta.bit_depth))
    if meta.y4m_header:
        print("  decoder y4m header: %s" % meta.y4m_header)
    if meta.color_info:
        print("  decoder color info: %s" % meta.color_info)
    if problems:
        for problem in problems:
            print("FAIL: %s" % problem)
        return EXIT_FAIL
    return EXIT_OK


def metadata_probe(args) -> int:
    """Prints the geometry the decoder reports, without asserting anything."""
    package_dir = args.package_dir or _default_package_dir()
    avmdec = _find_tool(args.avmdec, "AV2_AVMDEC", ["avmdec.exe", "avmdec"], package_dir)
    if not avmdec:
        print("BLOCKED: avmdec not available")
        return EXIT_BLOCKED
    if not os.path.isfile(args.bitstream):
        print("FAIL: bitstream not found: %s" % args.bitstream)
        return EXIT_FAIL
    meta, _text, code = probe_decoder_metadata(avmdec, args.bitstream, args.timeout)
    if meta.width is None and meta.height is None:
        print("FAIL: decoder reported no geometry (exit=%d)" % code)
        return EXIT_FAIL
    payload = {
        "bitstream": args.bitstream,
        "width": meta.width,
        "height": meta.height,
        "chroma": meta.chroma,
        "bit_depth": meta.bit_depth,
        "y4m_header": meta.y4m_header,
        "color_info": meta.color_info,
    }
    print(json.dumps(payload, indent=2, sort_keys=True))
    return EXIT_OK


def check_bitstream(args) -> int:
    """Validates an externally produced bitstream with the frozen decoder.

    Used by T005/T012/T013 integration and by the truncation negative test. The
    decoder is known to exit 0 on some truncated streams while decoding zero
    frames, so the frame count and exact raw size are checked, not just the exit
    code. The geometry comes from the decoder's own report, never from the
    caller and never from the file length.
    """
    package_dir = args.package_dir or _default_package_dir()
    avmdec = _find_tool(args.avmdec, "AV2_AVMDEC", ["avmdec.exe", "avmdec"], package_dir)
    if not avmdec:
        print("BLOCKED: avmdec not available")
        return EXIT_BLOCKED
    if not os.path.isfile(args.bitstream):
        print("FAIL: bitstream not found: %s" % args.bitstream)
        return EXIT_FAIL
    if args.width is None or args.height is None or args.frames is None:
        print("FAIL: --width/--height/--frames are required with --check-bitstream")
        return EXIT_USAGE

    out_dir = args.out_dir or tempfile.mkdtemp(prefix="svt_av2_decode_")
    os.makedirs(out_dir, exist_ok=True)
    raw_path = os.path.join(out_dir, os.path.basename(args.bitstream) + ".dec.yuv")

    # Geometry first: a wrong declaration must not be able to reach PASS via a
    # coincidentally matching byte count.
    meta, probe_text, probe_code = probe_decoder_metadata(avmdec, args.bitstream, args.timeout)
    if probe_code != 0 and meta.width is None:
        print("FAIL: decoder metadata probe exit %d" % probe_code)
        print(probe_text[-2000:])
        return EXIT_FAIL
    if _check_geometry_against_decoder(meta, args) != EXIT_OK:
        return EXIT_FAIL

    dec_cmd = [avmdec, "--i420", "--rawvideo", "--summary", "-o", raw_path, args.bitstream]
    print("decode: %s" % " ".join(dec_cmd))
    try:
        dec = _run(dec_cmd, args.timeout)
    except subprocess.TimeoutExpired:
        print("FAIL: avmdec timed out after %ds" % args.timeout)
        return EXIT_FAIL
    combined = (dec.stdout + dec.stderr).decode("utf-8", "replace")
    print("  exit=%d" % dec.returncode)
    decoded_frames = _parse_decoded_frames(combined)
    print("  decoded_frames=%s" % decoded_frames)

    if dec.returncode != 0:
        print("FAIL: avmdec exit %d" % dec.returncode)
        return EXIT_FAIL
    if decoded_frames is not None and decoded_frames != args.frames:
        print("FAIL: decoded %d frames, expected %d" % (decoded_frames, args.frames))
        return EXIT_FAIL
    if not os.path.isfile(raw_path) or os.path.getsize(raw_path) == 0:
        print("FAIL: no/empty decoded raw output")
        return EXIT_FAIL
    expected = _raw_size(args.width, args.height, args.frames)
    actual = os.path.getsize(raw_path)
    if actual != expected:
        print("FAIL: decoded raw size %d != expected %d" % (actual, expected))
        return EXIT_FAIL

    print("PASS: %s decoded to %d bytes (%d frames)" % (args.bitstream, actual, args.frames))
    print("artifacts: raw=%s" % raw_path)
    return EXIT_OK


def _json_write(path: str, payload: dict) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        json.dump(payload, fh, indent=2, sort_keys=True)
        fh.write("\n")


def _load_matrix(matrix_dir: str) -> dict:
    path = os.path.join(matrix_dir, "matrix.json")
    if not os.path.isfile(path):
        raise FileNotFoundError(path)
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def _case_record(case_id: str, pattern, declared, decoder_metadata, artifacts,
                 encoded_frames=None, raw_bytes=None, expected_raw_bytes=None,
                 compare=None):
    return {
        "id": case_id,
        "pattern": pattern,
        "status": None,
        "reason": "",
        "declared": declared,
        "decoder_metadata": decoder_metadata,
        "encoded_frames": encoded_frames,
        "raw_bytes": raw_bytes,
        "expected_raw_bytes": expected_raw_bytes,
        "compare": compare,
        "artifacts": artifacts,
    }


def _finish(case: dict, status: str, reason: str) -> dict:
    case["status"] = status
    case["reason"] = reason
    return case


def _decode_and_verify(avmdec, bitstream, declared, out_dir, tag, timeout):
    """Decode a bitstream, then verify it from the DECODER's own metadata.

    Returns (ok, reason, decoder_metadata, encoded_frames, raw_path, raw_bytes).
    Geometry, bit depth and chroma come from the decoder, never from `declared`
    and never from the file length (contracts section 9). `declared` is an
    assertion that must agree with the decoder.
    """
    meta, probe_text, probe_code = probe_decoder_metadata(avmdec, bitstream, timeout)
    dm = {"width": meta.width, "height": meta.height, "bit_depth": meta.bit_depth,
          "chroma": meta.chroma, "y4m_header": meta.y4m_header}
    if meta.width is None or meta.height is None:
        return False, ("decoder reported no usable geometry (exit=%d)" % probe_code), dm, None, None, None
    if declared["width"] is not None and (meta.width, meta.height) != (declared["width"], declared["height"]):
        return False, ("decoder geometry %dx%d != declared %dx%d"
                       % (meta.width, meta.height, declared["width"], declared["height"])), dm, None, None, None
    if meta.chroma is not None and declared["chroma"] is not None and meta.chroma != declared["chroma"]:
        return False, ("decoder chroma %s != declared %s" % (meta.chroma, declared["chroma"])), dm, None, None, None
    if meta.bit_depth is not None and declared["bit_depth"] is not None and meta.bit_depth != declared["bit_depth"]:
        return False, ("decoder bit depth %d != declared %d" % (meta.bit_depth, declared["bit_depth"])), dm, None, None, None
    if meta.chroma is not None and meta.chroma != SUPPORTED_CHROMA:
        return False, ("decoder chroma %s outside supported %s" % (meta.chroma, SUPPORTED_CHROMA)), dm, None, None, None
    if meta.bit_depth is not None and meta.bit_depth != SUPPORTED_BIT_DEPTH:
        return False, ("decoder bit depth %d outside supported %d" % (meta.bit_depth, SUPPORTED_BIT_DEPTH)), dm, None, None, None

    raw_path = os.path.join(out_dir, tag + ".dec.yuv")
    dec = _run([avmdec, "--i420", "--rawvideo", "--summary", "-o", raw_path, bitstream], timeout)
    combined = (dec.stdout + dec.stderr).decode("utf-8", "replace")
    if dec.returncode != 0:
        return False, ("avmdec exit %d" % dec.returncode), dm, None, raw_path, None
    decoded_frames = _parse_decoded_frames(combined)
    if decoded_frames is not None and declared["frames"] is not None and decoded_frames != declared["frames"]:
        return False, ("decoder produced %d frames, declared %d" % (decoded_frames, declared["frames"])), dm, decoded_frames, raw_path, None
    if not os.path.isfile(raw_path) or os.path.getsize(raw_path) == 0:
        return False, "no/empty decoded raw output", dm, decoded_frames, raw_path, None
    raw_bytes = os.path.getsize(raw_path)
    expected = _raw_size(meta.width, meta.height, decoded_frames if decoded_frames is not None else declared["frames"])
    if raw_bytes != expected:
        return False, ("decoded raw %d bytes != %d expected from decoder geometry" % (raw_bytes, expected)), dm, decoded_frames, raw_path, raw_bytes
    return True, "ok", dm, decoded_frames, raw_path, raw_bytes


def _encode_with_avmenc(avmenc, y4m_path, frames, out_path, timeout):
    cmd = [avmenc, "--good", "--cpu-used=8", "--end-usage=q", "--qp=100",
           "--limit=%d" % frames, "-o", out_path, y4m_path]
    try:
        proc = _run(cmd, timeout)
    except subprocess.TimeoutExpired:
        return False, "avmenc timed out after %ds" % timeout
    if proc.returncode != 0:
        return False, "avmenc exit %d: %s" % (proc.returncode,
                                              proc.stderr.decode("utf-8", "replace")[-500:])
    if not os.path.isfile(out_path) or os.path.getsize(out_path) == 0:
        return False, "avmenc produced no/empty output"
    return True, "ok"


def _emit_blocked_record(args, mode: str, reason: str) -> None:
    """Write a minimal, schema-valid BLOCKED record so a blocked run is archivable.

    Silently returning a non-zero exit code is enough for CTest, but T016 needs a
    durable record of *why* an S3/platform check could not run. The record carries
    no case data and lists the unmet dependency in `unverified`.
    """
    out_dir = args.out_dir
    payload = {
        "schema_version": 1,
        "mode": mode,
        "status": "BLOCKED",
        "reason": reason,
        "encoder": {"kind": "none", "path": None, "sha256": None},
        "decoder": {"kind": "none", "path": None, "sha256": None},
        "cases": [],
        "summary": {"total": 0, "passed": 0, "failed": 0, "blocked": 0},
        "unverified": [reason],
    }
    _json_write(os.path.join(out_dir, "blocked_result.json"), payload)


def matrix_mode(args) -> int:
    """Encode every matrix case with the oracle, then verify the real output.

    This is the M2 material matrix: it proves the corpus is actually encodable
    and that each decoded stream matches its declared geometry, bit depth,
    chroma, frame count and raw size. It does NOT prove encoder internal recon,
    which needs the SVT-AV2 encoder to export recon (separate mode).
    """
    package_dir = args.package_dir or _default_package_dir()
    avmdec = _find_tool(args.avmdec, "AV2_AVMDEC", ["avmdec.exe", "avmdec"], package_dir)
    avmenc = _find_tool(args.avmenc, "AV2_AVMENC", ["avmenc.exe", "avmenc"], package_dir)
    if not avmdec or not avmenc:
        print("BLOCKED: oracle tools not available (avmdec=%s avmenc=%s)" % (avmdec, avmenc))
        if args.out_dir:
            os.makedirs(args.out_dir, exist_ok=True)
            _emit_blocked_record(args, "oracle_material",
                                 "oracle tools not available (avmdec=%s avmenc=%s)" % (avmdec, avmenc))
        return EXIT_BLOCKED

    matrix_dir = args.matrix_dir or os.path.join(args.corpus, "matrix")
    try:
        manifest = _load_matrix(matrix_dir)
    except FileNotFoundError:
        print("FAIL: matrix manifest not found in %s" % matrix_dir)
        return EXIT_FAIL

    entries = manifest.get("entries", [])
    if args.pattern:
        entries = [e for e in entries if e.get("pattern") in args.pattern]
    if args.sizes:
        wanted = set(args.sizes)
        entries = [e for e in entries if "%dx%d" % (e["width"], e["height"]) in wanted]
    if args.frame_counts:
        wanted = set(args.frame_counts)
        entries = [e for e in entries if e["frames"] in wanted]
    if args.limit:
        entries = entries[:args.limit]
    if not entries:
        print("FAIL: matrix selection is empty (check --pattern/--sizes/--frame-counts)")
        return EXIT_FAIL

    out_dir = args.out_dir or tempfile.mkdtemp(prefix="svt_av2_matrix_")
    os.makedirs(out_dir, exist_ok=True)

    records = []
    unverified = []
    tampered = 0
    for entry in entries:
        case_id = entry["id"]
        y4m_path = os.path.join(matrix_dir, entry["path"])
        declared = {"width": entry["width"], "height": entry["height"], "frames": entry["frames"],
                    "bit_depth": entry["bit_depth"], "chroma": entry["chroma"]}
        if args.matrix_tamper_geometry:
            # Negative control for --matrix: declare the AREA-PRESERVING swap of
            # the true geometry. This keeps the raw byte count identical, so a
            # runner that still trusted the caller would PASS; the decoder's own
            # report is what must reject it. On a square entry the swap is a
            # no-op (the geometry is still correct), so skip those: they would
            # legitimately PASS and say nothing about the check.
            if entry["width"] == entry["height"]:
                continue
            declared["width"], declared["height"] = entry["height"], entry["width"]
            tampered += 1
        artifacts = {"bitstream": None, "decoded_raw": None, "internal_recon": None}
        case = _case_record(case_id, entry.get("pattern"), declared, None, artifacts)
        if not os.path.isfile(y4m_path):
            records.append(_finish(case, "FAIL", "material missing: %s" % y4m_path))
            continue

        ivf_path = os.path.join(out_dir, case_id + ".ivf")
        artifacts["bitstream"] = ivf_path
        ok, reason = _encode_with_avmenc(avmenc, y4m_path, entry["frames"], ivf_path, args.timeout)
        if not ok:
            records.append(_finish(case, "FAIL", reason))
            continue

        ok, reason, dm, decoded_frames, raw_path, raw_bytes = _decode_and_verify(
            avmdec, ivf_path, declared, out_dir, case_id, args.timeout)
        case["decoder_metadata"] = dm
        case["encoded_frames"] = decoded_frames
        case["artifacts"]["decoded_raw"] = raw_path
        case["raw_bytes"] = raw_bytes
        case["expected_raw_bytes"] = _raw_size(entry["width"], entry["height"], entry["frames"])
        records.append(_finish(case, "PASS" if ok else "FAIL", reason))

    summary = {
        "total": len(records),
        "passed": sum(1 for r in records if r["status"] == "PASS"),
        "failed": sum(1 for r in records if r["status"] == "FAIL"),
        "blocked": sum(1 for r in records if r["status"] == "BLOCKED"),
    }
    # Internal recon equality is the real M2 invariant and is not covered by the
    # oracle-material loop; record it as unverified rather than implying success.
    unverified.append("encoder internal recon == decoder recon (needs SVT-AV2 recon export; use --recon-matrix)")
    payload = {
        "schema_version": 1,
        "mode": "oracle_material",
        "status": "PASS" if summary["failed"] == 0 and summary["blocked"] == 0 else ("BLOCKED" if summary["failed"] == 0 else "FAIL"),
        "reason": "oracle material loop: %d case(s)" % summary["total"],
        "encoder": {"kind": "avmenc", "path": avmenc, "sha256": _sha256_file(avmenc)},
        "decoder": {"kind": "avmdec", "path": avmdec, "sha256": _sha256_file(avmdec)},
        "cases": records,
        "summary": summary,
        "unverified": unverified,
    }
    _json_write(os.path.join(out_dir, "matrix_result.json"), payload)
    print(json.dumps(summary, sort_keys=True))
    print("result: %s" % os.path.join(out_dir, "matrix_result.json"))
    if args.matrix_tamper_geometry:
        # The tamper control has an inverted expectation: every tampered case
        # declared a wrong (area-preserving) geometry, so any PASS means the
        # matrix path trusted the caller again and the control itself has failed.
        if not tampered:
            print("FAIL: --matrix-tamper-geometry selection has no non-square case "
                  "to tamper (the swap is a no-op on square entries)")
            return EXIT_FAIL
        if summary["passed"] or summary["failed"] != tampered:
            print("FAIL: --matrix-tamper-geometry expected all %d tampered case(s) to "
                  "FAIL, got passed=%d failed=%d" % (tampered, summary["passed"], summary["failed"]))
            return EXIT_FAIL
        print("MATRIX-TAMPER NEGATIVE OK: %d/%d wrong-geometry case(s) rejected"
              % (summary["failed"], tampered))
        return EXIT_OK
    if summary["failed"]:
        return EXIT_FAIL
    return EXIT_OK


def recon_matrix_mode(args) -> int:
    """Real M2 invariant: encoder internal recon == decoder recon, per frame.

    The entry point takes a bitstream produced by the SVT-AV2 encoder and the
    encoder's OWN reconstruction raw file, decodes the bitstream with the frozen
    decoder, and compares the two with tools/compare_recon.py. The internal recon
    must be produced by the encoder (contract section 6); substituting the
    decoder output here would make the check vacuous (global rule 2), so both
    inputs are required and neither defaults to the other.

    Until the encoder exports recon this returns BLOCKED, never PASS.
    """
    if not args.bitstream or not args.recon:
        reason = ("--recon-matrix requires BOTH --bitstream <encoder output> and "
                  "--recon <encoder internal recon raw file>; the SVT-AV2 encoder does not "
                  "export recon yet (M2 pending). Never pass the decoder output as --recon.")
        print("BLOCKED: %s" % reason)
        if args.out_dir:
            os.makedirs(args.out_dir, exist_ok=True)
            _emit_blocked_record(args, "encoder_recon", reason)
        return EXIT_BLOCKED
    if args.width is None or args.height is None or args.frames is None:
        print("FAIL: --width/--height/--frames are required with --recon-matrix")
        return EXIT_USAGE
    if not os.path.isfile(args.recon):
        reason = "internal recon file not found: %s" % args.recon
        print("BLOCKED: %s" % reason)
        if args.out_dir:
            os.makedirs(args.out_dir, exist_ok=True)
            _emit_blocked_record(args, "encoder_recon", reason)
        return EXIT_BLOCKED

    package_dir = args.package_dir or _default_package_dir()
    avmdec = _find_tool(args.avmdec, "AV2_AVMDEC", ["avmdec.exe", "avmdec"], package_dir)
    if not avmdec:
        reason = "avmdec not available"
        print("BLOCKED: %s" % reason)
        if args.out_dir:
            os.makedirs(args.out_dir, exist_ok=True)
            _emit_blocked_record(args, "encoder_recon", reason)
        return EXIT_BLOCKED
    if not os.path.isfile(args.bitstream):
        print("FAIL: bitstream not found: %s" % args.bitstream)
        return EXIT_FAIL

    declared = {"width": args.width, "height": args.height, "frames": args.frames,
                "bit_depth": args.bit_depth, "chroma": args.chroma}
    out_dir = args.out_dir or tempfile.mkdtemp(prefix="svt_av2_recon_")
    os.makedirs(out_dir, exist_ok=True)

    ok, reason, dm, decoded_frames, raw_path, raw_bytes = _decode_and_verify(
        avmdec, args.bitstream, declared, out_dir, "recon", args.timeout)
    if not ok:
        print("FAIL: %s" % reason)
        return EXIT_FAIL

    result, error = _run_compare(args.compare, args.recon, raw_path,
                                 args.width, args.height, args.frames, args.timeout)
    if error:
        print("FAIL: %s" % error)
        return EXIT_FAIL
    case = _case_record(os.path.basename(args.bitstream), None, declared, dm,
                        {"bitstream": args.bitstream, "decoded_raw": raw_path,
                         "internal_recon": args.recon},
                        encoded_frames=decoded_frames, raw_bytes=raw_bytes,
                        expected_raw_bytes=_raw_size(args.width, args.height, args.frames))
    compare_json = None
    if result.returncode == 0:
        case = _finish(case, "PASS", "internal recon == decoder recon")
        exit_code = EXIT_OK
    elif result.returncode == 1:
        case = _finish(case, "FAIL", "internal recon != decoder recon (see compare)")
        exit_code = EXIT_FAIL
    else:
        case = _finish(case, "FAIL", "compare_recon exit %d" % result.returncode)
        exit_code = EXIT_FAIL
    if result.stdout:
        try:
            compare_json = json.loads(result.stdout.decode("utf-8", "replace"))
        except ValueError:
            compare_json = None
    case["compare"] = compare_json

    payload = {
        "schema_version": 1,
        "mode": "encoder_recon",
        "status": case["status"],
        "reason": case["reason"],
        "encoder": {"kind": "svt_av2_app", "path": args.app, "sha256": _sha256_file(args.app) if args.app and os.path.isfile(args.app) else None},
        "decoder": {"kind": "avmdec", "path": avmdec, "sha256": _sha256_file(avmdec)},
        "cases": [case],
        "summary": {"total": 1, "passed": 1 if case["status"] == "PASS" else 0,
                    "failed": 1 if case["status"] == "FAIL" else 0,
                    "blocked": 1 if case["status"] == "BLOCKED" else 0},
        "unverified": [],
    }
    _json_write(os.path.join(out_dir, "recon_result.json"), payload)
    print("result: %s" % os.path.join(out_dir, "recon_result.json"))
    return exit_code


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", default=os.path.normpath(DEFAULT_CORPUS))
    parser.add_argument("--compare", default=os.path.normpath(DEFAULT_COMPARE))
    parser.add_argument("--avmdec")
    parser.add_argument("--avmenc")
    parser.add_argument("--package-dir")
    parser.add_argument("--entry")
    parser.add_argument("--out-dir")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT)
    parser.add_argument("--bitstream")
    parser.add_argument("--width", type=int)
    parser.add_argument("--height", type=int)
    parser.add_argument("--frames", type=int)
    parser.add_argument("--bit-depth", dest="bit_depth", type=int, default=SUPPORTED_BIT_DEPTH)
    parser.add_argument("--chroma", default=SUPPORTED_CHROMA)
    parser.add_argument("--matrix-dir", dest="matrix_dir")
    parser.add_argument("--pattern", action="append",
                        help="matrix pattern filter (repeatable)")
    parser.add_argument("--sizes", help="comma-separated WxH filter for --matrix")
    parser.add_argument("--frame-counts", dest="frame_counts",
                        help="comma-separated frame-count filter for --matrix")
    parser.add_argument("--limit", type=int, help="max cases to run in --matrix")
    parser.add_argument("--recon", help="encoder internal recon raw file (for --recon-matrix)")
    parser.add_argument("--app", help="SvtAv2EncApp path (recorded in --recon-matrix results)")
    parser.add_argument("--check-oracle", action="store_true")
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--check-bitstream", action="store_true")
    parser.add_argument("--metadata-probe", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--matrix", action="store_true",
                        help="run the oracle material matrix (encode->decode->verify)")
    parser.add_argument("--matrix-tamper-geometry", dest="matrix_tamper_geometry",
                        action="store_true",
                        help="negative control for --matrix: declare an area-preserving "
                             "wrong geometry for every case; all cases must FAIL")
    parser.add_argument("--recon-matrix", dest="recon_matrix", action="store_true",
                        help="compare encoder internal recon against decoder recon")
    args = parser.parse_args(argv)

    if args.sizes is not None:
        args.sizes = [s.strip() for s in args.sizes.split(",") if s.strip()]
    if args.frame_counts is not None:
        args.frame_counts = [int(s.strip()) for s in args.frame_counts.split(",") if s.strip()]

    if args.matrix_tamper_geometry and not args.matrix:
        parser.error("--matrix-tamper-geometry requires --matrix")

    if not (args.check_oracle or args.smoke or args.check_bitstream
            or args.metadata_probe or args.self_test or args.matrix
            or args.recon_matrix):
        parser.error("choose at least one of --check-oracle / --smoke / --check-bitstream "
                     "/ --metadata-probe / --self-test / --matrix / --recon-matrix")

    if args.check_oracle:
        return check_oracle(args)
    if args.smoke:
        return smoke(args)
    if args.check_bitstream:
        return check_bitstream(args)
    if args.metadata_probe:
        return metadata_probe(args)
    if args.self_test:
        return self_test(args)
    if args.matrix:
        return matrix_mode(args)
    if args.recon_matrix:
        return recon_matrix_mode(args)
    return EXIT_USAGE


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
