#include "flashcore.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
using namespace fc;
uint64_t number(const std::string &s) {
    if (s.empty() || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("expected unsigned integer: " + s);
    size_t used = 0;
    auto n = std::stoull(s, &used);
    if (used != s.size())
        throw std::invalid_argument("integer trailing text");
    return n;
}
uint32_t small(const std::string &s) {
    auto n = number(s);
    if (n > UINT32_MAX)
        throw std::invalid_argument("integer exceeds uint32");
    return static_cast<uint32_t>(n);
}
struct Operation {
    char op;
    uint32_t logical;
    uint64_t value;
};
std::vector<Operation> load(const std::string &path) {
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("cannot open workload: " + path);
    std::string line;
    if (!std::getline(in, line))
        throw std::runtime_error("empty CSV");
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    if (line != "op,lpn,value")
        throw std::runtime_error("CSV header must be op,lpn,value");
    std::vector<Operation> ops;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        auto a = line.find(','), b = line.find(',', a == std::string::npos ? 0 : a + 1);
        if (a != 1 || b == std::string::npos || line.find(',', b + 1) != std::string::npos)
            throw std::runtime_error("malformed workload row");
        if (line[0] != 'W' && line[0] != 'R' && line[0] != 'T')
            throw std::runtime_error("unknown workload operation");
        ops.push_back({line[0], small(line.substr(a + 1, b - a - 1)), number(line.substr(b + 1))});
    }
    if (ops.empty())
        throw std::runtime_error("workload has no operations");
    return ops;
}
int main(int argc, char **argv) {
    try {
        if (argc < 2)
            throw std::invalid_argument(
                "usage: flashcore demo|inspect|test-workload --input trace.csv|benchmark [--policy "
                "greedy|adaptive --alloc basic|wear --seed N --workload "
                "sequential|uniform|hotcold|mixed]");
        std::string command = argv[1];
        if (command != "demo" && command != "inspect" && command != "test-workload" &&
            command != "benchmark")
            throw std::invalid_argument("unknown command");
        std::map<std::string, std::string> options;
        const std::vector<std::string> allowed = {
            "--policy", "--alloc",     "--seed",  "--workload", "--input",
            "--output", "--blocks",    "--pages", "--logical",  "--operations",
            "--weight", "--threshold", "--decay", "--watermark"};
        for (int i = 2; i < argc; i += 2) {
            std::string key = argv[i];
            if (i + 1 >= argc || std::find(allowed.begin(), allowed.end(), key) == allowed.end() ||
                options.count(key))
                throw std::invalid_argument("unknown, duplicate or missing option: " + key);
            options[key] = argv[i + 1];
        }
        auto get = [&](const std::string &k, const std::string &d) {
            auto it = options.find(k);
            return it == options.end() ? d : it->second;
        };
        auto policy = get("--policy", "greedy"), allocation = get("--alloc", "basic"),
             workload = get("--workload", "uniform");
        if (policy != "greedy" && policy != "adaptive")
            throw std::invalid_argument("invalid policy");
        if (allocation != "basic" && allocation != "wear")
            throw std::invalid_argument("invalid allocation");
        if (workload != "sequential" && workload != "uniform" && workload != "hotcold" &&
            workload != "mixed")
            throw std::invalid_argument("invalid workload");
        Geometry g{small(get("--blocks", "24")), small(get("--pages", "8")),
                   small(get("--logical", "128"))};
        const uint32_t seed = small(get("--seed", "1")),
                       operations = small(get("--operations", "5000"));
        if (operations == 0 || operations > 10000000)
            throw std::invalid_argument("operations must be 1..10000000");
        Tuning t;
        size_t consumed = 0;
        const auto weight = get("--weight", "0.5");
        t.weight = std::stod(weight, &consumed);
        if (consumed != weight.size())
            throw std::invalid_argument("invalid weight");
        t.threshold = small(get("--threshold", "3"));
        t.decay = small(get("--decay", "256"));
        t.watermark = small(get("--watermark", "1"));
        if (command != "test-workload" && options.count("--input"))
            throw std::invalid_argument("input only valid for test-workload");
        Ftl f(g, policy == "greedy" ? Policy::greedy : Policy::adaptive,
              allocation == "basic" ? Allocation::basic : Allocation::wear, t);
        Reference reference;
        uint32_t errors = 0, operation_errors = 0;
        auto apply = [&](Operation op) {
            if (op.op == 'W') {
                auto s = f.write(op.logical, op.value);
                if (s == Status::ok)
                    reference[op.logical] = op.value;
                else
                    ++operation_errors;
            } else if (op.op == 'T') {
                auto s = f.trim(op.logical);
                if (s == Status::ok)
                    reference.erase(op.logical);
                else
                    ++operation_errors;
            } else {
                uint64_t value = 0;
                auto s = f.read(op.logical, value);
                auto it = reference.find(op.logical);
                if (op.logical >= g.logical)
                    ++operation_errors;
                else if (it == reference.end()) {
                    if (s != Status::unmapped)
                        ++errors;
                } else if (s != Status::ok || value != it->second)
                    ++errors;
            }
        };
        std::vector<Operation> trace;
        if (command == "benchmark") {
            // Fill logical capacity. Counters reset; physical state and heat are retained.
            for (uint32_t l = 0; l < g.logical; ++l)
                apply({'W', l, l});
            if (operation_errors || compare(f, reference))
                throw std::runtime_error("warm-up failed");
            f.reset_metrics();
            std::mt19937 rng(seed);
            for (uint32_t i = 0; i < operations; ++i) {
                uint32_t l = i % g.logical;
                if (workload != "sequential")
                    l = static_cast<uint32_t>(rng()) % g.logical;
                if (workload == "hotcold" && rng() % 100 < 85)
                    l = static_cast<uint32_t>(rng()) % std::max(1U, g.logical / 10);
                const char op = (workload == "mixed" && rng() % 100 < 20) ? 'T' : 'W';
                trace.push_back({op, l, (uint64_t(rng()) << 32) | rng()});
            }
        } else if (command == "test-workload") {
            if (!options.count("--input"))
                throw std::invalid_argument("missing --input");
            trace = load(options.at("--input"));
        } else if (command == "demo") {
            trace = {{'W', 0, 42}, {'W', 1, 99}, {'W', 0, 43},
                     {'R', 0, 0},  {'T', 1, 0},  {'R', 1, 0}};
        }
        // Stable FNV-like digest identifies the exact trace independent of policy.
        uint64_t digest = 1469598103934665603ULL;
        for (const auto &op : trace)
            for (auto v : {uint64_t(op.op), uint64_t(op.logical), op.value}) {
                digest ^= v;
                digest *= 1099511628211ULL;
            }
        const auto start = std::chrono::steady_clock::now();
        for (const auto &op : trace)
            apply(op);
        const auto end = std::chrono::steady_clock::now();
        errors += compare(f, reference);
        if (f.rebuild_mapping_from_metadata() != f.mapping())
            ++errors;
        const double seconds = std::chrono::duration<double>(end - start).count();
        const auto &m = f.nand.metrics;
        double mean = 0, variance = 0;
        uint64_t minimum = UINT64_MAX, maximum = 0;
        for (const auto &b : f.nand.blocks) {
            mean += double(b.erases);
            minimum = std::min(minimum, b.erases);
            maximum = std::max(maximum, b.erases);
        }
        mean /= g.blocks;
        for (const auto &b : f.nand.blocks)
            variance += (double(b.erases) - mean) * (double(b.erases) - mean);
        const double waf = m.successful ? double(m.programs) / double(m.successful) : 0;
        std::ostringstream csv;
        csv << std::setprecision(12);
        csv << "policy,allocation,workload,seed,blocks,pages_per_block,logical_pages,op_pages,"
               "operations,trace_hash,host_attempted,host_successful,physical_programs,gc_"
               "migrations,gc_cycles,erases,invalid_ops,failed_writes,free_pages,erase_min,erase_"
               "max,erase_stddev,waf,wall_seconds,simulator_throughput,correctness_violations,"
               "operation_errors,erase_counts,weight,threshold,decay,watermark\n";
        csv << policy << ',' << allocation << ',' << (command == "benchmark" ? workload : command)
            << ',' << seed << ',' << g.blocks << ',' << g.pages << ',' << g.logical << ','
            << (g.blocks * g.pages - g.logical) << ',' << trace.size() << ',' << digest << ','
            << m.attempted << ',' << m.successful << ',' << m.programs << ',' << m.migrations << ','
            << m.cycles << ',' << m.erases << ',' << m.invalid_ops << ',' << m.failed << ','
            << f.nand.free_pages() << ',' << minimum << ',' << maximum << ','
            << std::sqrt(variance / g.blocks) << ',' << waf << ',' << seconds << ','
            << (seconds > 0 ? double(trace.size()) / seconds : 0) << ',' << errors << ','
            << operation_errors << ',';
        for (uint32_t b = 0; b < g.blocks; ++b) {
            if (b)
                csv << '|';
            csv << f.nand.blocks[b].erases;
        }
        csv << ',' << t.weight << ',' << t.threshold << ',' << t.decay << ',' << t.watermark
            << '\n';
        if (options.count("--output")) {
            std::ofstream out(options.at("--output"));
            if (!out)
                throw std::runtime_error("cannot open output");
            out << csv.str();
            if (!out)
                throw std::runtime_error("cannot write output");
        }
        std::cout << csv.str();
        std::cerr << "FlashCore educational simulator: " << m.successful
                  << " successful writes; WAF=" << waf << "; correctness violations=" << errors
                  << "; operation errors=" << operation_errors << "\n";
        if (command == "inspect" || command == "demo")
            for (uint32_t b = 0; b < g.blocks; ++b)
                std::cerr << "block " << b << ": free=" << f.nand.count(b, State::free)
                          << " valid=" << f.nand.count(b, State::valid)
                          << " invalid=" << f.nand.count(b, State::invalid)
                          << " erases=" << f.nand.blocks[b].erases
                          << " bad=" << f.nand.blocks[b].bad << '\n';
        return (errors || operation_errors) ? 1 : 0;
    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 2;
    }
}
