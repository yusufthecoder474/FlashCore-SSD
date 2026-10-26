# Test plan and coverage

Run `ctest --test-dir build -C Release --output-on-failure` after building.
Final execution: 38 CTest entries passed, 0 failed, 0 skipped. These include
34 named C++ unit cases, two CLI smoke/error entries, one Python contract entry
containing 7 unittest methods, and one fault campaign. Do not add these together
and call them 45 independent CTest tests. Assertions inside loops are not tests.

| Area | Cases and meaningful assertions |
|---|---|
| NAND | lifecycle, erase_guard, geometry, bad_block, nand_fault_atomicity: state transitions, valid-page erase rejection, overflow/zero/OP rejection, pre-operation fault atomicity |
| FTL | mapping, overwrite, trim, invalid_address, out_of_space, trim_metrics: value preservation, old invalidation, idempotent trim, reserve exhaustion, host counters |
| GC | gc_empty, gc_mixed, gc_preserve, active_exclusion: all-valid has no victim; mixed block copies two pages; all-invalid copies none; active excluded |
| Allocation/policies | wear, tie_break, pressure, adaptive_choice, adaptive_divergence, decay: erase-count preference, deterministic ties, pressure fallback, scoring, actual policy divergence and decay |
| Faults | program_failure, gc_program_failure, gc_erase_failure, partial_failure_recovery: unchanged host value, consistent partial migrations, no premature erasure and later progress in tested geometry |
| Retirement | retirement, retirement_failure, retire_active, retire_no_reserve: evacuation, failure retry, active reset, safe exhaustion |
| Integrity | counters, rebuild, rebuild_highest_sequence: exact arithmetic, metadata reconstruction, duplicate detection |
| Random oracle | random_greedy, random_adaptive: 5 seeds per policy x 3,000 operations, full oracle and invariants after every operation |
| Fault campaign | 2 policies x 2 allocation modes x 3 seeds x 1,500 operations = 18,000 operations; mixed writes/TRIM, program/erase faults and retirement; check oracle, reconstruction and accounting after every operation |
| Python | malformed/missing CSV, seven corrupt metrics, deterministic replay excluding wall time, same policy trace hash, invalid args, input/output CSV, invalid workload address |

The fault campaign observed 946 expected failed writes and 24 successful
retirements. Both outcomes were required for the campaign to pass. The ordinary
random tests exercised another 30,000 operations. Benchmark final gates compare
every logical page after each trace; they do not run invariants after every write.

## Validation datasets

- Development: 24 runs (3 candidates x 4 workloads x 2 seeds), 2,000 operations/run.
- Held-out: 80 runs (4 workloads x 2 allocators x 2 policies x 5 seeds), 5,000 operations/run.
- Compatibility: 72 runs (3 block counts x 3 page sizes x 2 OP levels x 2 policies x 2 allocators), 1,000 operations/run.
- Rejections: 4 unsupported geometries, each expected exit 2.
- Deliberate defect: unit and CLI reproducer must each exit 1; fixed build exits 0.

## Sanitizers and portability

GCC ASan+UBSan: 38/38 passed with `ASAN_OPTIONS=detect_leaks=0` and
`UBSAN_OPTIONS=halt_on_error=1`. Initial default LeakSanitizer run failed because
this environment denies the necessary /proc task access; retain its log.
Windows/MSVC, MSVC ASan, and GitHub Actions have NOT EXECUTED status. Workflow
configuration is supplied, not evidence of hosted CI success. No code-coverage
percentage is claimed. Research experiments do not establish hardware correctness.
