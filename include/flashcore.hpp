#pragma once
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>
namespace fc {
constexpr uint32_t none = UINT32_MAX;
enum class Status {
    ok,
    invalid_address,
    non_free,
    non_valid,
    bad_block,
    injected_failure,
    out_of_space,
    unmapped,
    busy
};
const char *name(Status s);
enum class State { free, valid, invalid };
struct Page {
    State state = State::free;
    uint32_t logical = none;
    uint64_t payload = 0, sequence = 0;
};
struct Block {
    std::vector<Page> pages;
    uint64_t erases = 0;
    bool bad = false;
};
struct Geometry {
    uint32_t blocks = 24, pages = 8, logical = 128;
    void validate() const;
};
struct Metrics {
    uint64_t attempted = 0, successful = 0, programs = 0, migrations = 0, cycles = 0, erases = 0,
             invalid_ops = 0, failed = 0;
};
class Nand {
  public:
    explicit Nand(Geometry g);
    Geometry geometry;
    std::vector<Block> blocks;
    mutable Metrics metrics;
    // One-shot pre-operation faults. Countdown zero fails the next eligible operation.
    int64_t program_fault = -1, erase_fault = -1;
    Status read(uint32_t physical, Page &out) const;
    Status program(uint32_t physical, const Page &page);
    Status invalidate(uint32_t physical);
    Status erase(uint32_t block);
    Status retire(uint32_t block);
    const Page &at(uint32_t physical) const;
    uint32_t count(uint32_t block, State state) const;
    uint32_t free_blocks() const;
    uint32_t free_pages() const;

  private:
    bool address(uint32_t p) const;
};
enum class Policy { greedy, adaptive };
enum class Allocation { basic, wear };
struct Tuning {
    double weight = 0.5;
    uint32_t threshold = 3, decay = 256, watermark = 1;
};
class Ftl {
  public:
    Ftl(Geometry g = {}, Policy p = Policy::greedy, Allocation a = Allocation::basic,
        Tuning t = {});
    Nand nand;
    Status write(uint32_t lpn, uint64_t value);
    Status read(uint32_t lpn, uint64_t &value) const;
    Status trim(uint32_t lpn);
    Status collect();
    Status retire(uint32_t block);
    std::vector<uint32_t> rebuild_mapping_from_metadata() const;
    uint32_t violations() const;
    const std::vector<uint32_t> &mapping() const {
        return map_;
    }
    uint32_t active() const {
        return active_;
    }
    uint32_t victim() const;
    void reset_metrics() {
        nand.metrics = {};
    }

  private:
    Policy policy_;
    Allocation allocation_;
    Tuning tuning_;
    std::vector<uint32_t> map_, heat_;
    uint32_t active_ = none;
    uint64_t sequence_ = 0, heat_writes_ = 0;
    uint32_t free_block(uint32_t exclude = none) const;
    uint32_t slot(uint32_t block) const;
    Status allocate(uint32_t &physical);
    Status migrate(uint32_t block, bool retiring);
};
using Reference = std::unordered_map<uint32_t, uint64_t>;
uint32_t compare(const Ftl &f, const Reference &ref);
} // namespace fc
