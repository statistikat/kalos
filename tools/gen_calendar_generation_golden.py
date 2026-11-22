#!/usr/bin/env python3
"""Generate tests/jvm_calendar_generation.h from the pinned 21-I.1 calendar
generation captures (validation/evidence/native/calendar-generation/
{gen.out,gen2.out}). Every matrix value is embedded as the IEEE-754 bit
pattern of the captured %.17g decimal (the shortest-round-trip decimal and
the C printf %#.17g of the same double agree, but the decimal is not
recomputed anywhere: bits pass through from the capture). The case table
mirrors tools/verify_calendar_generation.cpp so the unit test and the raw
sweep replay exactly the same corpus."""

import struct
import sys

EV = 'validation/evidence/native/calendar-generation'
OUT = 'tests/jvm_calendar_generation.h'


def bits(x):
    return struct.unpack('<Q', struct.pack('<d', x))[0]


# ------------------------------------------------------------------ parse --

rows = {}
means = {}
tdc = {}
comp = []
pjul = {}
prob = {}
kv = {}


def bracket(line):
    s = line[line.index('[') + 1:line.rindex(']')]
    return [float(x) for x in s.replace(',', ' ').split()]


def load(path):
    for line in open(path):
        t = line.split()
        if not t:
            continue
        line = line.rstrip('\n')
        if t[0] == 'row':
            v = [float(x) for x in t[3:]]
            fam = rows.setdefault(t[1], [])
            i = int(t[2])
            while len(fam) <= i:
                fam.append(None)
            fam[i] = v
        elif t[0] == 'mean12':
            means['12'] = bracket(line)
        elif t[0] == 'mean':
            means[t[1] if not t[1].startswith('[') else ''] = bracket(line)
        elif t[0] == 'tdc':
            key = 'tdc' if '/' in t[1] else 'tdc ' + t[1]
            tdc.setdefault(key, []).append([float(x) for x in bracket(line)])
        elif t[0] == 'eas':
            md = lambda s: int(s[-5:-3]) * 100 + int(s[-2:])  # noqa: E731
            comp.append((int(t[1]), md(t[2]), md(t[4]), md(t[5]),
                         float(t[7]), float(t[9])))
        elif t[0] == 'pjul':
            pjul[int(t[1])] = float(t[2])
        elif t[0] == 'prob':
            prob[int(t[1])] = float(t[2])
        else:
            kv[line] = line


load(EV + '/gen.out')
load(EV + '/gen2.out')

# ------------------------------------------------------------- case table --
# kind codes (shared with the test):
K_TDC, K_MEAN, K_CONTRAST, K_LOP, K_EASTER, K_JULIAN, K_RAMP, K_USER, K_MEAN12 = range(9)

DOMAINS = [
    (1990, 1, 12, 108),   # 0  A
    (1989, 4, 12, 90),    # 1  M1
    (1990, 3, 12, 48),    # 2  M2
    (1990, 1, 4, 28),     # 3  Q1
    (1991, 4, 4, 30),     # 4  Q2
    (1990, 1, 1, 20),     # 5  Y1
    (1991, 2, 12, 11),    # 6  M3
    (2006, 1, 12, 36),    # 7  D
    (1990, 1, 4, 36),     # 8  Q
    (1992, 1, 12, 48),    # 9  ua_S domain
    (1988, 1, 12, 40),    # 10 ua_PRE
    (1997, 6, 12, 40),    # 11 ua_POST
    (1985, 1, 12, 24),    # 12 ua_BEF
    (1999, 1, 12, 24),    # 13 ua_AFT
    (1989, 1, 12, 200),   # 14 ua_COV
    (1991, 1, 4, 10),     # 15 ua_Q
    (1993, 1, 1, 10),     # 16 ua_Y
]

TAGS = ['TD2', 'TD2c', 'TDuser', 'TD3', 'TD3c', 'TD4', 'TD4c', 'TD6', 'TD7']
# CL index per tag row above
cases = []            # (id, dom, kind, p0, p1, p2, p3, udata_name, off, dlen)
vals = []             # flat bit pool


