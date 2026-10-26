#include "flashcore.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace fc;

// Failures are expected outcomes. Only successful host writes update the oracle.
// Unlike benchmark tests, this campaign intentionally keeps running after errors.
int main() {
    try {
        uint64_t operations = 0, failures = 0, retirements = 0;
        for (auto policy : {Policy::greedy, Policy::adaptive}) {
            for (auto allocation : {Allocation::basic, Allocation::wear}) {
                for (uint32_t seed = 301; seed < 304; ++seed) {
                    Ftl f({12, 4, 28}, policy, allocation);
                    Reference oracle;
                    std::mt19937 rng(seed);
                    for (uint32_t i = 0; i < 1500; ++i) {
                        // Both migration and host programs can hit this countdown.
                        if (i % 43 == 0)
                            f.nand.program_fault = static_cast<int64_t>(rng() % 3);
                        if (i % 61 == 0)
                            f.nand.erase_fault = 0;
                        const uint32_t logical = static_cast<uint32_t>(rng()) % 28;
                        if (i % 7 == 0) {
                            if (f.trim(logical) != Status::ok)
                                throw std::runtime_error("unexpected trim failure");
                            oracle.erase(logical);
                        } else {
                            const uint64_t payload = rng();
                            const Status status = f.write(logical, payload);
                            if (status == Status::ok)
                                oracle[logical] = payload;
                            else if (status == Status::injected_failure ||
                                     status == Status::out_of_space)
                                ++failures;
                            else
                                throw std::runtime_error("unexpected write status");
                        }
                        if (i == 500 || i == 1000) {
                            const uint32_t block = static_cast<uint32_t>(rng()) % 12;
                            const Status status = f.retire(block);
                            if (status == Status::ok)
                                ++retirements;
                            else if (status != Status::injected_failure &&
                                     status != Status::out_of_space && status != Status::bad_block)
                                throw std::runtime_error("unexpected retirement status");
                        }
                        ++operations;
                        if (compare(f, oracle) != 0 ||
                            f.rebuild_mapping_from_metadata() != f.mapping())
                            throw std::runtime_error(
                                "reference/invariant failure, seed=" + std::to_string(seed) +
                                " operation=" + std::to_string(i));
                        const auto &m = f.nand.metrics;
                        if (m.attempted != m.successful + m.failed ||
                            m.programs != m.successful + m.migrations)
                            throw std::runtime_error("fault-path accounting mismatch");
                    }
                }
            }
        }
        if (failures == 0 || retirements == 0)
            throw std::runtime_error("campaign did not exercise intended paths");
        std::cout << "PASS: " << operations << " operations, " << failures
                  << " expected failed writes, " << retirements << " successful retirements\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
