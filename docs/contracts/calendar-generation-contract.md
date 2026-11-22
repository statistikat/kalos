# Calendar design generation mirror (21-I.1)

Unit-qualified transcription of the LIVE pinned jdplus 3.9.0 regression-design
GENERATION layer — the machines `Regression.matrix(variable, domain)` drives
for every calendar variable the 23-A builder registers (register D10 deferred
exactly this algebra): `GenericTradingDaysFactory` (tdCount, meanDays, the
day-clustering contrasts, raw), `LPFactory` (None/LeapYear/LengthOfPeriod),
`EasterFactory` + `Easter` (computus + corrections), `JulianEasterFactory`,
`RampFactory` and `TsVariablesFactory` (user variables), plus the pinned
name/description conventions. `kalos/calendar_generation.hpp` is the column
layer the 21-I.2/.3 detection batteries will consume; NO detection decisions
live here (per decision I-5 the {none, 1, 8, 15} candidate batteries stay in
21-I.2/.3).

Oracle: `validation/evidence/native/calendar-generation/{gen.out,gen2.out}`
(4128 + 5904 lines, each byte-identical across two OpenJDK 21 JVM processes;
shas in `metadata.R`). Every machine was replayed bit-exact numerically first
(9632 matrix rows in total) before this API was written; the units below then
qualify the C++ mirror itself at 18383 checks (golden) / 18305 checks (raw
sweep), both lanes 0 failures.

```cpp
kalos::calendar_domain dom{1990, 1, 12, 108};       // year, month, freq, n
auto counts = kalos::td_count(dom);                 // 7 weekday rows x n
auto cols   = kalos::trading_days_contrast(
                  kalos::day_clustering::td2, dom); // raw() is bit-identical
auto ly     = kalos::length_of_period_column(kalos::calendar_lop::leap_year, dom);
auto eas    = kalos::easter_column(8, kalos::easter_correction::simple, 0, dom);
auto jul    = kalos::julian_easter_column(15, true, dom);
auto rmp    = kalos::ramp_column(1992, 3, 1994, 6, dom);
auto usr    = kalos::user_column(data, 1992, 1, dom);
```

## Machines (pinned decodings, byte-level from the captures)

- `td_count` — per-cell weekday counts (Mon..Sun); a quarterly cell counts
  every day whose date falls inside its span.
- `mean_days` — `GenericTradingDaysFactory.meanDays`: the pinned
  `MDAYS = [31, 28.25, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]` cycled from
  the domain start (monthly) or summed over each cell's member months
  (quarterly/annual), divided by 7 at the END (one division per cell).
  `mean_days_yearly(12)` is the constant 12-vector.
- Day-clustering contrasts — group definitions pinned per clustering
  (Mon..Sun → group id); reference group = lowest id; per cell
  `x = sum_ref / |ref|` (division FIRST), column `g` = `sum_g − |g|·x` for
  non-reference group ids ascending. `GenericTradingDays.raw(cluster)` and
  `.contrasts(cluster)` produce BIT-IDENTICAL columns: the factory always
  runs the contrast machine and `GenericTradingDaysVariable` discards the
  contrast flag (no dataNoContrast path exists on this route); the unit
  replays both captures through the one machine.
- `length_of_period_column` — None: the zero column (dim stays 1).
  LeapYear: the February cell (monthly; first-period cell at lower
  frequencies) strikes −0.25 every period; the first leap year's cell and
  every 4TH period from it (the pinned 4-year-assumption quirk — century
  non-leap years are NOT skipped) strike 0.75; the search for the first leap
  year is bounds-free because `TsDomain.get()` is pure arithmetic (a leapless
  range still completes with only the −0.25 cells — `ly_M3` pins it).
  LengthOfPeriod: `daysCount(cell) − 365.25/frequency` — the CONSTANT
  normalizer, not a domain mean.
- `easter_column` — the Butcher computus (`Easter.easter`, pinned ==
  `easter2` 1985..2015); per year the month-pair shares are written with SET
  at `pos−1`/`pos` (`pos = (4−month)/(12/f)` from the adjusted domain start);
  `endPosition` is faithful: only −1 (day−1) and +1 (day+1, rolling Mar 31 →
  Apr 1) are special, any other value behaves as 0; corrections:
  None → (0,0)-style zero shares, Simple (the pinned default) →
  (±0.5)/(dur−day)/dur−0.5 branches, PreComputed → `MEANS_MAR/MEANS_APR
  [25−dur]`, Theoretical → `m25 = (15+dur)/59.06118, m27 = 1−m25`.
