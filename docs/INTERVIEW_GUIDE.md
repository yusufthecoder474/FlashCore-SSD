# FlashCore interview guide: 24 implementation-specific questions

1. **What did you build?** A C++17 NAND/FTL simulator with validation tooling. `Nand`
   models blocks/pages; `Ftl` maps logical addresses. It is not firmware running
   on an SSD controller, and I have not verified it on NAND hardware.
2. **What problem does it explain?** Updates cannot overwrite programmed pages in
   this NAND abstraction. Out-of-place writes accumulate invalid pages; GC moves
   remaining valid data before erasing a block for reuse.
3. **Why C++17?** Fixed-width integers, enum class status codes, vectors and RAII
   keep ownership explicit. No owning raw pointers or inheritance were needed.
4. **What is stored in each page?** `Page` contains `State`, logical ID, uint64_t
   payload and uint64_t sequence. Logical ID and sequence imitate OOB metadata;
   payload and metadata are committed together in this simplified model.
5. **Why block erasure?** The model erases all pages of a block. `Nand::erase`
   rejects VALID pages as a software guard; real hardware would not necessarily
   protect the caller from erasing live data.
6. **How does WRITE preserve the old value on failure?** `Ftl::write` programs a
   FREE replacement first. Only a successful program invalidates the old page
   and changes the map. GC may already have moved that old value safely.
7. **What happens on an unmapped READ?** `Ftl::read` returns `Status::unmapped`.
   Reading an invalid physical page returns `non_valid`; these are different
   abstraction levels and tests cover both.
8. **What is over-provisioning?** Physical pages minus logical capacity. Default
   geometry is 24x8 physical pages with 128 logical pages: 64 spare pages, 33.33%
   of physical capacity or 50% relative to logical capacity. State the denominator.
9. **Why reserve a block?** A GC victim can contain valid data requiring a free
   destination. `allocate` does not open the last fully free block for host writes.
   `migrate` uses one erased block, sufficient for at most P valid pages.
10. **How is greedy GC deterministic?** `victim()` scans blocks in ascending order
    and replaces the candidate only for a strictly greater score. Greedy uses
    invalid count/P, so equal invalid counts retain the lowest block index.
11. **How is adaptive different?** It subtracts w*hot_valid/P. Heat is successful
    host-write frequency with periodic halving. Frozen w=0.5, T=3, N=256. The
    heuristic tries not to copy hot data likely to be overwritten soon.
12. **Could adaptive starve collection?** At or below one free block, w=0. Eligible
    victims still require invalid pages. Allocation is bounded and may return
    out_of_space; the code does not claim universal forward progress.
13. **Did adaptive always win?** No. Mean WAF fell about 4.53% for hot/cold with
    wear allocation, but increased on uniform and mixed traces. Sequential was
    identical. These are five-seed synthetic results, not hardware gains.
14. **What does wear-aware allocation do?** `free_block()` chooses the erased
    block with least erase count, breaking ties by index. It does not relocate
    cold live data solely for wear, so it is not static wear leveling.
15. **What happens halfway through migration failure?** Completed copies remain
    mapped at their destinations and their old pages are INVALID. Uncopied pages
    remain mapped in the victim. No victim erase occurs; space may be reduced.
16. **How do you model a bad block?** `Ftl::retire` evacuates readable valid pages
    and then marks the source bad. It is controlled retirement; sudden unreadable
    data would require redundancy/ECC/recovery outside this project.
17. **How is WAF calculated?** Successful physical programs divided by successful
    host writes. Physical programs include GC and successful partial migrations.
    Failed programs and TRIM add nothing. The zero-host-write convention is 0.
18. **How do you know contents are correct?** Successful operations update an
    `unordered_map` reference. `compare` checks every logical address; `violations`
    checks both directions of the map/page relationship and duplicate ownership.
19. **What did the deliberate bug teach?** Clearing the map during TRIM without
    invalidating the page can pass logical-read checks but fail physical
    invariants and metadata reconstruction. A two-operation trace caught it.
20. **Is metadata reconstruction crash recovery?** No. It returns a vector from
    VALID metadata using highest sequence. There is no durable image, journal,
    restart process, torn-page handling, or persistent TRIM tombstone.
21. **How did you prevent evaluation leakage?** Candidates used development seeds
    11/12; parameters were written before evaluation. Held-out seeds were 101–105,
    with identical trace hashes for each policy pair. No retuning followed.
22. **What is simulator throughput?** Trace operations divided by measured wall
    time. Trace generation and final checks are outside the timer. It measures
    this program on a CPU, not SSD IOPS or NAND latency.
23. **What was verified and what is pending?** Linux release and ASan+UBSan passed
    38 CTest entries. Leak detection was blocked by the environment. Windows/MSVC,
    MSVC ASan, hosted CI, and all hardware validation remain unexecuted.
24. **What would you improve next without inflating scope?** Test partial-failure
    space recovery and extend phase-change traces. Keep any change separately
    evaluated, preserve frozen benchmarks, and avoid adding NVMe or ECC claims
    without a real implementation and evidence.

Before an interview, manually trace WRITE 0=42, WRITE 0=43, TRIM 0; explain every
state transition and counter. Be prepared to show the negative results and the
actual expected-failure log. Do not claim independent authorship of generated
code or personal hardware experience you do not have.
