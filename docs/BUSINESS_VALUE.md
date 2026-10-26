# Business value: educational firmware validation

| Implemented feature | Modest practical benefit | Evidence/limit |
|---|---|---|
| NAND lifecycle and explicit statuses | Makes invalid storage operations reproducible before hardware access | Unit tests; not actual NAND protocol validation |
| Oracle plus bidirectional invariants | Detects value errors and orphaned physical pages | Deliberate TRIM defect rejected by CLI and unit test |
| Deterministic fault countdowns | Replays failure paths for debugging and code review | 18,000-operation campaign; no torn writes or ECC |
| Python CSV validation | Stops comparison reports when executable or metric checks fail | Bad CSV and arithmetic corruption rejected |
| Policy/geometry experiments | Shows that GC tradeoffs depend on workload | 80 held-out and 72 compatibility runs; synthetic data |
| CMake/CTest and Windows instructions | Makes the project reviewable by another developer | Linux verified; Windows and hosted CI pending |

No customer deployment, hardware endurance gain, monetary savings, manual-effort
reduction, or ROI was measured. The value demonstrated is reproducible learning,
test design, and issue investigation aligned with an entry-level firmware role.
