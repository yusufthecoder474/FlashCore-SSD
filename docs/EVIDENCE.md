# Execution evidence

Executed on 26 October 2026. Primary machine-readable evidence:
`results/reproduction.json`: every clean-release command, working directory,
exit code, expected exit, stdout and stderr. `results/environment.json` captures
the compiler, OS, CMake and Python versions. Earlier logs are retained as history;
final logs supersede initial counts. Host-dependent paths are evidence, not
required installation paths.

## Final results

- Clean Release CTest: 38 passed, 0 failed, 0 skipped.
- Python contracts: 7 unittest methods inside one CTest entry.
- Clean Debug ASan+UBSan CTest: 38 passed, 0 failed, 0 skipped, leak detection disabled.
- Fixed-seed oracle unit loops: 30,000 operations.
- Fault campaign: 18,000 operations, 946 expected failed writes, 24 successful retirements.
- Development: 24 runs, 2 seeds, 3 candidates, 4 workloads; parameters frozen.
- Evaluation: 80 runs, 5 seeds, 5,000 operations each; zero correctness violations.
- Compatibility: 72 runs; zero correctness violations; 4 expected geometry rejections.
- Deliberate bug: unit test exit 1 and CLI exit 1 with 2 correctness violations.
- Fixed minimal reproducer: exit 0; no correctness violations.
- Final release and sanitizer builds: no compiler warnings observed.

## Windows 11 / MSVC Verification — 26 October 2026

**Environment:**
- Windows 11 (build 10.0.26200.9711)
- Visual Studio 2022 Build Tools
- MSVC 19.44.35229.0
- Python 3.12.10
- CMake generator: Visual Studio 17 2022

**Commands executed:**
```powershell
cmake -S . -B build-win
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure
python python/run_validation.py --exe build-win/Release/flashcore.exe --mode evaluation
```

**Observed results:**
- CMake configuration and Release compilation: PASSED
- CTest: 38/38 passed, 0 failed (4.87 seconds)
- Held-out benchmark runs: 80 passed
- Compatibility runs: 72 passed
- Invalid geometries: 4 expected rejections

These results were verified locally on Windows 11. Windows sanitizer execution and hosted CI are not established by these results.

## Exact principal commands from scratch parent

```sh
PATH="$PATH:/root/.local/bin" python FlashCore-SSD/python/reproduce.py --build FlashCore-SSD/build-final
PATH="$PATH:/root/.local/bin" cmake -S FlashCore-SSD -B FlashCore-SSD/build-final-sanitize -DFLASHCORE_SANITIZE=ON -DCMAKE_BUILD_TYPE=Debug
PATH="$PATH:/root/.local/bin" cmake --build FlashCore-SSD/build-final-sanitize -j2
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 PATH="$PATH:/root/.local/bin" ctest --test-dir FlashCore-SSD/build-final-sanitize --output-on-failure
```

Both final build directories were absent before configuration. The reproduction
runner invokes build, CTest, demo, fixed reproducer, fault campaign, evaluation,
compatibility, and separate intentional-bug configure/build/failing checks. It
expects the bug checks to fail; PASS in its log means expected exit observed,
not that the deliberate defect disappeared.

Development commands were executed before held-out evaluation:

```sh
python FlashCore-SSD/python/run_validation.py --exe FlashCore-SSD/build/flashcore --mode development
python FlashCore-SSD/python/run_validation.py --exe FlashCore-SSD/build/flashcore --mode evaluation
```

`development.csv` preserves the initial development output schema (before
per-block erase_counts was added); numerical policy controls and outcomes were
unchanged. The final evaluation CSV contains the expanded schema. Do not run
`parse_csv` on the entire multi-row file; that function validates individual CLI
header+row output. `compare_policies.py` handles consolidated multi-row CSV.

## Retained failure evidence and unsupported checks

`build.log`: initial CLI indentation warning, corrected.
`sanitizer-tests.log`: initial LeakSanitizer environment failure (29 failed,
1 passed in the then-30-entry suite). It is not evidence of passing sanitizers.
`sanitizer-clean-tests.log`: authoritative final supported sanitizer checks.
`bug-detection.json`: intentionally failing TRIM experiment; reproduced in the
clean final bug build in reproduction.json.

Windows 11/MSVC Release build, CTest (38/38), and Python evaluation (80 held-out runs and 72 compatibility runs) were verified locally on 26 October 2026.

MSVC AddressSanitizer, hosted GitHub Actions, leak detection, and physical SSD hardware testing remain unverified. No unresolved correctness defect was observed in the executed tests.

Partial-migration capacity loss is a documented limitation. No code coverage percentage or real-device performance result is claimed.

## Benchmark overview

| Allocation | Workload | Greedy mean WAF | Adaptive mean WAF | Adaptive change | Wear stddev criterion |
|---|---|---:|---:|---:|---|
| basic | sequential | 1.00000 | 1.00000 | +0.00% | Pass |
| basic | uniform | 1.76516 | 1.79040 | +1.43% | Pass |
| basic | hotcold | 2.29688 | 2.18916 | -4.69% | Pass |
| basic | mixed | 1.31680 | 1.32230 | +0.42% | Pass |
| wear | sequential | 1.00000 | 1.00000 | +0.00% | Pass |
| wear | uniform | 1.77080 | 1.78632 | +0.88% | Pass |
| wear | hotcold | 2.29912 | 2.19500 | -4.53% | Pass |
| wear | mixed | 1.31486 | 1.31596 | +0.08% | Pass |
