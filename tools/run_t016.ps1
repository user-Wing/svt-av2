# T016 — reproducible S1/S2/S4 runs (Windows side + WSL side).
#
# Usage (from the AV2 root, after activating the toolchain):
#   . .\activate.ps1 -Quiet
#   powershell -NoProfile -File svt-av2\tools\run_t016.ps1 -Step S1
#   powershell -NoProfile -File svt-av2\tools\run_t016.ps1 -Step S2
#   powershell -NoProfile -File svt-av2\tools\run_t016.ps1 -Step S4-smoke
#   powershell -NoProfile -File svt-av2\tools\run_t016.ps1 -Step S4-1000
#
# This wrapper only composes the T016-owned entry points. It does NOT modify
# T014's runner/corpus/schema, and it never turns a missing tool into a PASS.
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('S1', 'S2', 'S3-selftest', 'S4-smoke', 'S4-1000')]
    [string]$Step,

    [string]$App = "svt-av2\build\T016\Win-Release\bin\SvtAv2EncApp.exe",
    [string]$Avmdec = "C:\Private\ffmpeg-native\deps-full\AV2\AVM-AV2-TEST\tools\bin\avmdec.exe",
    [string]$Distro = "ARXChem-Ubuntu-22.04-WSL1"
)

$ErrorActionPreference = 'Continue'
$py = "python"

switch ($Step) {
    'S1' {
        # Platform matrix: Windows MSVC Debug/Release + Linux GCC Release/Debug/San + clang probe.
        & $py svt-av2\tools\check_platform.py --out-dir svt-av2\artifacts\T016\S1
        exit $LASTEXITCODE
    }
    'S2' {
        # Prove the ASan/UBSan/LSan detector environment itself works in WSL.
        wsl -d $Distro --exec bash /mnt/c/Private/ffmpeg-native/deps-full/AV2/svt-av2/artifacts/T016/S2/run_san_env.sh
        exit $LASTEXITCODE
    }
    'S3-selftest' {
        # T016-owned tool self-tests + T014 entry smoke (entry is reused, not modified).
        & $py svt-av2\tools\test_check_platform.py
        $rc1 = $LASTEXITCODE
        & $py svt-av2\tools\run_matrix.py --self-test
        $rc2 = $LASTEXITCODE
        exit ([Math]::Max($rc1, $rc2))
    }
    'S4-smoke' {
        $env:AV2_AVMDEC = $Avmdec
        & $py svt-av2\tools\run_stability.py --app $App --frames 5 --width 64 --height 64 `
            --out-dir svt-av2\artifacts\T016\S4\smoke64
        exit $LASTEXITCODE
    }
    'S4-1000' {
        # NOTE: on the M1 mid-grey path this proves count/order/no-crash only.
        # It is NOT an M2 stability pass until T013/T012 deliver real pixels.
        $env:AV2_AVMDEC = $Avmdec
        & $py svt-av2\tools\run_stability.py --app $App --frames 1000 --width 64 --height 64 `
            --out-dir svt-av2\artifacts\T016\S4\m1path1000
        exit $LASTEXITCODE
    }
}
