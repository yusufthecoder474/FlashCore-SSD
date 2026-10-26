# Debug log and root cause analysis

## Deliberate defect: stale VALID page after TRIM

This bug was intentionally compiled using `FLASHCORE_BUG_TRIM=ON`. It was not an
accident found in production, and it is disabled in the delivered default build.

**Symptom:** logical reads report unmapped, but the NAND still contains a VALID
page for that logical address. Metadata reconstruction resurrects the trimmed
mapping. A read-only oracle alone would miss the immediate stale physical page.

**Minimal reproduction:** `workloads/trim-reproducer.csv`, seed 0 (seed is unused
for CSV traces): WRITE logical 0 payload 42; TRIM logical 0. Two operations suffice.

```sh
cmake -S . -B build-bug -DFLASHCORE_BUG_TRIM=ON
cmake --build build-bug --config Release
./build-bug/flashcore_tests trim
./build-bug/flashcore test-workload --input workloads/trim-reproducer.csv --seed 0
```

On Windows with Visual Studio generator, executables are under `build-bug/Release`.
Both commands exit 1 as expected. CLI CSV records correctness_violations=2:
one unmapped VALID physical page, plus reconstructed mapping differing from live
map. The reference logical values alone still match after this trace.

**Hypothesis:** TRIM changes the map but forgets the page-state transition.
**Root cause:** the compile-time branch skips `nand.invalidate(map_[l])` in
`Ftl::trim`, then clears the mapping. This violates the physical-to-logical half
of the bidirectional invariant.
**Fix:** invalidate the current physical page before removing its map entry.
**Regression:** `trim`, metadata rebuild comparison, CLI reproducer, and randomized
TRIM operations. Default builds passed; buggy builds failed. Exact observed output
and exit codes are retained in `results/bug-detection.json` and the final
`results/reproduction.json` (expected_exit=1).

## Actual issues encountered during this build

1. **CMake missing from initial PATH.** `cmake --version` returned command not found.
   Installed the Python CMake distribution and added its executable directory to
   PATH for subsequent builds. Final captured commands identify the actual binary.
2. **Compiler warning in CLI parser.** GCC reported misleading indentation where
   the missing-input guard and CSV load shared a line. Split the statements and
   formatted the code. Original warning is in `results/build.log`; final build
   output is in `results/reproduction.json`. No final compiler warnings observed.
3. **LeakSanitizer environment failure.** Initial sanitizer run printed
   `Can't open /proc/.../task for reading` and `LeakSanitizer has encountered a
   fatal error`. This occurred across otherwise passing small tests. Re-ran with
   leak checking disabled, retaining address and undefined-behavior checks;
   all 38 entries passed. Leak detection remains unverified.
4. **Metric completeness review.** Added per-block erase counts to CSV and
   address-error counting for reads; added validator checks that reconstruct
   erase extrema and population standard deviation. This was a review finding,
   not a discovered data-loss failure. Rebuilt and reran the suite/evaluation.
5. **Test depth review.** Added explicit adaptive-versus-greedy victim divergence,
   metadata highest-sequence selection, reserve-loss retirement, and mixed fault
   campaigns. Earlier tests were passing; the additions exercise previously
   under-specified cases. No non-deliberate core data-corruption failure was
   observed in executed tests. That does not prove absence of defects.

## Open boundaries

Partial migration failures can consume spare blocks and eventually prevent
progress. Current policy preserves values and returns an error; it does not
compact failed destinations or guarantee eventual recovery. Retirement is planned
and readable, not sudden hardware failure. Neither boundary is hidden by retries.