- `julian_easter_column` — window `[e−dur, e−1]` split over March/April/May
  with `x19` measured to the EXCLUSIVE March end (April 1), `x20` clamped to
  `[0, dur]`; per-month contribution `x/dur − C_*2[dur−1]/(1064·dur)`
  ACCUMULATED (+=): on a quarterly domain the March and April contributions
  land on the same cell and add. `gregorianDates` converts the Julian date
  through the jar JDN pair (`julianDate2JDN` + Fliegel–Van Flandern
  `JDN2GregorianDate`).
- `ramp_column` — endpoints truncated to domain periods through the
  `TsPeriod.of` position arithmetic (`i1`, `i2`); column = −1 on `[0, i1]`,
  linear `(i − i1)/(i2 − i1) − 1` on `(i1, min(i2, n))` (division then
  subtract), 0 on `[i2, n)`; **`i2 <= i1` (the "reversed" ramp) is the ALL
  ZERO column**, not a mirrored staircase — pinned correction to the 21-I.0
  note; a start at/after the last cell gives all −1.
- `user_column` — absolute calendar placement: with the data's position
  `p` relative to the domain, `[from, to)` receives `data[pos, end)` through
  the pinned overlap arithmetic (out-of-domain cells keep 0.0; a
  non-overlapping range gives the zero column — decision I-9).
- Computus/probabilities — `easter_date`, `julian_easter_date`,
  `prob_julian_easter` (`JD[43]/1064`, 0 outside `[0, 43)`), `prob_easter`
  (0 outside `[0, 35)`; `d < 6` ramp `(d+1)/206.71413`, `6..27` constant
  `0.03386319067786996`, `28..34` tail `(35−d+0.53059)/206.71413`).
- Names — `td`/`lp` consts, `defaultName(k) = "td"`, `nameOf` =
  `td2/td2c/tduser/…` (the TD2d CLUSTERING carries tag `TDuser`),
  `easter [d]`, `julian easter [d]`, `Length of period` / `Leap year`,
  `ramp 1992-03 / 1994-06`, the `TradingDaysType` nvar/names/contrastNames/
  description table (TD2d prints `names=null, contrastNames=[mon-thu]`;
  TDuser prints nvar −1 and nulls).

## Envelope (fail-closed `std::invalid_argument`, never a silent zero)

| call | refused | why |
| --- | --- | --- |
| any domain | frequency ∉ {12, 4, 1}, unaligned month, length ≤ 0, year ∉ [1, 9999] | outside the captured corpus; positions are month/ratio floor divisions |
| `td_count`/`trading_days_contrast` | frequency 1 | the jar's `fill` silently leaves the column empty (`annualFrequency <= 3`) — refused instead of emitting unprobed zeros |
| `easter_column`/`julian_easter_column` | frequency 1; duration ∉ [1, 28]; PreComputed with duration > 25 | jar gates (`EasterFactory.fill` 12/4 only; `JulianEasterFactory` indexes 28-entry cycle tables and AIOOBEs beyond; PreComputed indexes 25-entry means) |
| `length_of_period_column`/`ramp_column`/`user_column` | — (all of {12, 4, 1} captured) | user data month must align with the domain frequency (mixed-unit alignment arithmetic unprobed) |
| `mean_days_yearly` | frequency ≠ 12 | only the 12-vector was captured |
| `ramp_description` | frequency ≠ 12 | the captured format is monthly precision |
| `trading_days_default_name` | k ∉ [1, 8] | captured range |

Holiday-effect families beyond this live generation path keep their 23-A
refusals; jar bugs are NOT ported (e.g. `DayClustering.toString`'s AIOOBE).

## Qualification

`test_calendar_generation` (#75) replays the full 137-case golden — every
matrix value as its captured IEEE-754 bit pattern (17916 values), the 31-year
computus rows, both probability tables incl. the out-of-range guards, the
name/description conventions, the raw/contrast and default/Simple twin
identities, and 15 envelope refusals — at 18383 checks.
`tools/verify_calendar_generation.cpp` re-drives the RAW captures directly
(9632 row lines, 123 cases, 18305 checks, plus the same envelope refusals),
shim-invariant (no JVM numerics beyond `strtod` round-trips). The probe-side
`tdc` month labels in gen.out print period labels, not weekdays — display
only; the 7 rows are the weekday vectors (verified against `td_count`).
