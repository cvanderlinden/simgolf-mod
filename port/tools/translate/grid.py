#!/usr/bin/env python3
"""Map-size pass over the translated game code (task-7 prototype, task-8).

golf.exe keeps every per-tile array at a fixed address, 50 x 50 tiles
(2500 entries, row stride 50; corner heights 51 x 51). This pass moves
those arrays to storage allocated at start-up (GRID(i), port/src/port/grid.c)
and turns the 50-tile row stride and bounds into MAP_W / MAP_H, so the grid
size becomes a run-time setting. With MAP_W = MAP_H = 50 the game behaves
exactly as the original.

Rules (only in functions that touch a grid array, plus EXTRA_FUNCS):
- symbols and literals inside an array's range become GRID(i) + offset;
  symbols just outside it that are used as arrays (row/column neighbours,
  e.g. DAT_005722b6 = row -1) are re-encoded as (dy, dx) with the new stride;
- `* 0x32`, `/ 0x32`, `% 0x32` -> MAP_W; `* 0x33` -> MAP_W + 1 (corners);
- comparisons and +/- with 0x30..0x33 -> MAP_W - 2 .. MAP_W + 1;
- 0x9c4 (2500) -> MAP_W * MAP_H, 0xa29 (2601) -> corners count.
SKIP_LINES / SKIP_FUNCS keep UI and unrelated uses of the same numbers.

Usage: grid.py <gen/golf dir> [report.txt]   (rewrites golf_*.c in place)
"""
import re, sys, glob, os

# (base address, bytes per entry, kind) ; kind "tile" = W x H, "corner" = (W+1) x (H+1)
GRIDS = [
    (0x5722e8, 1, "tile"),   # tile type (saved)
    (0x56988c, 1, "tile"),   # object/building index per tile (saved)
    (0x53bbac, 1, "tile"),   # (saved)
    (0x5a6378, 1, "tile"),   # (saved)
    (0x56c7e4, 1, "tile"),   # (saved)
    (0x53ea24, 1, "tile"),   # (saved)
    (0x542414, 1, "tile"),   # pre-purchase tile types (saved)
    (0x5830b8, 1, "tile"),   # pending work target (saved)
    (0x59c090, 1, "tile"),   # pending work cost (saved)
    (0x53caf0, 2, "tile"),   # tile flags, short (saved)
    (0x5a4998, 1, "corner"), # corner heights 51 x 51 (saved)
    (0x543018, 1, "tile"),
    (0x5619a0, 1, "tile"),
    (0x578804, 1, "tile"),
    (0x55eb40, 2, "tile"),
    (0x55fec8, 2, "tile"),
    (0x51b770, 8, "tile"),
    (0x822da0, 1, "tile"),   # shade map A
    (0x82376c, 1, "tile"),   # shade map B
    (0x58d370, 1, "tile"),   # path search distances (FUN_0042e7e0)
]

# functions without a grid array that still use tile bounds
EXTRA_FUNCS = {"FUN_00449470", "FUN_0044a5b0"}
# loops that start at the last row/column (`i = 0x31; ... i--`)
# (and every `n = 0x32; do {` row loop, see rewrite_function)
ASSIGN_FUNCS = {"FUN_00449470",   # syncTiles: game tiles -> Terrain
                "FUN_00449540",   # rebuildTerrainGrid
                "FUN_00470a60"}   # land generator (coast, island ring)
SKIP_FUNCS = set()
# lines whose 0x30..0x33 is not the map: dialogue slots and story names
# (50-byte records), 0x81b490 records, home-site price / 50, sprite frame
# 50 + n, event id 50, a random % 48, hole length < 51, golfer stat 50,
# a distance under 50 tiles
SKIP_LINE_RE = re.compile(
    r"DAT_00543d1[0-9a-f]|0x543d1|DAT_0053a45|0x53a45|DAT_004ba6|DAT_0081b[45]"
    r"|DAT_0058bcc0\)\[[^\]]*\] / 0x32|& 3U\) \+ 0x32|& 0x7f\) == 0x32"
    r"|FUN_0042ef40\([^)]*\),\s*iVar23 < 0x32|local_b0c|DAT_00834170 % 0x30"
    r"|if \(param_2 != 0x32\)|if \(iVar(?:9|23) < 0x33\)|\*piVar35 = 0x9c4|DAT_004c2fa0 = ")
# Parts of a line whose 50s are not the map, kept while the rest of the line
# (often a tile index on the same line) is rewritten (task-8 audit):
# sub-tile positions (>> 4, 0..63) against 48, the yardage loop in steps of
# 50, money / 50 (a percentage), a 49-yard threshold, golfer stat noise and
# shot power / 50, golfer screen pixels, a snapshot pixel offset.
KEEP_RE = re.compile(
    r"\(int\)uVar26 >> 4 < 0x30|\(int\)uStack_121c < 0x30|\(int\)puStack_1224 \+ 0x32\)"
    r"|DAT_00571fd4 / 0x32|0x30 < \(int\)uVar(?:12|7) >> 4|0x31 < iVar28 && \(3 < "
    r"|0xfc71c71d\) / 0x32|\(uVar9 \* 0x32\) / 3|0x32 < \(int\)\(&DAT_005794c[04]\)"
    r"|local_640 % 0x32")


