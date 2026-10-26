# Research comparison: a small reproducible experiment

Question: does the specified hot-page penalty reduce GC migrations and WAF
relative to greedy while keeping erase wear acceptably balanced in this model?
This is a teaching heuristic, not a novel algorithm or a reproduction of AGC.

## Protocol and frozen controls

Geometry: 24 blocks x 8 pages = 192 physical pages, 128 logical pages, 64 OP pages.
Warm-up writes each logical page once, checks correctness, then resets counters.
State and heat remain. Measurement performs 5,000 operations. This fill-only
warm-up does not establish steady state. Wear counters are cumulative; no warm-up
erases occur in this configuration. Data is synthetic.

Sequential cycles through logical addresses. Uniform uses mt19937 modulo logical
capacity. Hot/cold directs 85% of selections to the first floor(L/10) pages, with
the other 15% uniform across the full address space (so hot addresses also receive
some of that 15%). Mixed uses uniform addresses and 20% TRIM operations. Modulo
selection has slight bias; no claim of mathematically perfect uniformity.

Development used seeds 11 and 12, four workloads, wear allocation, 2,000 operations
and three predeclared candidates. Lowest mean development WAF wins; ties use the
first candidate. Scores: 1.55919417, 1.55334654, 1.56623112.
Selected w=0.5, T=3, N=256, watermark=1. Freeze was written before held-out runs;
see `frozen_parameters.json`. Basic allocation is a held-out extension, not a
separately tuned configuration. Counters decay on successful host writes.

Held-out seeds: 101, 102, 103, 104, 105. Four workloads, two allocators and two
policies produce 80 runs. Each paired policy run checks the same trace hash.
Timing is excluded from deterministic equality. Results are descriptive means
and medians; no confidence interval or statistical significance is claimed.

Before evaluation, an exploratory wear criterion was fixed: adaptive mean erase
stddev <= 1.10 x greedy for each workload/allocation. This is a simulation reporting
threshold, not an industry endurance standard or proof of acceptable device life.

## Results

Negative percentage means lower adaptive WAF.

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

Adaptive lowered hot/cold mean WAF by 4.69% (basic) and 4.53% (wear), with fewer
migrations. It increased WAF on uniform and mixed traces. Sequential results were
identical. All eight comparisons met the exploratory wear-spread criterion.
Do not claim universal superiority. Wear-aware allocation itself does not guarantee
balanced wear when cold data remains stationary; there is no static leveling.

Full means and medians for WAF, migrations, GC cycles, erase min/max/stddev and
correctness violations follow. Raw counts and per-block wear are in evaluation.csv.