def add_matrix(fid, dom, kind, p0=0, p1=0, p2=0, p3=0, matrix=None, cols=None):
    """matrix: list of rows (default: captured rows). Emit flat bits."""
    m = matrix if matrix is not None else rows[fid]
    w = cols if cols is not None else len(m[0])
    start = len(vals)
    for r in m:
        assert len(r) == w, (fid, len(r), w)
        vals.extend(bits(x) for x in r)
    cases.append((fid, dom, kind, p0, p1, p2, p3, '', 0, 0, start, len(m), w))


def add_tdc(tag, dom):
    m = tdc[tag]
    add_matrix(tag, dom, K_TDC, matrix=m)


def add_mean(tag, dom):
    m = [means[tag]]
    add_matrix('mean ' + tag, dom, K_MEAN, matrix=m, cols=len(m[0]))


# ---- gen.out primary corpus ----
add_tdc('tdc', 0)
for tg in TAGS:
    add_matrix('c_' + tg, 0, K_CONTRAST, TAGS.index(tg))
    # the raw() twin of every clustering: replayed through the SAME contrast
    # machine, pinning the bit-identity of raw() and contrasts() columns.
    add_matrix('r_' + tg, 0, K_CONTRAST, TAGS.index(tg))
add_matrix('q_td2', 8, K_CONTRAST, 0)
add_matrix('lop_LeapYear', 0, K_LOP, 1)
add_matrix('lop_LengthOfPeriod', 0, K_LOP, 2)
for d in (1, 5, 8, 15):
    add_matrix('eas_%d' % d, 0, K_EASTER, d, 1, 0)
    # default-correction twin replayed through the explicit Simple machine,
    # pinning the pinned default Correction == Simple bit for bit.
    add_matrix('easS_%d' % d, 0, K_EASTER, d, 1, 0)
for d in (1, 5, 8, 15):
    add_matrix('jul_%d' % d, 0, K_JULIAN, d, 1)
add_matrix('ramp', 0, K_RAMP, 1992, 3, 1994, 6)
add_matrix('ramp2', 0, K_RAMP, 1994, 6, 1992, 3)
add_matrix('q_eas8', 8, K_EASTER, 8, 1, 0)
add_mean('12', 0)   # meanDays(12) (kind MEAN12 is the constant 12-vector)
cases[-1] = cases[-1][:2] + (K_MEAN12,) + cases[-1][3:]

# ---- user data pool: aligned dumps ARE the clean(n) inputs ----
udata = []           # (name, start, len)
udata_vals = []
DATA = {'user_aligned': ('user_aligned', 'user_aligned', 0, 108),
        'user_short': ('user_aligned', 'user_aligned', 0, 48)}
for tag, dlen in (('S', 48), ('PRE', 40), ('POST', 40), ('BEF', 24),
                  ('AFT', 24), ('COV', 200), ('Q', 10), ('Y', 10)):
    DATA['u_' + tag] = ('ua_' + tag, 'ua_' + tag, 0, dlen)


def add_user(fid, dom, dy, dm, dlen):
    key = fid if fid in DATA else fid.replace('ua_', 'u_', 1)
    src = DATA[key][0]
    off = DATA[key][2]
    if not any(u[0] == src for u in udata):
        col = [r[0] for r in rows[src]]
        udata.append((src, len(udata_vals), len(col)))
        udata_vals.extend(bits(x) for x in col)
    add_matrix(fid, dom, K_USER, dy, dm, dlen)
    cases[-1] = cases[-1][:7] + (src, off, dlen) + cases[-1][10:]


add_user('user_aligned', 0, 1990, 1, 108)
add_user('user_short', 0, 1992, 1, 48)

