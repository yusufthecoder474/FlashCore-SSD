# Resume bullets — measured simulator evidence

Use only after you can build and explain the code. These are verified project
measurements, not evidence of hardware firmware experience or independent authorship.

- Developed a C++17 NAND/FTL simulator with page mapping, two GC policies and fault handling; validated correctness through **38 CTest entries** and **18,000 fault-campaign operations**.
- Evaluated simulated GC across **80 held-out runs** and **72 compatibility runs**, observing **4.53% lower hot/cold mean WAF** with adaptive GC and wear allocation, while documenting regressions on other workloads.
- Automated storage validation with Python CSV checks and a reference model, detecting an intentionally introduced TRIM defect using a **two-operation reproducer** and nonzero correctness-gate exits.

Wording clarification: compatibility comprises 18 geometry combinations x 4
policy/allocation combinations = 72 runs, not 72 unique physical geometries.
Prefer “72 compatibility runs” in a space-limited resume. WAF comparison uses
five seeds at 24x8 physical pages / 128 logical pages. No hardware endurance,
SSD IOPS, Windows verification, or novel-algorithm claim is ready.