| Workload | Allocation | Policy | Metric | Mean | Median |
|---|---|---|---|---:|---:|
| hotcold | basic | adaptive | waf | 2.18916 | 2.19660 |
| hotcold | basic | adaptive | gc_migrations | 5945.80000 | 5983.00000 |
| hotcold | basic | adaptive | gc_cycles | 1362.40000 | 1367.00000 |
| hotcold | basic | adaptive | erase_min | 0.00000 | 0.00000 |
| hotcold | basic | adaptive | erase_max | 95.60000 | 94.00000 |
| hotcold | basic | adaptive | erase_stddev | 21.14867 | 21.66010 |
| hotcold | basic | adaptive | correctness_violations | 0.00000 | 0.00000 |
| hotcold | basic | greedy | waf | 2.29688 | 2.29900 |
| hotcold | basic | greedy | gc_migrations | 6484.40000 | 6495.00000 |
| hotcold | basic | greedy | gc_cycles | 1429.60000 | 1431.00000 |
| hotcold | basic | greedy | erase_min | 0.00000 | 0.00000 |
| hotcold | basic | greedy | erase_max | 101.40000 | 102.00000 |
| hotcold | basic | greedy | erase_stddev | 25.21029 | 25.07364 |
| hotcold | basic | greedy | correctness_violations | 0.00000 | 0.00000 |
| hotcold | wear | adaptive | waf | 2.19500 | 2.19500 |
| hotcold | wear | adaptive | gc_migrations | 5975.00000 | 5975.00000 |
| hotcold | wear | adaptive | gc_cycles | 1366.00000 | 1366.00000 |
| hotcold | wear | adaptive | erase_min | 26.60000 | 27.00000 |
| hotcold | wear | adaptive | erase_max | 74.60000 | 75.00000 |
| hotcold | wear | adaptive | erase_stddev | 14.94954 | 15.27230 |
| hotcold | wear | adaptive | correctness_violations | 0.00000 | 0.00000 |
| hotcold | wear | greedy | waf | 2.29912 | 2.29920 |
| hotcold | wear | greedy | gc_migrations | 6495.60000 | 6496.00000 |
| hotcold | wear | greedy | gc_cycles | 1431.00000 | 1431.00000 |
| hotcold | wear | greedy | erase_min | 28.40000 | 29.00000 |
| hotcold | wear | greedy | erase_max | 83.00000 | 84.00000 |
| hotcold | wear | greedy | erase_stddev | 18.24107 | 18.01268 |
| hotcold | wear | greedy | correctness_violations | 0.00000 | 0.00000 |
| mixed | basic | adaptive | waf | 1.32230 | 1.31668 |
| mixed | basic | adaptive | gc_migrations | 1289.60000 | 1268.00000 |
| mixed | basic | adaptive | gc_cycles | 655.60000 | 653.00000 |
| mixed | basic | adaptive | erase_min | 0.00000 | 0.00000 |
| mixed | basic | adaptive | erase_max | 33.40000 | 33.00000 |
| mixed | basic | adaptive | erase_stddev | 6.19281 | 6.12146 |
| mixed | basic | adaptive | correctness_violations | 0.00000 | 0.00000 |
| mixed | basic | greedy | waf | 1.31680 | 1.31668 |
| mixed | basic | greedy | gc_migrations | 1267.60000 | 1268.00000 |
| mixed | basic | greedy | gc_cycles | 652.80000 | 653.00000 |
| mixed | basic | greedy | erase_min | 0.00000 | 0.00000 |
| mixed | basic | greedy | erase_max | 34.40000 | 34.00000 |
| mixed | basic | greedy | erase_stddev | 6.24232 | 6.29801 |
| mixed | basic | greedy | correctness_violations | 0.00000 | 0.00000 |
| mixed | wear | adaptive | waf | 1.31596 | 1.31593 |
| mixed | wear | adaptive | gc_migrations | 1264.20000 | 1265.00000 |
| mixed | wear | adaptive | gc_cycles | 652.40000 | 653.00000 |
| mixed | wear | adaptive | erase_min | 23.80000 | 24.00000 |
| mixed | wear | adaptive | erase_max | 29.00000 | 29.00000 |
| mixed | wear | adaptive | erase_stddev | 1.44324 | 1.31696 |
| mixed | wear | adaptive | correctness_violations | 0.00000 | 0.00000 |
| mixed | wear | greedy | waf | 1.31486 | 1.31369 |
| mixed | wear | greedy | gc_migrations | 1259.80000 | 1256.00000 |
| mixed | wear | greedy | gc_cycles | 652.00000 | 652.00000 |
| mixed | wear | greedy | erase_min | 22.20000 | 22.00000 |
| mixed | wear | greedy | erase_max | 29.60000 | 29.00000 |
| mixed | wear | greedy | erase_stddev | 1.93754 | 1.86339 |
| mixed | wear | greedy | correctness_violations | 0.00000 | 0.00000 |
| sequential | basic | adaptive | waf | 1.00000 | 1.00000 |
| sequential | basic | adaptive | gc_migrations | 0.00000 | 0.00000 |
| sequential | basic | adaptive | gc_cycles | 619.00000 | 619.00000 |
| sequential | basic | adaptive | erase_min | 0.00000 | 0.00000 |
| sequential | basic | adaptive | erase_max | 37.00000 | 37.00000 |
| sequential | basic | adaptive | erase_stddev | 16.55541 | 16.55541 |
| sequential | basic | adaptive | correctness_violations | 0.00000 | 0.00000 |
| sequential | basic | greedy | waf | 1.00000 | 1.00000 |
| sequential | basic | greedy | gc_migrations | 0.00000 | 0.00000 |
| sequential | basic | greedy | gc_cycles | 619.00000 | 619.00000 |
| sequential | basic | greedy | erase_min | 0.00000 | 0.00000 |
| sequential | basic | greedy | erase_max | 37.00000 | 37.00000 |
| sequential | basic | greedy | erase_stddev | 16.55541 | 16.55541 |
| sequential | basic | greedy | correctness_violations | 0.00000 | 0.00000 |
| sequential | wear | adaptive | waf | 1.00000 | 1.00000 |
| sequential | wear | adaptive | gc_migrations | 0.00000 | 0.00000 |
| sequential | wear | adaptive | gc_cycles | 619.00000 | 619.00000 |
| sequential | wear | adaptive | erase_min | 0.00000 | 0.00000 |
| sequential | wear | adaptive | erase_max | 33.00000 | 33.00000 |
| sequential | wear | adaptive | erase_stddev | 13.23814 | 13.23814 |
| sequential | wear | adaptive | correctness_violations | 0.00000 | 0.00000 |
| sequential | wear | greedy | waf | 1.00000 | 1.00000 |
| sequential | wear | greedy | gc_migrations | 0.00000 | 0.00000 |
| sequential | wear | greedy | gc_cycles | 619.00000 | 619.00000 |
| sequential | wear | greedy | erase_min | 0.00000 | 0.00000 |
| sequential | wear | greedy | erase_max | 33.00000 | 33.00000 |
| sequential | wear | greedy | erase_stddev | 13.23814 | 13.23814 |
| sequential | wear | greedy | correctness_violations | 0.00000 | 0.00000 |
| uniform | basic | adaptive | waf | 1.79040 | 1.79280 |
| uniform | basic | adaptive | gc_migrations | 3952.00000 | 3964.00000 |
| uniform | basic | adaptive | gc_cycles | 1113.20000 | 1115.00000 |
| uniform | basic | adaptive | erase_min | 0.00000 | 0.00000 |
| uniform | basic | adaptive | erase_max | 54.40000 | 54.00000 |
| uniform | basic | adaptive | erase_stddev | 10.15145 | 10.11728 |
| uniform | basic | adaptive | correctness_violations | 0.00000 | 0.00000 |
| uniform | basic | greedy | waf | 1.76516 | 1.76300 |
| uniform | basic | greedy | gc_migrations | 3825.80000 | 3815.00000 |
| uniform | basic | greedy | gc_cycles | 1097.40000 | 1096.00000 |
| uniform | basic | greedy | erase_min | 0.00000 | 0.00000 |
| uniform | basic | greedy | erase_max | 56.60000 | 57.00000 |
| uniform | basic | greedy | erase_stddev | 10.77265 | 10.69917 |
| uniform | basic | greedy | correctness_violations | 0.00000 | 0.00000 |
| uniform | wear | adaptive | waf | 1.78632 | 1.78660 |
| uniform | wear | adaptive | gc_migrations | 3931.60000 | 3933.00000 |
| uniform | wear | adaptive | gc_cycles | 1110.80000 | 1111.00000 |
| uniform | wear | adaptive | erase_min | 42.40000 | 42.00000 |
| uniform | wear | adaptive | erase_max | 48.20000 | 48.00000 |
| uniform | wear | adaptive | erase_stddev | 1.58065 | 1.43795 |
| uniform | wear | adaptive | correctness_violations | 0.00000 | 0.00000 |
| uniform | wear | greedy | waf | 1.77080 | 1.77260 |
| uniform | wear | greedy | gc_migrations | 3854.00000 | 3863.00000 |
| uniform | wear | greedy | gc_cycles | 1101.00000 | 1102.00000 |
| uniform | wear | greedy | erase_min | 38.00000 | 38.00000 |
| uniform | wear | greedy | erase_max | 50.00000 | 50.00000 |
| uniform | wear | greedy | erase_stddev | 3.68101 | 3.67423 |
| uniform | wear | greedy | correctness_violations | 0.00000 | 0.00000 |

