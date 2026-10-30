# Selective 16-to-8 coding leaves

Experimental internal plans 4096..11095. One 32x32 quadrant uses four 16x16 cells,
with a mask from 1 through 14 selecting cells split into four 8x8 leaves. Other
quadrants retain existing five-shape geometry. All wire partitions use existing
NONE/HORZ/VERT syntax. Max 61 leaves, no sub-8 chroma ownership change, no TX split.

The complete old tool/RDOQ route is retained. Fifty-six independent basic
proposals are evaluated from the same SB entry; deterministic top four each run
one and two complete tool/RDOQ rounds. Outer equal-cost ties keep old output.
This is SB objective monotonicity only, not a whole-frame quality/size guarantee.

Default M6_FINE16=0. Test controls retain-old and reverse-choice are independent.
This source preparation has not built, tested, or measured coding gain.