# ---- gen2.out envelope corpus ----
for tag, dom, freq in (('A', 0, 12), ('M1', 1, 12), ('M2', 2, 12),
                       ('Q1', 3, 4), ('Q2', 4, 4), ('Y1', 5, 1)):
    add_mean(tag, dom)
    if freq > 3:
        add_tdc('tdc ' + tag, dom)
        for pfx, tg in (('c2_', 'TD2'), ('c7_', 'TD7'), ('cu_', 'TDuser'),
                        ('c3_', 'TD3c')):
            add_matrix(pfx + tag, dom, K_CONTRAST, TAGS.index(tg))
        add_matrix('eas_' + tag, dom, K_EASTER, 8, 1, 0)
        add_matrix('jul_' + tag, dom, K_JULIAN, 8, 1)
    add_matrix('ly_' + tag, dom, K_LOP, 1)
    add_matrix('lp_' + tag, dom, K_LOP, 2)
    add_matrix('rmp_' + tag, dom, K_RAMP, 1992, 3, 1994, 6)
add_matrix('ly_M3', 6, K_LOP, 1)
add_matrix('lp_M3', 6, K_LOP, 2)
for fid, dur, corr, end, dom in (('e13n', 13, 0, 0, 0), ('e28s', 28, 1, 0, 0),
                                 ('e10p', 10, 2, 0, 0), ('e25p', 25, 2, 0, 0),
                                 ('e20t', 20, 3, 0, 0), ('e8pm', 8, 1, 1, 0),
                                 ('e8nm', 8, 1, -1, 0), ('e1pm', 1, 0, 1, 0),
                                 ('e15nm', 15, 1, -1, 0),
                                 ('e8nm_D', 8, 1, -1, 7)):
    add_matrix(fid, dom, K_EASTER, dur, corr, end)
for fid, dur, greg, dom in (('jul15f', 15, 0, 0), ('jul28', 28, 1, 0),
                            ('jul1f', 1, 0, 0), ('jul8_Q1', 8, 1, 3),
                            ('jul15_Q2', 15, 1, 4)):
    add_matrix(fid, dom, K_JULIAN, dur, greg)
UA_DOM = {'S': 9, 'PRE': 10, 'POST': 11, 'BEF': 12, 'AFT': 13, 'COV': 14,
          'Q': 15, 'Y': 16}
U_DOM = {'S': 0, 'PRE': 0, 'POST': 0, 'BEF': 0, 'AFT': 0, 'COV': 0, 'Q': 3,
         'Y': 5}
U_START = {'S': (1992, 1), 'PRE': (1988, 1), 'POST': (1997, 6), 'BEF': (1985, 1),
           'AFT': (1999, 1), 'COV': (1989, 1), 'Q': (1991, 1), 'Y': (1993, 1)}
U_LEN = {'S': 48, 'PRE': 40, 'POST': 40, 'BEF': 24, 'AFT': 24, 'COV': 200,
         'Q': 10, 'Y': 10}
for tag in U_LEN:
    dy, dm = U_START[tag]
    add_user('ua_' + tag, UA_DOM[tag], dy, dm, U_LEN[tag])
    add_user('u_' + tag, U_DOM[tag], dy, dm, U_LEN[tag])
add_matrix('rmpBEF', 0, K_RAMP, 1985, 1, 1986, 1)
add_matrix('rmpAFT', 0, K_RAMP, 2000, 1, 2001, 1)
add_matrix('rmpEQ', 0, K_RAMP, 1992, 3, 1992, 3)
add_matrix('rmpQ', 3, K_RAMP, 1992, 3, 1994, 6)
add_matrix('rmpY', 5, K_RAMP, 1993, 1, 1997, 1)

# ------------------------------------------------------------- name table --
def kvs(needle):
    for l in sorted(kv):
        if needle in l:
            return l
    raise KeyError(needle)


NAMEOF = [(tg, kvs('nameOf %s = ' % tg).split(' = ')[1]) for tg in TAGS]
TDT = []
for l in sorted(kv):
    if l.startswith('tdt '):
        head = l.split(' ', 3)
        nvar = int(head[2][len('nvar='):])
        rest = head[3]  # "names=... contrastNames=... desc=..."
        names = rest[len('names='):rest.index(' contrastNames=')]
        cnames = rest[rest.index('contrastNames=') + len('contrastNames='):
                      rest.rindex(' desc=')]
        desc = rest[rest.rindex(' desc=') + len(' desc='):]
        TDT.append((head[1], nvar, names, cnames, desc))