## Compatibility

72 successful runs: blocks {8,12,16}, pages/block {2,4,8}, OP {2,3} blocks,
two policies and two allocation modes. Each uses mixed seed 201, 1,000 operations.
Four invalid geometries were correctly rejected. See compatibility.csv and
unsupported.csv. This is geometry compatibility, not device/protocol compatibility.

## Verified literature and boundaries

Sources checked 26 October 2026. Only bibliographic metadata and accessible
abstracts were used; no inaccessible full-text findings are asserted.

1. **AGC: An Adaptive Workload Burst-Aware Garbage Collection Mechanism for
   High-Performance SSDs**, Hui Sun et al., IEEE Transactions on Computers,
   75(3), 845–859, March 2026. DOI **10.1109/TC.2025.3642636**. Verified via
   [author publication list](https://www.eng.auburn.edu/~xqin/pubs/) and
   [publisher record](https://www.computer.org/csdl/journal/tc/2026/03/11298441/2cmEJl3nI0U).
   Publisher abstract describes predicting workload bursts and reducing access
   conflicts. FlashCore has no queue or timing model and does not implement AGC.
2. **A survey on SSD wear leveling techniques**, Computer Science Review,
   volume 60, article 100891, 2026. Title/venue/year verified from
   [publisher metadata](https://www.sciencedirect.com/science/article/pii/S1574013725001674).
   DOI **10.1016/j.cosrev.2025.100891** also appears in
   [ACM bibliographic record](https://dl.acm.org/doi/10.1016/j.cosrev.2025.100891).
   Full-text verification pending; no detailed algorithm claims derived from it.
3. **Adaptive multi-phased wear leveling method for SSD lifetime enhancement**,
   Tingyu Wang et al., **Journal of Information and Intelligence**, 4(3), 202–220,
   May 2026 (online/copyright 2025), DOI **10.1016/j.jiixd.2025.10.001**.
   [Publisher record](https://www.sciencedirect.com/science/article/pii/S2949715925000575)
   verifies the title, venue and DOI. The supplied prompt's Journal of Industrial
   Information Integration attribution was incorrect. Its abstract describes
   phase-dependent thresholds; FlashCore does not model lifetime phases.
4. **Device-Level Optimization Techniques for Solid-State Drives: A Survey**,
   Tianyu Ren et al., arXiv:2507.10573, submitted 2025, revised February 2026,
   DOI **10.48550/arXiv.2507.10573**.
   [Primary record](https://arxiv.org/abs/2507.10573). It covers FTL, reliability
   and device-level optimization; FlashCore intentionally models only a small subset.
5. **MQSim: A Framework for Enabling Realistic Studies of Modern Multi-Queue SSD
   Devices**, Arash Tavakkol et al., FAST 2018, pp. 49–66.
   [USENIX primary record](https://www.usenix.org/conference/fast18/presentation/tavakkol).
   No DOI is listed there; DOI unverified, use the proceedings URL. MQSim models
   queues/end-to-end latency and is hardware-validated in its paper. FlashCore is not.

## Limits and what this experiment does not establish

No real NAND, ECC, timing, channels, NVMe, PCIe, endurance limit, static wear
leveling, persistent recovery, or hardware data. Small payloads, small geometry,
short traces, modulo RNG bias, fill-only warm-up, five evaluation seeds and a
small candidate set limit generalization. The reference is an in-memory value
oracle, not a flash specification. Performance validation measures simulator
runtime and operation counts; it is not SSD IOPS. There is no demonstrated
novelty, research-gap closure, hardware endurance gain, deployment, or ROI.
The useful contribution is an explainable, reproducible policy comparison with
failure handling and honest negative outcomes.
