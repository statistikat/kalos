# Transformation decision — log vs levels (21-B)

`decide_transformation()` is the stage-T log/level decision unit: a literal
transcription of the pinned `jdplus.sa.base.core.regarima.LogLevelModule`
comparison composed onto the qualified [estimation-control](
estimation-control-contract.md) layer. It decides *whether the modelling
continues on the log or the level data* and reports both branch estimations;
it does not mutate the modelling and feeds no fit seam yet. There is no Rcpp
boundary; the unit is C++-only.

```cpp
kalos::TransformationDecisionRequest q;        // branch = a plain 21-A
q.branch.control = {1e-7, 1e-4, true, false,   // request (Multiple, ML),
                    true, true, kalos::EstimationStartingPoint::Multiple};
q.aicc_log_correction = -2.0;                  // required, never a default
auto d = kalos::decide_transformation(q);      // outcome + both branches
```

Everything the pinned module reads from its Builder arrives as required
request data (standing rule of item 21: pinned values stay transcription
constants, never configuration defaults). The decoder-pinned call site
(`jdplus.x13.base.core.x13.regarima.X13SpecDecoder.readTransformation`) uses
`estimationPrecision = 1.0E-7`, `preadjust = LengthOfPeriodType.None`,
`outliersCorrection = false` and the `TransformSpec` default
`DEF_AICDIFF = -2.0`; the preadjust and `outliersCorrection` variants are
outside this slice, so the unit reproduces only the pinned
`DemetraUtility.processor(true, precision)` computer — bytecoded-verified
`iconst_1` before the `processor:(ZD)` call, i.e. maximum likelihood with the
`Multiple` starting point and the builder-default `feps = 1.0E-4`.

## Pinned semantics, step by step

1. *Short-circuit* (`dcmpg(v, 0) <= 0` scan): any strictly non-positive value
   selects levels with **no estimation at all** (`outcome = "nonpositive"`),
   both AICCs at the cleared 0.0 and no branch reports. The scan runs before
   any computer is built, so it also runs before this unit's own request
   validation (a malformed request carrying a zero still decides `nonpositive`).
   NaN passes the scan under `dcmpg` but then takes the pinned missing-data
   staging inside `buildTransformation`; that staging is unqualified, so a
   positive NaN or infinity is an explicit `invalid_argument` right after the
   scan.
2. *Level branch*: the request estimated as-is through
   `estimate_sarima_fixed` with the pinned control.
3. *Log branch*: the pinned `copyOf + setLogTransformation(true)` twin — the
   same request over the `LogTransformation` data rule (`log` on finite
   values; unreachable here because step 1 rejects non-finite inputs) and the
   same design.
4. *Statistics* (`LikelihoodStatistics` bytecode): the level branch carries
   the exact 0.0 Jacobian. The log branch carries the pinned `LogJacobian`
   value, `-sum(log y_t)` accumulated left-to-right over the differenced
   estimation sample `[d + seasonal_d * period, L)` (NaN sanitized to 0.0;
   an adjustment of exactly 0.0 leaves the raw likelihood — level branch).
   `observations = L - (d + seasonal_d * period)` (no missing positions in
   the qualified scope) and `parameters = free ARMA + design columns + 1`
   (the pinned `+ 1` is the innovation-variance count). AICC is
   `-2 * (adjusted - n * k / ((n - k) - 1))` in that exact arithmetic order;
   IEEE division results are never guarded.
5. *Comparison* (`isChoosingLog`, `dcmpl(aiccLevel, aiccLog + correction)`):
   logs are chosen iff `aiccLevel > aiccLog + correction` strictly; equality
   and any NaN select levels (`log_level_choose_log` is exposed for direct
   unit-level use). Module-null rules keep precedence: a missing log branch
   selects levels, a missing level branch selects logs; when both branch
   estimations are unavailable the outcome is `"failed"` and levels are kept
   (the pinned `ProcessingResult.Failed` path). A branch-internal exception
   is exactly that null: it is recorded as an unavailable branch and never
   escapes as a failure of the decision.