GTD = [(tg, kvs('gtd %s ' % tg)) for tg in TAGS]
DESC_LINES = {
    'td_name': 'const ITradingDaysVariable.NAME=td',
    'lp_name': 'const ILengthOfPeriodVariable.NAME=lp',
    'q_desc': kvs('q desc='),
    'uv_desc': kvs('uv dim='),
    'user_aligned_desc': kvs('user_aligned desc='),
    'ramp_desc': kvs('ramp dim='),
    'ramp2_desc': kvs('ramp2 desc='),
    'lop_none': kvs('lop None '),
    'lop_leap': kvs('lop LeapYear '),
    'lop_period': kvs('lop LengthOfPeriod '),
}
EAS_DESC = {d: kvs('julas JulianEasterVariable(duration=%d, ' % d).split('desc=')[1]
            for d in (1, 5, 8, 15)}
VDESC = {}
for l in sorted(kv):
    if l.startswith('vdesc '):
        t = l.split()
        VDESC[t[1]] = (t[2].split('=')[1], int(t[3].split('=')[1]),
                       l.split(' desc=')[1])


def cstr(s):
    assert all(32 <= ord(c) < 127 for c in s), s
    return '"%s"' % s.replace('\\', '\\\\').replace('"', '\\"')


# ------------------------------------------------------------------ emit ---
out = []
w = out.append
w('// GENERATED by tools/gen_calendar_generation_golden.py - do not edit.')
w('// Item 21-I.1 calendar-generation golden: the pinned jdplus 3.9.0 design')
w('// matrices (validation/evidence/native/calendar-generation/{gen.out,')
w('// gen2.out}, byte-identical double JVM captures) as IEEE-754 bit patterns,')
w('// plus the scalar computus/probability tables and the name/description')
w('// conventions. Values pass through from the capture; nothing recomputes.')
w('#ifndef KALOS_TESTS_JVM_CALENDAR_GENERATION_H')
w('#define KALOS_TESTS_JVM_CALENDAR_GENERATION_H')
w('#include <cstdint>')
w('namespace kalos_golden {')
w('enum CaseKind { K_TDC = 0, K_MEAN = 1, K_CONTRAST = 2, K_LOP = 3,')
w('                K_EASTER = 4, K_JULIAN = 5, K_RAMP = 6, K_USER = 7,')
w('                K_MEAN12 = 8 };')
w('// {year, month, frequency, length}')
w('inline const int DOMAINS[17][4] = {')
for d in DOMAINS:
    w('    {%d, %d, %d, %d},' % d)
w('};')
w('inline const uint64_t VALS[] = {')
for i in range(0, len(vals), 8):
    w('    ' + ', '.join('0x%016xULL' % v for v in vals[i:i + 8]) + ',')
w('};')
w('struct Case {')
w('    const char* id; int dom, kind, p0, p1, p2, p3;')
w('    const char* udata; int uoff, ulen;   // K_USER input data')
w('    int start, rows, cols;               // into VALS')
w('};')
w('inline const Case CASES[] = {')
for (fid, dom, kind, p0, p1, p2, p3, udn, off, dlen, start, nr, nc) in cases:
    w('    {%s, %d, %d, %d, %d, %d, %d, %s, %d, %d, %d, %d, %d},' %
      (cstr(fid), dom, kind, p0, p1, p2, p3, cstr(udn), off, dlen, start, nr, nc))
w('};')
w('inline const int NCASES = %d;' % len(cases))
w('inline const uint64_t UDATA[] = {')
for i in range(0, len(udata_vals), 8):
    w('    ' + ', '.join('0x%016xULL' % v for v in udata_vals[i:i + 8]) + ',')
w('};')
w('inline const struct { const char* name; int start, len; } UDATA_TAB[] = {')
for (nm, st, ln) in udata:
    w('    {%s, %d, %d},' % (cstr(nm), st, ln))
