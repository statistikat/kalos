# Shared preparation contract

Kalos owns period alignment and inclusive estimation-window selection, complete
observed/backcast/forecast regressor coverage, fixed/free coefficient states,
mean/drift construction and the pinned calendar-window log-Jacobian correction.
No prior state crosses an estimation-window boundary. Indices in C++ are zero
based; language bindings convert them for display. Missing response values remain
masked, and required regressor rows must be finite even at missing responses.
Unused supplied regressor history/future values are cropped before validation.

Drift uses the original one-based response position over every requested domain:
`t^(d+D) / ((d+D)! * frequency^D)`. Named coefficient states and generated names
are checked in C++. Conversion, R ts representability, names, serialization and
presentation stay at the R boundary. The existing limits (32 regression columns,
32-bit matrix element counts, differencing orders 0..2) remain explicit.

These functions were ported from the preserved original R helpers in
`validation/migration/original-r/`; they are independently authored code, not
translations of JDemetra implementation. The standalone preparation tests cover
negative years, slices, gaps, extra supplied history, drift origins, fixed/free
states, overflow gates and cancellation/reuse. The migrated R suite supplies
end-to-end numerical equivalence checks.