The request itself is otherwise a plain 21-A fixed-order request. Fail-closed
scope rejections (explicit `invalid_argument`, never a silent fallback):
starting point other than `Multiple` (the module builds no other computer);
fixed SARIMA block entries (the pinned `estimate()` dispatches fixed
specifications to the unqualified `optimize` path); masked or non-complete
responses; fixed regression design coefficients; and a response not longer
than the differencing warmup `d + seasonal_d * period`.

## Evidence

* Corpus `validation/evidence/native/transformation-decision/` (11 cases):
  oracle = the real `LogLevelModule.process` driven through the public-API
  observer `validation/reference/TransformationDecisionProbe.java` (module,
  `ModelDescription`, `RegSarimaModelling` — no decision rule or estimation
  algorithm is re-implemented there), captured by
  `validation/reference/capture-transformation-decision.R`. The design
  columns travel as the product's own `UserVariable`s and the corpus stores
  the `regarima()` matrix actually consumed (post-`check()`), so the replay
  re-creates identical columns. Byte-identical across two JVM processes
  (double-capture policy for estimation-touching corpora).
* Decision coverage: log decisive, log near-tie (measured gap +0.15), level
  decisive, level past the boundary (gap +2.51), seasonal multiplicative,
  user-variable columns with `d = 0` and `d = 1`, and both short-circuit
  variants (zero, negative).
* `tests/test_transformation_decision.cpp` (generated by
  `validation/reference/generate-transformation-decision-test.py`) pins for
  every case: the decision bit, the outcome string, branch availability, the
  counts, the Jacobian data function, the AICC identity and the rule
  self-consistency; plus the boundary/NaN comparison semantics, the
  scan-before-validation ordering, every scope rejection, and bit-level
  reuse determinism at fixed thread counts.
* `tools/verify_transformation_decision.cpp` sweeps the corpus end-to-end.

### Landing qualification tiers

The pinned ML surface of these fixtures carries competing local optima near
the unit root; the pinned Levenberg-Marquardt and the qualified BFGS replay
can stop at different points, and the discrete `+0.9/-0.9` alternate-start
sign of the Multiple policy (21-A contract) can pick the other basin when
the first-pass winner sits near the sign change. Corpus qualification is
therefore tiered (the generated test and the sweep tool share the tiers):

* *Exact* (`coeff < 2e-3`, `ll < 5e-4`, `aicc < 2e-3`): all fixtures except
  the three below.
* *Flat ridge* (`growth_log` `coeff 2e-3 / ll 1e-2 / aicc 3e-2`;
  `cal_column` `2e-2 / 5e-1 / 5e-1`): documented near-unit-root MA stops;
  measured drift 4.3e-3 and 0.11 ll; decision margins >= 5.7.
* *Basin-unpinned* (`reflected_level`, `seasgrowth_log`): the replay provably
  takes the alternate basin (observed drift up to 1.65 ll / 2.0 coeff); only
  the decision, counts, Jacobian and identities are pinned, and the replayed
  decision margin is asserted > 1.0 (observed >= 32).

This is the same qualification class as the pinned variance-guard landing
(21-A contract): the decision bit stays pinned, the landing does not where
the pinned run itself sits on a knife-edge. A future 21-A replay-fidelity
pass should revisit these two fixtures first.

## Not delivered or claimed

No promotion of `-2.0` (the comparison arrives as required request data;
roadmap sign-off items for stage T stay open), no preadjust-on-decision or
`outliersCorrection` staging (the pinned `RobustOutliersDetector` pre-fits
are stage O/21-C territory), no `llCorrection` beyond the qualified Log-
Jacobian window, no integration into the fit seams, and no claim that the
branch landing reproduces the pinned optimizer on arbitrary data.