w('};')
w('inline const int NUDATA = %d;' % len(udata))
w('// {year, easter(MM*100+DD), julian(true), julian(false)}, prob bits pair')
w('inline const struct { int y, e, jt, jf; uint64_t pjul, prob; } COMP[] = {')
for (y, e, jt, jf, pj, pb) in comp:
    w('    {%d, %d, %d, %d, 0x%016xULL, 0x%016xULL},' % (y, e, jt, jf, bits(pj), bits(pb)))
w('};')
w('inline const int NCOMP = %d;' % len(comp))
w('inline const struct { int d; uint64_t v; } PJUL[] = {')
for d in sorted(pjul):
    w('    {%d, 0x%016xULL},' % (d, bits(pjul[d])))
w('};')
w('inline const int NPJUL = %d;' % len(pjul))
w('inline const struct { int d; uint64_t v; } PROB[] = {')
for d in sorted(prob):
    w('    {%d, 0x%016xULL},' % (d, bits(prob[d])))
w('};')
w('inline const int NPROB = %d;' % len(prob))
w('inline const struct { const char* tag; const char* name; } NAMEOF[] = {')
for tg, nm in NAMEOF:
    w('    {%s, %s},' % (cstr(tg), cstr(nm)))
w('};')
w('inline const struct { const char* tag; int nvar; const char* names,')
w('    *contrast_names, *desc; } TDT[] = {')
for (tg, nv, ns, cs, ds) in TDT:
    w('    {%s, %d, %s, %s, %s},' % (cstr(tg), nv, cstr(ns), cstr(cs), cstr(ds)))
w('};')
w('inline const struct { const char* tag; const char* line; } GTD[] = {')
for tg, l in GTD:
    w('    {%s, %s},' % (cstr(tg), cstr(l)))
w('};')
w('inline const struct { const char* julian_desc; } EAS_DESC[4] = {')
for d in (1, 5, 8, 15):
    w('    {%s},' % cstr(EAS_DESC[d]))
w('};')
w('inline const int EAS_DUR[4] = {1, 5, 8, 15};')
w('inline const struct { const char* corr; int end, dur; const char* desc; }'
  ' VDESC[] = {')
VDESC_DUR = {'e13n': 13, 'e28s': 28, 'e10p': 10, 'e25p': 25, 'e20t': 20,
             'e8pm': 8, 'e8nm': 8, 'e1pm': 1, 'e15nm': 15}
for fam in ('e13n', 'e28s', 'e10p', 'e25p', 'e20t', 'e8pm', 'e8nm', 'e1pm', 'e15nm'):
    corr, end, desc = VDESC[fam]
    w('    {%s, %d, %d, %s},' % (cstr(corr), end, VDESC_DUR[fam], cstr(desc)))
w('};')
w('inline const char* DESC_TD_NAME = %s;' % cstr(DESC_LINES['td_name']))
w('inline const char* DESC_LP_NAME = %s;' % cstr(DESC_LINES['lp_name']))
w('inline const char* DESC_Q = %s;' % cstr(DESC_LINES['q_desc']))
w('inline const char* DESC_UV = %s;' % cstr(DESC_LINES['uv_desc']))
w('inline const char* DESC_USER_ALIGNED = %s;' % cstr(DESC_LINES['user_aligned_desc']))
w('inline const char* DESC_RAMP = %s;' % cstr(DESC_LINES['ramp_desc']))
w('inline const char* DESC_RAMP2 = %s;' % cstr(DESC_LINES['ramp2_desc']))
w('inline const char* DESC_LOP_NONE = %s;' % cstr(DESC_LINES['lop_none']))
w('inline const char* DESC_LOP_LEAP = %s;' % cstr(DESC_LINES['lop_leap']))
w('inline const char* DESC_LOP_PERIOD = %s;' % cstr(DESC_LINES['lop_period']))
w('inline const int NVALL = %d;' % len(vals))
w('}  // namespace kalos_golden')
w('#endif')

open(OUT, 'w').write('\n'.join(out) + '\n')
print('%s: %d cases, %d values, %d udata, %d computus, %d+%d prob, %d tdt' %
      (OUT, len(cases), len(vals), len(udata), len(comp), len(pjul), len(prob),
       len(TDT)))
