#include "flashcore.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
using namespace fc;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     " " #x);                                                      \
    } while (false)
void put(Ftl &f, uint32_t l, uint64_t v) {
    CHECK(f.write(l, v) == Status::ok);
}
void value(const Ftl &f, uint32_t l, uint64_t expected) {
    uint64_t v = 0;
    CHECK(f.read(l, v) == Status::ok);
    CHECK(v == expected);
}
void healthy(const Ftl &f) {
    CHECK(f.violations() == 0);
    CHECK(f.rebuild_mapping_from_metadata() == f.mapping());
}
Ftl mixed() {
    Ftl f({8, 4, 16});
    for (uint32_t i = 0; i < 12; ++i)
        put(f, i, i);
    put(f, 0, 100);
    put(f, 1, 101);
    return f;
}
void random_test(Policy policy) {
    for (uint32_t seed = 41; seed < 46; ++seed) {
        Ftl f({12, 4, 32}, policy, Allocation::wear);
        Reference ref;
        std::mt19937 rng(seed);
        for (uint32_t i = 0; i < 3000; ++i) {
            const uint32_t l = static_cast<uint32_t>(rng()) % 32;
            if (rng() % 5 == 0) {
                CHECK(f.trim(l) == Status::ok);
                ref.erase(l);
            } else {
                const uint64_t v = rng();
                put(f, l, v);
                ref[l] = v;
            }
            CHECK(compare(f, ref) == 0);
        }
        healthy(f);
    }
}
int main(int argc, char **argv) {
    const std::map<std::string, std::function<void()>> tests = {
        {"lifecycle",
         [] {
             Nand n({4, 2, 4});
             Page p;
             CHECK(n.read(0, p) == Status::non_valid);
             p.payload = 7;
             p.logical = 0;
             CHECK(n.program(0, p) == Status::ok);
             CHECK(n.program(0, p) == Status::non_free);
             CHECK(n.invalidate(0) == Status::ok);
             CHECK(n.read(0, p) == Status::non_valid);
             CHECK(n.erase(0) == Status::ok);
             CHECK(n.at(0).state == State::free);
         }},
        {"erase_guard",
         [] {
             Nand n({4, 2, 4});
             CHECK(n.program(0, Page{}) == Status::ok);
             CHECK(n.erase(0) == Status::busy);
             CHECK(n.blocks[0].erases == 0);
         }},
        {"geometry",
         [] {
             for (auto g : std::vector<Geometry>{{0, 2, 1},
                                                 {4, 0, 1},
                                                 {4, 2, 0},
                                                 {4, 2, 6},
                                                 {UINT32_MAX, UINT32_MAX, 1},
                                                 {2, 4, 1},
                                                 {1000, 1001, 1}}) {
                 bool thrown = false;
                 try {
                     Nand n(g);
                 } catch (const std::invalid_argument &) {
                     thrown = true;
                 }
                 CHECK(thrown);
             }
             Nand n({3, 2, 3});
             CHECK(n.free_pages() == 6);
         }},
        {"mapping",
         [] {
             Ftl f;
             put(f, 0, 4);
             auto old = f.mapping()[0];
             put(f, 0, 9);
             CHECK(f.mapping()[0] != old);
             CHECK(f.nand.at(old).state == State::invalid);
             value(f, 0, 9);
             healthy(f);
         }},
        {"overwrite",
         [] {
             Ftl f({6, 4, 12});
             for (uint32_t i = 0; i < 2000; ++i) {
                 put(f, 0, i);
                 value(f, 0, i);
             }
             CHECK(f.nand.metrics.cycles > 0);
             healthy(f);
         }},
        {"trim",
         [] {
             Ftl f;
             put(f, 0, 42);
             CHECK(f.trim(0) == Status::ok);
             uint64_t v = 0;
             CHECK(f.read(0, v) == Status::unmapped);
             CHECK(f.trim(0) == Status::ok);
             healthy(f);
         }},
        {"gc_empty",
         [] {
             Ftl f;
             CHECK(f.collect() == Status::out_of_space);
             for (uint32_t i = 0; i < 24; ++i)
                 put(f, i, i);
             CHECK(f.collect() == Status::out_of_space);
             healthy(f);
         }},
        {"gc_mixed",
         [] {
             auto f = mixed();
             CHECK(f.victim() == 0);
             CHECK(f.collect() == Status::ok);
             CHECK(f.nand.metrics.migrations == 2);
             CHECK(f.nand.blocks[0].erases == 1);
             value(f, 0, 100);
             value(f, 2, 2);
             healthy(f);
         }},
        {"gc_preserve",
         [] {
             auto f = mixed();
             for (uint32_t l = 2; l < 4; ++l)
                 put(f, l, 100 + l);
             CHECK(f.collect() == Status::ok);
             CHECK(f.nand.metrics.migrations == 0);
             for (uint32_t l = 0; l < 12; ++l)
                 value(f, l, l < 4 ? 100 + l : l);
             healthy(f);
         }},
        {"wear",
         [] {
             Ftl basic({6, 2, 6}), wear({6, 2, 6}, Policy::greedy, Allocation::wear);
             CHECK(basic.nand.erase(0) == Status::ok);
             CHECK(wear.nand.erase(0) == Status::ok);
             put(basic, 0, 1);
             put(wear, 0, 1);
             CHECK(basic.mapping()[0] == 0);
             CHECK(wear.mapping()[0] == 2);
         }},
        {"invalid_address",
         [] {
             Ftl f;
             uint64_t v = 0;
             CHECK(f.write(UINT32_MAX, 0) == Status::invalid_address);
             CHECK(f.read(UINT32_MAX, v) == Status::invalid_address);
             CHECK(f.trim(UINT32_MAX) == Status::invalid_address);
             CHECK(f.nand.program(UINT32_MAX, Page{}) == Status::invalid_address);
             CHECK(f.nand.erase(UINT32_MAX) == Status::invalid_address);
             healthy(f);
         }},
        {"out_of_space",
         [] {
             Ftl f({4, 2, 4});
             CHECK(f.retire(3) == Status::ok);
             for (uint32_t l = 0; l < 4; ++l)
                 put(f, l, l);
             CHECK(f.write(0, 99) == Status::out_of_space);
             value(f, 0, 0);
             healthy(f);
         }},
        {"bad_block",
         [] {
             Nand n({4, 2, 4});
             CHECK(n.retire(0) == Status::ok);
             Page p;
             CHECK(n.program(0, p) == Status::bad_block);
             CHECK(n.read(0, p) == Status::bad_block);
             CHECK(n.erase(0) == Status::bad_block);
             CHECK(n.free_pages() == 6);
         }},
        {"program_failure",
         [] {
             Ftl f;
             put(f, 0, 42);
             f.nand.program_fault = 0;
             CHECK(f.write(0, 99) == Status::injected_failure);
             value(f, 0, 42);
             CHECK(f.nand.metrics.programs == 1);
             CHECK(f.nand.metrics.failed == 1);
             healthy(f);
         }},
        {"gc_program_failure",
         [] {
             auto f = mixed();
             f.nand.program_fault = 1;
             CHECK(f.collect() == Status::injected_failure);
             CHECK(f.nand.metrics.migrations == 1);
             CHECK(f.nand.blocks[0].erases == 0);
             value(f, 2, 2);
             value(f, 3, 3);
             healthy(f);
         }},
        {"gc_erase_failure",
         [] {
             auto f = mixed();
             f.nand.erase_fault = 0;
             CHECK(f.collect() == Status::injected_failure);
             CHECK(f.nand.metrics.migrations == 2);
             CHECK(f.nand.metrics.erases == 0);
             healthy(f);
             CHECK(f.collect() == Status::ok);
             healthy(f);
         }},
        {"retirement",
         [] {
             auto f = mixed();
             CHECK(f.retire(0) == Status::ok);
             CHECK(f.nand.blocks[0].bad);
             value(f, 2, 2);
             healthy(f);
         }},
        {"retirement_failure",
         [] {
             auto f = mixed();
             f.nand.program_fault = 1;
             CHECK(f.retire(0) == Status::injected_failure);
             CHECK(!f.nand.blocks[0].bad);
             healthy(f);
             CHECK(f.retire(0) == Status::ok);
             healthy(f);
         }},
        {"counters",
         [] {
             auto f = mixed();
             CHECK(f.collect() == Status::ok);
             const auto m = f.nand.metrics;
             CHECK(m.attempted == 14);
             CHECK(m.successful == 14);
             CHECK(m.programs == 16);
             CHECK(m.migrations == 2);
             CHECK(m.cycles == 1);
             CHECK(m.erases == 1);
             CHECK(f.nand.free_pages() == 20);
         }},
        {"rebuild",
         [] {
             auto f = mixed();
             f.collect();
             f.trim(2);
             put(f, 3, 333);
             healthy(f);
         }},
        {"random_greedy", [] { random_test(Policy::greedy); }},
        {"random_adaptive", [] { random_test(Policy::adaptive); }},
        {"tie_break",
         [] {
             auto f = mixed();
             put(f, 4, 104);
             put(f, 5, 105);
             CHECK(f.victim() == 0);
         }},
        {"pressure",
         [] {
             Tuning t;
             t.watermark = 100;
             Ftl f({8, 4, 16}, Policy::adaptive, Allocation::basic, t);
             for (uint32_t i = 0; i < 12; ++i)
                 put(f, i, i);
             put(f, 0, 100);
             put(f, 1, 101);
             CHECK(f.victim() == 0);
         }},
        {"adaptive_choice",
         [] {
             // Threshold one makes every written live page hot. Unequal valid counts alter score.
             Tuning t;
             t.threshold = 1;
             t.decay = 10000;
             t.weight = 2;
             t.watermark = 0;
             Ftl f({8, 4, 16}, Policy::adaptive, Allocation::basic, t);
             for (uint32_t i = 0; i < 12; ++i)
                 put(f, i, i);
             put(f, 0, 100);
             put(f, 4, 104);
             put(f, 5, 105);
             CHECK(f.victim() == 1);
             healthy(f);
         }},
        {"decay",
         [] {
             Tuning t;
             t.decay = 1;
             t.threshold = 1;
             t.weight = 100;
             t.watermark = 0;
             Ftl f({8, 4, 16}, Policy::adaptive, Allocation::basic, t);
             for (uint32_t i = 0; i < 12; ++i)
                 put(f, i, i);
             put(f, 0, 100);
             put(f, 4, 104);
             put(f, 5, 105);
             CHECK(f.victim() == 1);
             healthy(f);
         }},
        {"rebuild_highest_sequence",
         [] {
             Ftl f;
             put(f, 0, 1);
             Page newer = f.nand.at(f.mapping()[0]);
             newer.sequence += 10;
             newer.payload = 2;
             CHECK(f.nand.program(8, newer) == Status::ok);
             CHECK(f.rebuild_mapping_from_metadata()[0] == 8);
             CHECK(f.violations() > 0);
         }},
        {"active_exclusion",
         [] {
             Ftl f({8, 4, 16});
             put(f, 0, 1);
             put(f, 0, 2);
             CHECK(f.nand.count(f.active(), State::invalid) == 1);
             CHECK(f.victim() == none);
             CHECK(f.collect() == Status::out_of_space);
             healthy(f);
         }},
        {"retire_active",
         [] {
             Ftl f;
             put(f, 0, 7);
             auto b = f.active();
             CHECK(f.retire(b) == Status::ok);
             CHECK(f.active() == none);
             value(f, 0, 7);
             put(f, 1, 8);
             healthy(f);
         }},
        {"retire_no_reserve",
         [] {
             Ftl f({4, 2, 4});
             CHECK(f.retire(2) == Status::ok);
             CHECK(f.retire(3) == Status::ok);
             put(f, 0, 7);
             CHECK(f.retire(1) == Status::ok);
             CHECK(f.retire(0) == Status::out_of_space);
             CHECK(!f.nand.blocks[0].bad);
             value(f, 0, 7);
             healthy(f);
         }},
        {"nand_fault_atomicity",
         [] {
             Nand n({4, 2, 4});
             n.program_fault = 0;
             CHECK(n.program(0, Page{}) == Status::injected_failure);
             CHECK(n.at(0).state == State::free);
             CHECK(n.program(0, Page{}) == Status::ok);
             CHECK(n.invalidate(0) == Status::ok);
             n.erase_fault = 0;
             CHECK(n.erase(0) == Status::injected_failure);
             CHECK(n.at(0).state == State::invalid);
             CHECK(n.blocks[0].erases == 0);
             CHECK(n.erase(0) == Status::ok);
             CHECK(n.blocks[0].erases == 1);
         }},
        {"trim_metrics",
         [] {
             Ftl f;
             put(f, 0, 9);
             auto before = f.nand.metrics.programs;
             CHECK(f.trim(0) == Status::ok);
             CHECK(f.nand.metrics.programs == before);
             CHECK(f.nand.metrics.successful == 1);
             healthy(f);
         }},
        {"adaptive_divergence",
         [] {
             Tuning t;
             t.threshold = 2;
             t.decay = 1000;
             t.weight = 2;
             t.watermark = 0;
             Ftl greedy({20, 4, 32}, Policy::greedy, Allocation::basic, t),
                 adaptive({20, 4, 32}, Policy::adaptive, Allocation::basic, t);
             std::mt19937 rng(19);
             bool different = false;
             for (uint32_t i = 0; i < 100 && !different; ++i) {
                 uint32_t l = static_cast<uint32_t>(rng()) % 16;
                 put(greedy, l, i);
                 put(adaptive, l, i);
                 different = greedy.victim() != adaptive.victim();
             }
             CHECK(different);
             healthy(greedy);
             healthy(adaptive);
         }},
        {"partial_failure_recovery", [] {
             auto f = mixed();
             f.nand.program_fault = 1;
             CHECK(f.collect() == Status::injected_failure);
             for (uint32_t i = 0; i < 200; ++i)
                 put(f, i % 16, i);
             healthy(f);
         }}};
    try {
        if (argc != 2 || !tests.count(argv[1]))
            throw std::runtime_error("expected registered test name");
        tests.at(argv[1])();
        std::cout << "PASS " << argv[1] << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
