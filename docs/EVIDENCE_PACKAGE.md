# Capability evidence package — item 27 default-backend cutover review

Date: **2026-11-24**, core `main` at `a7d4b12` ("Record the robust factorizer
cart normalization gate").

This is the final read-only evidence index for every delivered capability
family of the kalos numerical core, assembled for the item 27 default-backend
cutover review. It inventories what backs each capability, where the frozen
reference bytes live, how they were captured (oracle provenance), a short
digest anchor, and which tests in the three repositories replay them. Nothing
here re-baselines, regenerates, or modifies any evidence file; every digest
below was read from the committed manifests (`SHA256SUMS.csv`,
`reference.json`) or is the capture SHA-256 recorded in
`IMPLEMENTATION_STATUS.md` for that capture.

## How to read the table

- **Evidence directory** — relative to `validation/evidence/native/` unless
  prefixed `fixtures/` (historical `validation/fixtures/native/` corpora) or
  `reference/` (goldens and captures under `validation/reference/`).
- **Provenance.** *Double-JVM* is the house standard: two independent JVM
  processes produced byte-identical captures before freezing (recorded per
  family in `IMPLEMENTATION_STATUS.md` and the evidence READMEs). *Probe
  rerun* means a single pinned-JVM Java probe whose CSVs are recorded
  byte-identical across reruns. *Independent R oracle* means an independently
  implemented R reference calculation, not a JVM oracle. The JVM math-flag
  domain `-Dline.separator=\n -XX:+UnlockDiagnosticVMOptions
  -XX:DisableIntrinsic=_dexp,_dlog` (pinned FdLibm bytecode semantics,
  CI-FDLIBM) is stated where the capture or replay depends on it.
- **Reference digest.** First 12 hex digits of the SHA-256 of the named
  anchor file: as pinned in that directory's `SHA256SUMS.csv`, as recorded in
  its `reference.json`, or (for the manifest-less `*-x86_64.txt` public
  captures) the capture SHA-256 cited in `IMPLEMENTATION_STATUS.md`, which
  equals the file's own digest at this commit.
- **Consumers.** kalos CTest names, `rkalos/inst/tinytest/test-native-*.R`
  files (fixtures vendored under `inst/tinytest/fixtures/native/`, pinned by
  `inst/core.lock.json`), and `pykalos/tests/test_*.py` (evidence vendored
  under `pykalos/vendor/kalos-evidence/native/`). *vendored-only* means the
  corpus ships pinned in the binding but the golden replay lives in kalos.

## Capability index

| # | Capability | Core entry point(s) | Evidence directory | Capture provenance | Reference digest (short) | Consuming tests (kalos ctest / rkalos / pykalos) |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | SEATS WK extraction | `kalos::seats_extract_wk` (`include/kalos/seats_extract_wk.hpp`) | `seats-wk-extraction`; spectrum-cos rows in `tramo-seats-bridge/spectrum-cos-x86_64.txt` | Double-JVM (`SeatsWkExtractionProbe`, csv byte-identical across two independent JVM processes) | `model-facts.csv` `334ab6fff988` | `test_seats_extract_wk` / `test-native-seats-extract-wk.R` / `test_seats.py` |
| 2 | WK factorizer tiers EV2→EV1→fast→robust + symmetric Muller–Newton | `factorize_sf` tier chain (`include/kalos/detail/seats_ucarima_model.hpp`), `Complex.cart` mirrors | `seats-wk-factorizer-tiers` | Double-JVM per capture (README: "two independent JVM invocations, byte-identical"); adversarial Muller capture from four independent JVM runs; JVM math-flag domain shared with `tramo-seats-bridge` | `reference.json` captures: wk `de4410d4edd8`, muller-adversarial `4e3c2d4aff8e` | replayed as the seas-profile tier walk: `test_decompose_ucarima`, `test_seats_extract_wk`, `test_adjusted_seasonality` (18 fits incl. 2 seas) / vendored-only / — |
| 3 | SEATS admissibility + QR-cascade root solver | `kalos::validate_tramo_seats_model` (`tramo_seats_model_validator.hpp`) | `tramo-seats-model` | Double-JVM (`--repro` re-capture, 4 data files byte-identical, SHA256SUMS 5/5) | `model-facts.csv` `aef6982b0b9d` | `test_tramo_seats_model` + `tools/verify_tramo_seats_model` / vendored-only / — |
| 4 | SEATS UCARIMA decomposition (selector chain, spectral factorization) | `kalos::decompose_ucarima` (`decompose_ucarima.hpp`) | `decompose-ucarima` | Double-JVM (csv byte-identical across JVM processes) | `model-facts.csv` `9c662e09d9b6` | `test_decompose_ucarima` / vendored-only / — |
| 5 | SEATS monthly airline approximation gate | legacy WK approximation retry (`src/seats_extract_wk.cpp`, thresholds mirror) | `seats-airline-approximation` | Double-JVM (eight cases) | `thresholds.csv` `d89b81fd4063` | embedded via `tools/gen_seats_wk_extraction_golden.py` → `test_seats_extract_wk` (132/132) / vendored-only / — |
| 6 | SEATS fitted bridge + deterministic-effects merge | `kalos::tramo_seats_bridge` / merge + calendar restoration (`tramo_seats_bridge.hpp`) | `tramo-seats-bridge` (`bridge.txt`, `merge*.txt`, fitted panels) | Double-JVM, JVM math-flag domain (`capture-tramo-seats-bridge.py` pins the DisableIntrinsic flags) | `reference.json` inputs `be09660e3a75`; `rsafull-x86_64.txt` `c0749b62cae3` | `test_tramo_seats_bridge`, `test_tramo_calendar_effect` / `test-native-seats-from-tramo.R`, `test-native-tramo-preprocess.R` / `test_tramoseats_fit.py`, `test_seats.py` |
| 7 | TRAMO-SEATS integrated fits RSA0 / RSA1 / RSA3 / RSAfull | `estimate_tramo_tr1`/`tr3`/`trfull`, `run_tramo_seats_rsa1`/`rsafull` (`tramo_seats_pipeline.hpp`, `tramo_tr0.hpp`) | `tramo-seats-bridge` (`rsa0-*`, `rsa1-*`, `rsa3-*`, `rsafull-*` panels) | Double-JVM across all product panels (e.g. RSA3 `3811bf5c301e…`, RSAfull products and diagnostics) | `rsa3-x86_64.txt` capture `3811bf5c301e`; `rsafull-residual-diagnostics-x86_64.txt` `0dce2063b45b` | `test_tramo_seats_rsa0`, `test_tramo_seats_automatic`, `test_tramo_selected_model` (+ctests `test_tramo_trfull_reference`, `test_tramo_seats_rsafull`, `test_tramo_rsafull_late_calendar`) / `test-native-tramoseats-{fit,rsa1,rsa3,rsafull,residuals}.R` / `test_tramoseats_fit.py` |
| 8 | X-11 endpoints & filter machinery (initial, mean, MSR, MSR selection, Musgrave, normalization, seasonal, seasonal15, seasonal plan/processing, symmetric filters, trend selection) | `x11_initial.hpp`, `x11_mean_endpoints.hpp`, `x11_msr.hpp`, `x11_msr_selection.hpp`, `x11_musgrave.hpp`, `x11_normalization.hpp`, `x11_seasonal_endpoints.hpp`, `x11_seasonal15`, `x11_seasonal_plan.hpp`, `x11_seasonal_processing.hpp`, `x11_filters.hpp`, `x11_trend_selection.hpp` | `fixtures/x11-initial`, `x11-mean`, `x11-msr`, `x11-msr-selection`, `x11-musgrave`, `x11-normalization`, `x11-seasonal`, `x11-seasonal15`, `x11-seasonal-plan`, `x11-seasonal-processing`, `x11-symmetric`, `x11-trend-selection` | Pre-policy single pinned-JVM capture (2026-11-14 extended-fixtures harness) **paired with an independent R reference operator** ("Independent R operators are separate from capture"); not double-JVM | `points.csv` `a8bbbdd5d9f9` (initial); `weights.csv` `4c276fe779ca` (symmetric); `pilots.csv` `b78a087ec490` (msr-selection) | `test_x11_initial`, `test_x11_mean_endpoints`, `test_x11_msr`, `test_x11_msr_selection`, `test_x11_musgrave`, `test_x11_normalization`, `test_x11_seasonal_endpoints`, `test_x11_seasonal_plan`, `test_x11_seasonal_processing`, `test_x11_filters`, `test_x11_trend_selection` / `test-native-x11-{initial,mean,msr,msr-selection,musgrave,normalization,seasonal,seasonal15,seasonal-plan,seasonal-processing,filters,trend-selection}.R` / — |
| 9 | X-11 sigma policy + same-phase replacement/endpoints/corrections | `x11_sigma.hpp`, `x11_replacement.hpp` | `x11-sigma`, `x11-replacement` | Single pinned-JVM capture (rjd3 3.9.0/3.9.1 jars via rJava, OpenJDK 21.0.11, metadata.R-pinned); **not double-JVM** | `weights.csv` `86c985220b53`; `replacements.csv` `4f29dba94c2f` | `test_x11_sigma`, `test_x11_replacement` / `test-native-x11-{sigma,replacement}.R` / — |
| 10 | X-11 B/C/D passes + selection flow, four decomposition modes | `x11_b_pass.hpp`, `x11_c_pass.hpp`, `x11_d_pass.hpp`, `x11_selection_pass.hpp`, `x11_decomposition.hpp` | `x11-{bpass,cpass,dpass,selection}` + `-log`/`-mult`/`-padd` (16 dirs) | Single pinned-JVM kernel capture (rjd3 3.9.0, JVM 21, `X11Kernel.process` with capture-side cross-checks; re-captured additive fixtures byte-identical); **not double-JVM** | `tables.csv` `fedc622378a1` (bpass), `5f3ac2d14074` (dpass), `488f67b0a642` (selection), `24b4923e954d` (selection-padd) | `test_x11_b_pass`, `test_x11_c_pass`, `test_x11_d_pass`, `test_x11_selection_pass`, `test_x11_multiplicative`, `test_x11_log_additive`, `test_x11_pseudo_additive` / `test-native-x11-{bpass,cpass,dpass,selection,mode-log,mode-mult,mode-padd}.R` / — |
| 11 | X-13 deterministic-effect allocation + finals (`X13Kernel.finals`) | `x13_allocation.hpp` (`x13_restore`) | `x13-allocation` | Pinned-JVM probe (`X13AllocationProbe.java`) with compositional recompute gate; CSVs byte-identical across reruns (probe rerun, single-process family) | `tables.csv` `f5cf9f3c3daf` | `test_x13_allocation` / `test-native-x13-allocation.R` / `test_x13_fixed.py` |
| 12 | X-13 restricted fixed-model fit | `x13_fit.hpp` | `x13-fit` (+ `x13fast.csv` cross-check) | Pinned-JVM probe (`X13FitProbe.java`), byte-identical reruns; `f_empty` additionally re-executed through the R `x13_fast` path bit-exactly | `tables.csv` `687fc8367f7e` | `test_x13_fit` / `test-native-x13-fit.R` / `test_x13_fixed.py` |
| 13 | X-13 automatic preprocessing pipeline (21-H, stage Z) | `x13_automatic.hpp` (+ `final_estimator`, `arima_forecasts` seams) | `x13-automatic` | Double-JVM (slice-owned fresh capture, `oracle-facts.csv` sha256 identical in two independent JVM processes) | `oracle-facts.csv` `798c81acc8f7` | `test_x13_automatic_oracle`, `test_x13_automatic`, `test_final_estimator` / `test-native-x13-automatic.R` / `test_x13_automatic.py` |
| 14 | X-13 nonzero-horizon forecast route | `kalos::arima_forecasts` (`arima_forecasts.hpp`) | `x13-horizon` | Double-JVM (repro gate 3/3 byte-identical) | `oracle-facts.csv` `ed80b8372e22` | `test_x13_automatic_horizon` / `test-native-x13-horizon.R` / `test_x13_automatic_horizon.py` |
| 15 | X-13 refused-spec profiles (fixed/automatic decoder) | fixed/automatic spec decoding + refusal semantics (`x13_fit.hpp`, `x13_automatic.hpp`, `x13_estimated_calendar.hpp`) | `x13-refusal-jvm-profiles` | Cross-platform pinned probe, JVM-flag domain: x86-64 Temurin 21.0.11 + ARM64 divergence reproduced bit-for-byte by a second x86-64 run with six math intrinsics disabled; `application_revision` pinned in `reference.json` | `reference.json` input `seas.csv` `326efc58d35b` | `test_x13_estimated_templates`/`_selection`/`_calendar`, `test_adjusted_seasonality`, `test_combined_seasonality` (inputs) / `test-native-x13-estimated.R` / `test_x13_estimated.py`, `test_x13_fixed.py`, `test_combined_seasonality.py` |
| 16 | X-13 estimated templates — stock RSA0 / RSA1 / RSA2c (item 28) | `x13_estimated_rsa2c`, `x13_rsa2c_calendar_test`, `x13_rsa2c_scale` (`x13_estimated_calendar.hpp`), RSA0/RSA1 native fits | `x13-estimated-templates` | Double-JVM on every panel ("checksum-verified Temurin 21.0.11 replay passed in two fresh processes") | `templates-x86_64.txt` `42be69e16cc2`; `rsa2c-calendar-x86_64.txt` `24844c19a300`; scale panel `f4efd70e645b` | `test_x13_estimated_templates`, `test_x13_estimated_selection`, `test_x13_estimated_calendar` (+ctest `test_x13_rsa2c_scale`) / `test-native-x13-estimated.R` / `test_x13_estimated.py` |
| 17 | X-13 pseudo-additive mode on non-log data | `X13Kernel.updateSpec` mode rule (`x13_fit.hpp`, `x13_automatic.hpp`) | `x13-nonlog-pseudo` | Double-JVM ("Two pinned JVM runs", fixed and automatic) | `fixed-x86_64.txt` `d9192949b880`; `automatic-x86_64.txt` `da18b09fa143` | `test_x13_fit`, `test_x13_automatic_oracle` / vendored-only / — |
| 18 | TRAMO model-builder decision mirror (23-A) | `kalos::tramo_model_builder` (`tramo_model_builder.hpp`) | `tramo-model-builder` | Double-JVM (`--repro` byte-identical across independent JVM processes; probe double-capture 7/7) | `inputs.csv` `134985173466` | `test_tramo_model_builder` / `test-native-tramo-prepare.R` / — |
| 19 | TRAMO pre-stage transform + outlier detection (21-B/C, 23-B, 23-F rounds) | `decide_transformation` (`transformation_decision.hpp`), `decide_tramo_transform` (`tramo_transform_decision.hpp`), `score_outliers`/forward selection (`outlier_scoring.hpp`), `scan_tramo_outliers`, `tramo_outlier_rounds` | `transformation-decision`, `tramo-transform`, `outlier-scoring`, `tramo-outlier-scan`, `tramo-outlier-rounds` | Double-JVM throughout (byte-identical independent-process captures; outlier rounds captured twice per trace) | `inputs.csv` `147556dffa89` (transformation), `a9d16ef14e62` (tramo-transform), `5f8c55c26c7a` (outlier-scoring), `mr-capture-1.out` `026b72fe0d8d` | `test_transformation_decision`, `test_tramo_transform_decision`, `test_outlier_scoring`, `test_tramo_outlier_scan` / `test-native-tramo-transform.R` / `test_tramo_transform.py`, `test_provider_import_order.py` |
| 20 | Differencing decision + TRAMO differencing ladder + seasonal battery (21-D, 23-C, 23-C.2) | `decide_differencing` (`differencing_decision.hpp`), `decide_tramo_differencing` (`tramo_differencing_decision.hpp`), `decide_tramo_differencing_battery` (`tramo_differencing_battery.hpp`) | `differencing-decision`, `tramo-differencing`, `tramo-differencing-battery` (+ `reference/fcdf_golden.txt`, battery `fcdf-capture{1,2}.txt`) | Double-JVM incl. the F-CDF golden (`FcdfGolden.java`, byte-identical from independent JVM processes); second differencing-decision capture ran with `_dexp/_dlog` intrinsics disabled (JVM-flag domain) | `fcdf-capture1.txt` `ce9471e7ad92`; `inputs.csv` `6020cf952b5d` (differencing-decision), `041a094c0a5a` (tramo-differencing) | `test_differencing_decision`, `test_tramo_differencing`, `test_tramo_differencing_battery`, `test_special_functions` / `test-native-tramo-differencing.R` / — |
| 21 | ARMA identification / refinement / TRAMO ARMA selection (21-E/F, 23-D) | `initial_arma_identification.hpp`, `arma_refinement.hpp`, `select_tramo_arma` (`tramo_arma_selection.hpp`) | `initial-arma-identification`, `arma-refinement`, `tramo-arma-selection` | Double-JVM (byte-identical across two independent JVM processes; `--repro`, 7 data files) | `candidates.csv` `fbe5eb2dd254`; `inputs.csv` `b5fd8b112ccc` (refinement), `caf964213eca` (tramo-arma-selection) | `test_initial_arma_identification`, `test_arma_refinement`, `test_tramo_arma` / vendored-only / — |
| 22 | Regression variable tests + final-information producer + fixed-order estimation control (21-G/A.1, 23-E) | `run_tramo_regression_tests` (`tramo_regression_tests.hpp`), `kalos::final_information` (`final_information.hpp`), `estimation_control.hpp`, `hannan_rissanen.hpp` | `regression-tests`, `tramo-regression-tests`, `final-information`, `estimation-control`, `estimation-control-hr` | Double-JVM; the `final-information` and `estimation-control` captures additionally ran with `_dexp/_dlog` intrinsics disabled (JVM-flag domain); the pinned short-window HR crash rows are mirrored as throws | `cases.csv` `f90afae39259`; `capture.csv` `af8d3b2de16b`; `inputs.csv` `9e2bb8aac39f` (estimation-control), `ad37aa4ef43f` (estimation-control-hr) | `test_regression_variable_tests`, `test_tramo_regression_tests`, `test_final_information`, `test_estimation_control`, `test_hannan_rissanen` / vendored-only / `test_estimation.py` |
| 23 | TRAMO final estimation + kernel rounds + fit assembly (23-F) | `tramo_final_estimation.hpp`, `tramo_run_kernel_rounds` (`tramo_kernel_rounds.hpp`), `tramo_fit.hpp` | `tramo-final-estimation`, `tramo-kernel-rounds`, `tramo-fit` | Double capture for F.1 (`fe1-`/`n23-capture-{1,2}.out`) and F.3 (`kr-capture-{1,2}.out`); F.4 corpus is **triple**-identical (three captures + class-load witness) | `fe1-capture-1.out` `f1eb284570ee`; `kr-capture-1.out` `ca567a733b6a`; `fit-capture-1.out` `6fb93a48e77f` | `test_tramo_final_estimation`, `test_tramo_kernel_rounds`, `test_tramo_fit` / vendored-only / — |
| 24 | Calendar design generation (21-I.1) | `calendar_generation.hpp` | `calendar-generation` | Double-JVM ("both double-captured byte-identical in two OpenJDK 21 JVMs") | `gen2.out` `075d58d66e7e` (envelope corpus); `gen.out` `cb8d7ac0…` pinned in metadata | `test_calendar_generation` / vendored-only / — |
| 25 | Calendar regressions, F tests, Fast retest, trading-day controllers (21-I.3/I.4) | `tramo_calendar_f_test.hpp` (F test, Fast retest, TD candidate controller, residual TD test) | `tramo-calendar-ftest` | Double-JVM on every panel ("Two independent x86-64 Java runs produced byte-identical evidence"; "Two pinned JVMs agreed on 130 threshold decisions") | `calendar-outliers.txt` `ca58695a37f0`; `panel.txt`/`fast-calendar.txt`/`td-controller.txt` pinned in `SHA256SUMS.csv` (7 files) | `test_tramo_calendar_f_test`, `test_tramo_calendar_effect` / vendored-only / `test_tramo_calendar.py` |
| 26 | Adjusted-series residual seasonality diagnostics (Qs / seasonal F) | `arma_diagnostics.hpp` Qs + seasonal-F helpers over fitted products | `adjusted-seasonality` | Double-JVM ("Two independent JDemetra+ processes"; short-sample extension independently replayed at its existing SHA) | `public-x86_64.txt` `7503e03fa9e3` (extended capture; first capture `92a579cd62fb` in status) | `test_adjusted_seasonality` / `test-native-adjusted-seasonality.R` / — |
| 27 | Combined SI seasonality diagnostic | `combined_seasonality.hpp` | `combined-seasonality` (public / fixed-x13 / automatic / stock / stock-rsa2c) | Double-JVM on all five panels (status cites two independent JVMs + a capture SHA per panel) | `public-x86_64.txt` `fcad11cf3be6`; `stock-x86_64.txt` `41da1bef3242`; `fixed-x13-x86_64.txt` `d2c629a23b68` | `test_combined_seasonality`, `test_x13_automatic_oracle` / `test-native-combined-seasonality.R`, `test-native-x13-automatic.R` / `test_combined_seasonality.py` |
| 28 | Residual trading-day dashboard tests | residual TD test (`tramo_calendar_f_test.hpp`) + public getter replay | `residual-trading-days` (+ `tramo-calendar-ftest/residual-td.txt`) | Double-JVM capture ("Two independent pinned x86 JVMs", per-product MA(1) witnesses) | `public-x86_64.txt` `e3316fdeec37` | `test_tramo_calendar_f_test` (replays `residual-td.txt`; the dashboard `public-x86_64.txt` capture is frozen, native wiring pending — see caveats) / vendored-only / — |
| 29 | Public preprocessing diagnostics + forecast uncertainty (RSA0/1/3/full, automatic X-13) | `arma_diagnostics.hpp` (Ljung–Box, Doornik–Hansen via portable StrictMath path), `monthly_tramo_residual_ljung_box`, `tramo_tr1_forecast_errors`, `tramo_tr3_forecast_errors`, `tramo_selected_forecast_errors` | `tramo-seats-bridge` (`rsa0-ljung-box`, `rsa0-doornik-hansen`, `rsa*-residual-diagnostics`, `rsa*-forecast-errors`, `rsafull-*`), `x13-automatic-diagnostics`, `tramo-seats-bridge` diagnostics dirs | Double-JVM on every panel (nine stock X-13 fits: "Two independent pinned x86-64 JVM processes produced identical …"; RSA3/4th-pass/retry/RSAfull captures with recorded SHAs); JVM math-flag domain | `rsa0-doornik-hansen-x86_64.txt` family pinned in `SHA256SUMS.csv`; RSA3 diag `3811bf5c301e`, fast-mean-retry diag `afd4385d7096`, RSA3 forecast `a1b77b26c7d8` | `test_arma_diagnostics`, `test_tramo_seats_rsa0`, `test_tramo_seats_automatic`, `test_tramo_selected_model`, `test_x13_estimated_templates` / `test-native-tramoseats-{ljung-box,normality,residuals,forecast-errors}.R` / `test_tramoseats_fit.py` |
| 30 | ARMA likelihood / information / residuals / interpolation / forecast kernels (state-space core) | `arma_objective.hpp`, `arma_information.hpp`, `arma_residuals.hpp`, `forecast_information.hpp`, `interpolation_information.hpp`, `prediction_moments.hpp`, `diffuse_state_space.hpp` | `fixtures/*-oracle.R` + `reference/information/2026-11-14-*` matrices; JVM goldens `reference/{arma_diagnostics,check_roots,special_functions}_golden.txt` | **Independent R reference oracles — no JVM oracle exists for these kernels** (deliberately slow dense-Gaussian / independent implementations, e.g. `arma-residual-oracle.R`); the JVM-backed pieces are the three goldens, captured twice byte-identical across processes with `_dexp/_dlog` intrinsics disabled (JVM-flag domain) | `oracle-manifest.R` sha `a2394411ca58` (fixture pin); `arma_diagnostics_golden.txt`, `check_roots_golden.txt`, `special_functions_golden.txt` (584 / 1637 / 2261 rows, all bit-exact) | `test_arma_residuals`, `test_legacy_arma_residuals`, `test_arma_objective`, `test_information`, `test_arma_information`, `test_forecast_information`, `test_forecast_arma_information`, `test_interpolation_information`, `test_interpolation_arma_information`, `test_joint_regression_information`, `test_regression_information`, `test_residual_information`, `test_prediction_moments`, `test_diffuse_state_space`, `test_state_space`, `test_regression_likelihood`, `test_arma_diagnostics`, `test_final_estimator`, `test_special_functions` / `test-native-{arma-residuals,legacy-residuals,arma-objective,information,likelihood,fixed-likelihood,forecast-arma,interpolation,interpolation-arma,joint-information,regression-information,tramo-information,tramo-legacy,prediction-moments,diffuse}.R` / `test_estimation.py` |

## Coverage summary

- **30 capability families** indexed; all **57** directories under
  `validation/evidence/native/` and all **12** JVM fixture corpora under
  `validation/fixtures/native/` map to exactly one row (plus the
  `validation/reference/` goldens and dated information captures in rows 20,
  24, 30).
- **23 of 30** families are backed by **double-JVM** (or triple, row 23)
  byte-identical pinned-JVM oracles — rows 1–7, 13–14, 16–29.
- **6 of 30** rest on **single pinned-JVM captures** (rows 8–12, 15): the
  early 2026-11-14/15 X-11 fixtures and the two X-13 Java probes. Row 8 is
  additionally paired with an independent R reference operator on every case;
  none of these used the double-JVM process policy.
- **1 of 30** (row 30) has **no JVM oracle at all**: the ARMA likelihood /
  information / residuals / interpolation / forecast kernels are qualified by
  independent R reference oracles and analytic invariants only; their only
  JVM-backed pieces are the special-function / diagnostics goldens.
- Manifest state at this commit: `tools/check-manifest.py` green —
  207 core / 112 fixture / 410 evidence files.

## Caveats — bounded parity scope for the cutover reader

1. **JVM fast-tier nondeterminism (the known JVM-disagrees-with-itself case).**
   The ported `FastEigenValuesSolver.qr` cascade's exceptional `it_count % 25`
   bulge branch "seeds its shift from `new Random()`" — verified across three
   fresh JVM processes that the *root bits* are **not bit-reproducible**
   (status, 24-A.1 entry). Downstream captured decisions are reproducible, so
   the unit substitutes a deterministic shift and the evidence claims are
   tier outcomes/decisions, never fast-tier root bits. The M6 512-case
   factorizer differential campaign re-run twice confirms "no new divergence
   beyond the six JVM-randomized fast-tier cases (156, 233, 302, 400, 412,
   430)". Any cutover parity budget must exempt exactly those fast-tier root
   bits.
2. **Pre-policy single-process X-11 fixtures.** Rows 8–10 predate the
   standing double-JVM policy (captured 2026-11-14/15, `metadata.R` pins the
   harness sources + oracle manifest; re-captures recorded byte-identical but
   not from an independent JVM process). Mitigation: independent R reference
   operators replay them row-for-row in rkalos and bit-exact native replay in
   kalos; treat the second-process guarantee as absent for this family.
3. **JVM-flag-domain parity.** Row 15 and the flagged captures in rows 2, 6,
   20, 22, 29–30 pin the x86-64 numerical profile with `_dexp/_dlog` (and for
   refusals, six) math intrinsics disabled — the FdLibm bytecode semantics of
   the pinned JVM are ground truth (CI-FDLIBM entry), not host libm. ARM64
   JVM output legitimately differs (fitted coefficients) and is reproduced
   only through the flag-disabled x86-64 run.
4. **Frozen oracles awaiting native wiring.** Two public captures are
   double-JVM-frozen but have no test reading them yet:
   `x13-automatic-diagnostics/public-x86_64.txt` (six decoder-equivalent
   automatic X-13 diagnostic runs) and
   `residual-trading-days/public-x86_64.txt` (48 residual TD getters). The
   capabilities themselves are replay-backed via rows 25/28/29 panels
   (`residual-td.txt`, bridge diagnostics, `x13-estimated-templates`
   diagnostics); the two files are surplus public-dashboard references, not
   gaps in capability backing.
5. **Platform evidence outside this package.** Hosted Windows/macOS CI lanes,
   the standalone Windows qualification job and all wheel/bundle deliveries
   are maintainer-side infrastructure; their artifacts (e.g. the Windows
   evidence checkout entry) are not vendored here and are outside this
   package's scope.

## Cutover gaps

No delivered capability was found with **zero** evidence backing — every
family above has at least an independent (analytic/R-oracle or pinned-JVM)
reference, and the four bounded-scope items above (fast-tier root bits,
pre-policy X-11 fixtures, flag-domain claims, two unwired frozen panels) are
the complete list of qualification weaknesses. Vendored-but-not-replayed
corpora (rows 2–4, 21–23 in rkalos) are a packaging property of the bindings,
not missing kalos coverage: each replays bit-exactly in the kalos suite.
