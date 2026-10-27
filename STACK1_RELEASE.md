# Stack1 experimental snapshot — 2026-10-27

This publication contains the frozen stack1 source matching the recorded
17-point 2D single-frame comparison with AOM. All original source files
are preserved byte for byte against publication/stack1/stack1-source-manifest.json.

Matched-quality VMAF 93–95 size ratio versus AOM:
- PCHIP: 1.0743774136093873x
- Linear log interpolation: 1.0751135565656058x

These are results on one development image, not general video compression
claims. The recorded wide-range audit passed; cross-content matched-quality
acceptance is incomplete, holdout was not used, and performance was not measured.
This is an experimental snapshot, not a formal project version promotion.
The existing LICENSE remains unresolved and has not been replaced.

Original audits, per-point metrics, job configurations, build script, source
manifest and development report are under publication/stack1/. Absolute paths
in those historical records identify the original test environment.
The source README describes earlier historical stages; this note identifies
the exact published snapshot. Build instructions remain in README.md.