def n_side(kind):
    return 51 if kind == "corner" else 50


def size(g):
    b, e, k = g
    return n_side(k) * n_side(k) * e


def core(a):
    for i, g in enumerate(GRIDS):
        if g[0] <= a < g[0] + size(g):
            return i
    return None


def near(a):
    """grid whose row -1 window (just before the array) holds a: the
    decompiler names (y-1, x) and (y, x-1) neighbours by their own address.
    Past the end there are other arrays (sprites), never neighbours."""
    for i, g in enumerate(GRIDS):
        row = n_side(g[2]) * g[1]
        if g[0] - row - 2 * g[1] <= a < g[0]:
            return i
    return None


def expr(i, a):
    b, e, k = GRIDS[i]
    n = n_side(k)
    w = "(MAP_W+1)" if k == "corner" else "MAP_W"
    h = "(MAP_H+1)" if k == "corner" else "MAP_H"
    row = n * e
    o = a - b
    dy = (o + row // 2) // row
    rem = o - dy * row
    if dy > n // 2:   # rows near the end stay relative to the end
        dys = "(%s%+d)" % (h, dy - n)
    else:
        dys = "%d" % dy
    s = "GRID(%d)" % i
    if dy:
        s += "+%s*%s*%d" % (dys, w, e)
    if rem:
        s += "%+d" % rem
    return "(%s)" % s


# a loop bound up to this many bytes past an array's end still belongs to it
END_SLACK = 8

CONST = {"0x30": "(MAP_W-2)", "0x31": "(MAP_W-1)", "0x32": "MAP_W", "0x33": "(MAP_W+1)"}


# Functions where 0x30..0x33 are pixels (sprite cutting): only the array
# sizes and clear-loop counts change there.
COUNTS_ONLY_FUNCS = {"FUN_00442180"}   # loadInterfaceArt


def rewrite_counts(line):
    # rep stosd clears: dword counts of 2500, 5000, 2601 (+1 byte) and 20000 bytes
    line = re.sub(r"(for \(\w+ = )0x271;", r"\1(MAP_W*MAP_H/4);", line)
    line = re.sub(r"(for \(\w+ = )0x4e2;", r"\1(MAP_W*MAP_H/2);", line)
    line = re.sub(r"(for \(\w+ = )0x28a;", r"\1((MAP_W+1)*(MAP_H+1)/4);", line)
    line = re.sub(r"(for \(\w+ = )5000;", r"\1(MAP_W*MAP_H*2);", line)
    # save/load field sizes
    line = re.sub(r"(FUN_0040af70\(\(int\)\(GRID[^;]*,)5000\)", r"\1(MAP_W*MAP_H*2))", line)
    return line


def rewrite_consts(line, counts_only=False):
    if SKIP_LINE_RE.search(line):
        return line
    line = rewrite_counts(line)
    if counts_only:
        return line
    kept = []

    def stash(m):
        kept.append(m.group(0))
        return "\0%d\0" % (len(kept) - 1)
    line = KEEP_RE.sub(stash, line)
    line = re.sub(r"\*\s*0x32\b(?!\s*\+\s*0x)", "* MAP_W", line)
    line = re.sub(r"\b0x32\s*\*", "MAP_W *", line)
    line = re.sub(r"\*\s*0x33\b", "* (MAP_W+1)", line)
    line = re.sub(r"([/%])\s*0x32\b", r"\1 MAP_W", line)
    line = re.sub(r"([<>]=?|[!=]=)\s*(0x3[0-3])\b", lambda m: "%s %s" % (m.group(1), CONST[m.group(2)]), line)
    line = re.sub(r"\b(0x3[0-3])\s*([<>]=?|[!=]=)", lambda m: "%s %s" % (CONST[m.group(1)], m.group(2)), line)
    line = re.sub(r"([+-])\s*(-?)(0x3[123])\b(?!\s*[*\]])", lambda m: "%s %s%s" % (m.group(1), m.group(2), CONST[m.group(3)]), line)
    line = re.sub(r"\b0x9c4\b", "(MAP_W*MAP_H)", line)
    line = re.sub(r"\b0xa29\b", "((MAP_W+1)*(MAP_H+1))", line)
    line = re.sub(r"(FUN_00467130\([^;]*,\s*0,\s*)0x31\)", r"\1(MAP_W-1))", line)
    return re.sub(r"\0(\d+)\0", lambda m: kept[int(m.group(1))], line)


SYM = re.compile(r"\b((?:_?(?:PTR_)?(?:DAT|BYTE|WORD|DWORD|PTR))_00([0-9a-f]{6}))\b")
LIT = re.compile(r"\b0x([4-8][0-9a-f]{5})\b")
DEF = re.compile(r"^(#define (\S+) .*)0x([0-9a-f]+)\)$")


def lit_sub(m):
    """a literal address inside a grid array. As the bound of a loop
    (`p < 0x...`) the array's start is another array's end and stays, and
    the array's end (just outside it) moves with the array."""
    a = int(m.group(1), 16)
    before = m.string[max(0, m.start() - 6):m.start()]
    if re.search(r"(<=?|!=)\s*(\(int\))?\s*$", before):
        for i, g in enumerate(GRIDS):
            if a == g[0]:
                return m.group(0)
            # the end, or a few bytes past it: a pointer that starts at an
            # offset (FUN_0042f7a0 walks byte 3 of 8-byte tiles, the land
            # generator's corner pass runs one row longer than the tiles)
            if g[0] + size(g) <= a <= g[0] + size(g) + END_SLACK:
                w = "(MAP_W+1)" if g[2] == "corner" else "MAP_W"
                h = "(MAP_H+1)" if g[2] == "corner" else "MAP_H"
                extra = a - g[0] - size(g)
                return "(int)(GRID(%d)+%s*%s*%d%s)" % (i, h, w, g[1], "+%d" % extra if extra else "")
    i = core(a)
    return "(int)%s" % expr(i, a) if i is not None else m.group(0)


def is_grid_func(body):
    for m in SYM.finditer(body):
        if core(int(m.group(2), 16)) is not None:
            return True
    for m in LIT.finditer(body):
        if core(int(m.group(1), 16)) is not None:
            return True
    return False


def array_use(name, body):
    """symbol used as an array or address (neighbour offsets), not a scalar"""
    return re.search(r"\(&%s\)\s*\[|&%s\s*\+" % (name, name), body) is not None


def rewrite_function(name, text, report):
    lines = text.split("\n")
    body = "\n".join(l for l in lines if not l.startswith(("#define", "#undef")))
    grid_fn = is_grid_func(body)
    if not grid_fn and name not in EXTRA_FUNCS:
        return text
    if name in SKIP_FUNCS:
        return text
    out = []
    for k, l in enumerate(lines):
        if l.startswith("#define"):
            m = DEF.match(l)
            if m:
                a = int(m.group(3), 16)
                i = core(a)
                if i is None:
                    j = near(a)
                    if j is not None and array_use(m.group(2), body):
                        i = j
                        report.append("%s neighbour %s -> grid %06x" % (name, m.group(2), GRIDS[j][0]))
                if i is not None:
                    l = "%s%s)" % (m.group(1), expr(i, a))
            out.append(l)
            continue
        if l.startswith("#undef"):
            out.append(l)
            continue
        new = LIT.sub(lit_sub, l)
        new = rewrite_consts(new, name in COUNTS_ONLY_FUNCS)
        nxt = next((x.strip() for x in lines[k + 1:] if x.strip()), "")
        if (name in ASSIGN_FUNCS or nxt == "do {") and name not in COUNTS_ONLY_FUNCS and not SKIP_LINE_RE.search(new):
            new = re.sub(r"(=\s*)(0x3[0-3]);", lambda m: m.group(1) + CONST[m.group(2)] + ";", new)
        if new != l:
            report.append("%s: %s  =>  %s" % (name, l.strip()[:150], new.strip()[:170]))
        out.append(new)
    return "\n".join(out)


def process_file(path, report):
    t = open(path, encoding="utf-8", errors="surrogateescape").read()
    if "GRID(" in t or "MAP_W" in t:
        return False   # already rewritten (the pass runs once per translation)
    parts = re.split(r"(?m)^(/\* (FUN_[0-9a-f]{8}) @ [0-9a-f]+ \*/)$", t)
    res = [parts[0]]
    for i in range(1, len(parts), 3):
        res.append(parts[i])
        res.append(rewrite_function(parts[i + 1], parts[i + 2], report))
    new = "".join(res)
    if new != t:
        open(path, "w", encoding="utf-8", errors="surrogateescape", newline="\n").write(new)
        return True
    return False


def write_table(gen):
    """golf_grid.c: the relocated arrays' table and storage (port/src/port/golf/grid.c sets them up)"""
    nl = chr(10)
    rows = ("," + nl).join("  {0x%06x, %d, %d}" % (b, e, k == "corner") for (b, e, k) in GRIDS)
    text = nl.join([
        "// Generated by tools/translate/grid.py. Do not edit.",
        "#include <stdint.h>",
        '#include "port/grid.h"',
        "const GridArray grid_arrays[] = {", rows, "};",
        "const int grid_count = %d;" % len(GRIDS),
        "uintptr_t g_grid[%d];" % len(GRIDS), ""])
    open(os.path.join(gen, "golf_grid.c"), "w", newline=nl).write(text)


if __name__ == "__main__":
    gen = sys.argv[1]
    report = []
    changed = [p for p in sorted(glob.glob(os.path.join(gen, "golf_[0-9]*.c"))) if process_file(p, report)]
    write_table(gen)
    rep = sys.argv[2] if len(sys.argv) > 2 else os.path.join(gen, "grid_report.txt")
    open(rep, "w", encoding="utf-8").write("\n".join(report) + "\n")
    print("grid.py: %d files, %d rewrites, report %s" % (len(changed), len(report), rep))
