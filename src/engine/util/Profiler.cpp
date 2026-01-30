// Profiler.cpp
#include "Profiler.hpp"

#include <iomanip>  // for std::fixed, std::setprecision

namespace eeng
{
namespace util
{

std::mutex Profiler::mtx_;
std::unordered_map<std::string, Profiler::CategoryData> Profiler::data_;
std::unordered_map<std::string, Profiler::TimePoint>    Profiler::active_;
std::unordered_map<std::string, Profiler::CategorySnapshot> Profiler::last_snapshots_;
uint64_t Profiler::snapshot_seq_ = 0;

Profiler::CategorySnapshot Profiler::build_snapshot_locked(
    const std::string& category,
    const CategoryData& data)
{
    CategorySnapshot snapshot{};
    snapshot.subtasks.reserve(data.accum.size());

    for (const auto& [name, acc] : data.accum)
    {
        snapshot.subtasks.push_back(SubtaskStats{ name, acc.totalMs, acc.count });
        if (name == category)
        {
            snapshot.has_total_subtask = true;
            snapshot.total_ms = acc.totalMs;
            snapshot.total_count = acc.count;
        }
    }

    if (!snapshot.has_total_subtask)
    {
        for (const auto& p : data.accum)
            snapshot.total_ms += p.second.totalMs;
    }

    return snapshot;
}

void Profiler::start(const std::string& category,
                     const std::string& subtask)
{
    auto now = Clock::now();
    std::lock_guard<std::mutex> lock(mtx_);
    active_[makeKey(category, subtask)] = now;
    data_.emplace(category, CategoryData{});
}

void Profiler::stop(const std::string& category,
                    const std::string& subtask)
{
    auto now = Clock::now();
    std::lock_guard<std::mutex> lock(mtx_);
    auto key = makeKey(category, subtask);
    auto it = active_.find(key);
    if (it == active_.end())
    {
        return;
    }

    double ms = std::chrono::duration<double, std::milli>(now - it->second).count();
    active_.erase(it);

    auto& acc = data_[category].accum[subtask];
    acc.totalMs += ms;
    acc.count++;
}

void Profiler::log(const std::string& category,
                   std::ostream& os)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = data_.find(category);
    if (it == data_.end())
    {
        os << "[" << category << "] no data to report\n";
        return;
    }

    auto& accums = it->second.accum;
    // Determine overall total
    double totalMs = 0.0;
    auto overallIt = accums.find(category);
    if (overallIt != accums.end())
    {
        totalMs = overallIt->second.totalMs;
    }
    else
    {
        for (const auto& p : accums)
        {
            totalMs += p.second.totalMs;
        }
    }

    os << "[" << category << "] total="
       << std::fixed << std::setprecision(2)
       << totalMs << "ms\n";

    for (const auto& p : accums)
    {
        const auto& name = p.first;
        const auto& acc  = p.second;
        double avg       = acc.count ? (acc.totalMs / acc.count) : 0.0;
        double pct       = totalMs > 0.0 ? (acc.totalMs / totalMs) * 100.0 : 0.0;

        os << "  " << std::setw(15) << name
           << " total=" << std::setw(7) << std::fixed << std::setprecision(2)
           << acc.totalMs << "ms"
           << "  count=" << std::setw(3) << acc.count
           << "  avg="   << std::setw(7) << std::fixed << std::setprecision(2)
           << avg << "ms"
           << "  (" << std::setw(5) << std::fixed << std::setprecision(1)
           << pct << "%)\n";
    }
}

void Profiler::reset(const std::string& category)
{
    std::lock_guard<std::mutex> lock(mtx_);
    data_.erase(category);
    for (auto it = active_.begin(); it != active_.end(); )
    {
        if (it->first.rfind(category + "#", 0) == 0)
        {
            it = active_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool Profiler::get_snapshot(const std::string& category,
                            CategorySnapshot& out)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = data_.find(category);
    if (it == data_.end())
        return false;

    out = build_snapshot_locked(category, it->second);
    return true;
}

bool Profiler::snapshot_and_reset(const std::string& category)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = data_.find(category);
    if (it == data_.end())
        return false;

    auto snapshot = build_snapshot_locked(category, it->second);
    snapshot.sequence = ++snapshot_seq_;
    last_snapshots_[category] = snapshot;
    data_.erase(it);

    for (auto active_it = active_.begin(); active_it != active_.end(); )
    {
        if (active_it->first.rfind(category + "#", 0) == 0)
        {
            active_it = active_.erase(active_it);
        }
        else
        {
            ++active_it;
        }
    }

    return true;
}

bool Profiler::get_last_snapshot(const std::string& category,
                                 CategorySnapshot& out)
{
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = last_snapshots_.find(category);
    if (it == last_snapshots_.end())
        return false;

    out = it->second;
    return true;
}

} // namespace util
} // namespace eeng
