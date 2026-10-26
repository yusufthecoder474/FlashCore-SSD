#include "flashcore.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace fc {
const char *name(Status s) {
    switch (s) {
    case Status::ok:
        return "ok";
    case Status::invalid_address:
        return "invalid_address";
    case Status::non_free:
        return "non_free";
    case Status::non_valid:
        return "non_valid";
    case Status::bad_block:
        return "bad_block";
    case Status::injected_failure:
        return "injected_failure";
    case Status::out_of_space:
        return "out_of_space";
    case Status::unmapped:
        return "unmapped";
    case Status::busy:
        return "busy";
    }
    return "unknown";
}
void Geometry::validate() const {
    const uint64_t total = uint64_t(blocks) * pages;
    // A block is reserved for migration; another page is required for progress.
    if (blocks < 3 || pages == 0 || logical == 0 || total >= none || total > 1000000 ||
        uint64_t(logical) + pages >= total)
        throw std::invalid_argument("geometry needs >=3 blocks, positive pages/logical and >one "
                                    "block OP; maximum 1M pages");
}
Nand::Nand(Geometry g) : geometry(g) {
    g.validate();
    blocks.resize(g.blocks);
    for (auto &b : blocks)
        b.pages.resize(g.pages);
}
bool Nand::address(uint32_t p) const {
    return uint64_t(p) < uint64_t(geometry.blocks) * geometry.pages;
}
const Page &Nand::at(uint32_t p) const {
    return blocks.at(p / geometry.pages).pages.at(p % geometry.pages);
}
Status Nand::read(uint32_t p, Page &out) const {
    if (!address(p)) {
        ++metrics.invalid_ops;
        return Status::invalid_address;
    }
    if (blocks[p / geometry.pages].bad)
        return Status::bad_block;
    if (at(p).state != State::valid)
        return Status::non_valid;
    out = at(p);
    return Status::ok;
}
Status Nand::program(uint32_t p, const Page &page) {
    if (!address(p)) {
        ++metrics.invalid_ops;
        return Status::invalid_address;
    }
    auto &b = blocks[p / geometry.pages];
    auto &target = b.pages[p % geometry.pages];
    if (b.bad)
        return Status::bad_block;
    if (target.state != State::free) {
        ++metrics.invalid_ops;
        return Status::non_free;
    }
    if (program_fault == 0) {
        program_fault = -1;
        return Status::injected_failure;
    }
    if (program_fault > 0)
        --program_fault;
    target = page;
    target.state = State::valid;
    ++metrics.programs;
    return Status::ok;
}
Status Nand::invalidate(uint32_t p) {
    if (!address(p))
        return Status::invalid_address;
    auto &page = blocks[p / geometry.pages].pages[p % geometry.pages];
    if (page.state != State::valid)
        return Status::non_valid;
    page.state = State::invalid;
    return Status::ok;
}
Status Nand::erase(uint32_t b) {
    if (b >= blocks.size()) {
        ++metrics.invalid_ops;
        return Status::invalid_address;
    }
    if (blocks[b].bad)
        return Status::bad_block;
    if (count(b, State::valid))
        return Status::busy;
    if (erase_fault == 0) {
        erase_fault = -1;
        return Status::injected_failure;
    }
    if (erase_fault > 0)
        --erase_fault;
    for (auto &p : blocks[b].pages)
        p = Page{};
    ++blocks[b].erases;
    ++metrics.erases;
    return Status::ok;
}
Status Nand::retire(uint32_t b) {
    if (b >= blocks.size())
        return Status::invalid_address;
    if (blocks[b].bad)
        return Status::bad_block;
    if (count(b, State::valid))
        return Status::busy;
    blocks[b].bad = true;
    return Status::ok;
}
uint32_t Nand::count(uint32_t b, State s) const {
    uint32_t n = 0;
    for (const auto &p : blocks.at(b).pages)
        if (p.state == s)
            ++n;
    return n;
}
uint32_t Nand::free_blocks() const {
    uint32_t n = 0;
    for (uint32_t b = 0; b < blocks.size(); ++b)
        if (!blocks[b].bad && count(b, State::free) == geometry.pages)
            ++n;
    return n;
}
uint32_t Nand::free_pages() const {
    uint32_t n = 0;
    for (uint32_t b = 0; b < blocks.size(); ++b)
        if (!blocks[b].bad)
            n += count(b, State::free);
    return n;
}
Ftl::Ftl(Geometry g, Policy p, Allocation a, Tuning t)
    : nand(g), policy_(p), allocation_(a), tuning_(t), map_(g.logical, none), heat_(g.logical, 0) {
    if (!std::isfinite(t.weight) || t.weight < 0 || t.threshold == 0 || t.decay == 0)
        throw std::invalid_argument("invalid tuning");
}
uint32_t Ftl::slot(uint32_t b) const {
    if (b == none || nand.blocks[b].bad)
        return none;
    for (uint32_t i = 0; i < nand.geometry.pages; ++i)
        if (nand.blocks[b].pages[i].state == State::free)
            return b * nand.geometry.pages + i;
    return none;
}
uint32_t Ftl::free_block(uint32_t exclude) const {
    uint32_t best = none;
    for (uint32_t b = 0; b < nand.blocks.size(); ++b) {
        if (b == exclude || nand.blocks[b].bad || nand.count(b, State::free) != nand.geometry.pages)
            continue;
        if (best == none ||
            (allocation_ == Allocation::wear && nand.blocks[b].erases < nand.blocks[best].erases))
            best = b;
    }
    return best;
}
uint32_t Ftl::victim() const {
    uint32_t best = none;
    double score = -std::numeric_limits<double>::infinity();
    const double w = (policy_ == Policy::adaptive && nand.free_blocks() > tuning_.watermark)
                         ? tuning_.weight
                         : 0;
    for (uint32_t b = 0; b < nand.blocks.size(); ++b) {
        if (b == active_ || nand.blocks[b].bad || nand.count(b, State::invalid) == 0)
            continue;
        uint32_t hot = 0;
        for (const auto &p : nand.blocks[b].pages)
            if (p.state == State::valid && heat_[p.logical] >= tuning_.threshold)
                ++hot;
        const double value =
            (double(nand.count(b, State::invalid)) - w * hot) / nand.geometry.pages;
        if (value > score) {
            score = value;
            best = b;
        }
    }
    return best;
}
Status Ftl::migrate(uint32_t b, bool retiring) {
    // Commit each copied page separately. Failure leaves a consistent partial migration.
    const uint32_t destination = free_block(b);
    if (nand.count(b, State::valid) > 0 && destination == none)
        return Status::out_of_space;
    for (uint32_t i = 0; i < nand.geometry.pages; ++i) {
        const uint32_t old = b * nand.geometry.pages + i;
        const Page page = nand.at(old);
        if (page.state != State::valid)
            continue;
        const auto target = slot(destination);
        const Status status = nand.program(target, page);
        if (status != Status::ok)
            return status;
        map_[page.logical] = target;
        nand.invalidate(old);
        ++nand.metrics.migrations;
    }
    const auto result = retiring ? nand.retire(b) : nand.erase(b);
    if (result == Status::ok && !retiring)
        ++nand.metrics.cycles;
    return result;
}
Status Ftl::collect() {
    const uint32_t b = victim();
    if (b == none)
        return Status::out_of_space;
    return migrate(b, false);
}
Status Ftl::allocate(uint32_t &p) {
    // Prefer reclaiming while two reserves remain, so adaptive scoring can operate.
    for (uint32_t iteration = 0; iteration <= nand.geometry.blocks; ++iteration) {
        p = slot(active_);
        if (p != none)
            return Status::ok;
        // Reuse partially filled migration blocks before opening another erased block.
        for (uint32_t b = 0; b < nand.blocks.size(); ++b) {
            if (!nand.blocks[b].bad && nand.count(b, State::free) > 0 &&
                nand.count(b, State::free) < nand.geometry.pages) {
                active_ = b;
                p = slot(b);
                return Status::ok;
            }
        }
        if (nand.free_blocks() <= 2 && victim() != none) {
            auto s = collect();
            if (s != Status::ok)
                return s;
            continue;
        }
        if (nand.free_blocks() > 1) {
            active_ = free_block();
            p = slot(active_);
            return Status::ok;
        }
        return Status::out_of_space;
    }
    return Status::out_of_space;
}
Status Ftl::write(uint32_t l, uint64_t value) {
    auto &m = nand.metrics;
    ++m.attempted;
    auto fail = [&](Status s) {
        ++m.failed;
        return s;
    };
    if (l >= map_.size()) {
        ++m.invalid_ops;
        return fail(Status::invalid_address);
    }
    if (sequence_ == UINT64_MAX)
        return fail(Status::out_of_space);
    uint32_t p = none;
    const auto alloc = allocate(p);
    if (alloc != Status::ok)
        return fail(alloc);
    Page page;
    page.logical = l;
    page.payload = value;
    page.sequence = sequence_ + 1;
    const auto s = nand.program(p, page);
    if (s != Status::ok)
        return fail(s);
    ++sequence_;
    if (map_[l] != none)
        nand.invalidate(map_[l]);
    map_[l] = p;
    ++m.successful;
    if (heat_[l] != UINT32_MAX)
        ++heat_[l];
    if (++heat_writes_ % tuning_.decay == 0)
        for (auto &h : heat_)
            h /= 2;
    return Status::ok;
}
Status Ftl::read(uint32_t l, uint64_t &value) const {
    if (l >= map_.size()) {
        ++nand.metrics.invalid_ops;
        return Status::invalid_address;
    }
    if (map_[l] == none)
        return Status::unmapped;
    Page p;
    const auto s = nand.read(map_[l], p);
    if (s == Status::ok)
        value = p.payload;
    return s;
}
Status Ftl::trim(uint32_t l) {
    if (l >= map_.size()) {
        ++nand.metrics.invalid_ops;
        return Status::invalid_address;
    }
    if (map_[l] != none) {
#ifndef FLASHCORE_BUG_TRIM
        nand.invalidate(map_[l]);
#endif
        map_[l] = none;
    }
    return Status::ok;
}
Status Ftl::retire(uint32_t b) {
    if (b >= nand.blocks.size())
        return Status::invalid_address;
    if (nand.blocks[b].bad)
        return Status::bad_block;
    // Retirement is planned evacuation, not sudden unreadable data loss.
    if (b == active_)
        active_ = none;
    return migrate(b, true);
}
std::vector<uint32_t> Ftl::rebuild_mapping_from_metadata() const {
    std::vector<uint32_t> result(map_.size(), none);
    for (uint32_t b = 0; b < nand.blocks.size(); ++b)
        if (!nand.blocks[b].bad) {
            for (uint32_t i = 0; i < nand.geometry.pages; ++i) {
                const uint32_t address = b * nand.geometry.pages + i;
                const auto &p = nand.at(address);
                if (p.state != State::valid || p.logical >= result.size())
                    continue;
                const auto old = result[p.logical];
                if (old == none || nand.at(old).sequence < p.sequence)
                    result[p.logical] = address;
            }
        }
    return result;
}
uint32_t Ftl::violations() const {
    uint32_t errors = 0;
    std::vector<bool> seen(uint64_t(nand.geometry.blocks) * nand.geometry.pages, false);
    for (uint32_t l = 0; l < map_.size(); ++l)
        if (map_[l] != none) {
            const auto p = map_[l];
            if (p >= seen.size()) {
                ++errors;
                continue;
            }
            if (seen[p])
                ++errors;
            seen[p] = true;
            const auto &page = nand.at(p);
            if (page.state != State::valid || page.logical != l ||
                nand.blocks[p / nand.geometry.pages].bad)
                ++errors;
        }
    for (uint32_t p = 0; p < seen.size(); ++p)
        if (nand.at(p).state == State::valid && !seen[p])
            ++errors;
    return errors;
}
uint32_t compare(const Ftl &f, const Reference &ref) {
    uint32_t errors = f.violations();
    for (uint32_t l = 0; l < f.nand.geometry.logical; ++l) {
        uint64_t value = 0;
        auto s = f.read(l, value);
        const auto it = ref.find(l);
        if (it == ref.end()) {
            if (s != Status::unmapped)
                ++errors;
        } else if (s != Status::ok || value != it->second)
            ++errors;
    }
    return errors;
}
} // namespace fc
