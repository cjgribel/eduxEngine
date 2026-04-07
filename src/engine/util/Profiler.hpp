// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

// Profiler.hpp
#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>
#include <iostream>
#include <ostream>
#include <iomanip>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace eeng
{
namespace util
{

/**
 * @brief A global profiler that accumulates timing intervals
 *        for named categories and subtasks.
 *
 * Usage examples:
 *   eeng::util::Profiler::start("cat");            // overall interval
 *   …
 *   eeng::util::Profiler::stop("cat");
 *
 *   eeng::util::Profiler::start("cat","sub");    // named subtask
 *   …
 *   eeng::util::Profiler::stop("cat","sub");
 *
 * Then:
 *   eeng::util::Profiler::log("cat");              // write report to std::cout
 *   eeng::util::Profiler::log("cat", myStream);    // write to custom ostream
 *   eeng::util::Profiler::reset("cat");            // clear data
 */
class Profiler
{
public:
    struct SubtaskStats
    {
        std::string name;
        double total_ms = 0.0;
        int count = 0;
    };

    struct CategorySnapshot
    {
        double total_ms = 0.0;
        int total_count = 0;
        bool has_total_subtask = false;
        uint64_t sequence = 0;
        std::vector<SubtaskStats> subtasks;
    };

    /**
     * @brief Start timing an interval for the category as its own subtask.
     *
     * @param category  Category name (used also as subtask name).
     */
    static void start(const std::string& category)
    {
        start(category, category);
    }

    /**
     * @brief Stop timing the overall interval for the category.
     *
     * @param category  Category name (must match prior start).
     */
    static void stop(const std::string& category)
    {
        stop(category, category);
    }

    /**
     * @brief Start timing a named subtask within a category.
     *
     * @param category  Category name.
     * @param subtask   Subtask name.
     */
    static void start(const std::string& category,
                      const std::string& subtask);

    /**
     * @brief Stop timing a previously started subtask.
     *
     * @param category  Category name.
     * @param subtask   Subtask name.
     */
    static void stop(const std::string& category,
                     const std::string& subtask);

    /**
     * @brief Log accumulated data for a category to an output stream.
     *
     * @param category  Category name to log.
     * @param os        Output stream (default: std::cout).
     */
    static void log(const std::string& category,
                    std::ostream& os = std::cout);

    /**
     * @brief Reset all accumulated data for a category.
     *
     * @param category  Category name to reset.
     */
    static void reset(const std::string& category);

    /**
     * @brief Retrieve a snapshot of accumulated data for a category.
     *
     * @param category  Category name to snapshot.
     * @param out       Output snapshot.
     * @return true if data was found for the category.
     */
    static bool get_snapshot(const std::string& category,
                             CategorySnapshot& out);

    /**
     * @brief Snapshot and reset accumulated data for a category.
     *
     * @param category  Category name to snapshot and reset.
     * @return true if data was found for the category.
     */
    static bool snapshot_and_reset(const std::string& category);

    /**
     * @brief Retrieve the last stored snapshot for a category.
     *
     * @param category  Category name to retrieve.
     * @param out       Output snapshot.
     * @return true if a snapshot was available for the category.
     */
    static bool get_last_snapshot(const std::string& category,
                                  CategorySnapshot& out);

private:
    using Clock     = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    struct Accum
    {
        double totalMs = 0.0;
        int    count   = 0;
    };

    struct CategoryData
    {
        std::unordered_map<std::string, Accum> accum;
    };

    static std::mutex                                    mtx_;
    static std::unordered_map<std::string, CategoryData> data_;
    static std::unordered_map<std::string, TimePoint>    active_;
    static std::unordered_map<std::string, CategorySnapshot> last_snapshots_;
    static uint64_t snapshot_seq_;

    static std::string makeKey(const std::string& category,
                               const std::string& subtask)
    {
        return category + "#" + subtask;
    }

    static CategorySnapshot build_snapshot_locked(const std::string& category,
                                                  const CategoryData& data);
};

} // namespace util
} // namespace eeng

#define START_TIMER(...)   ::eeng::util::Profiler::start(__VA_ARGS__)
#define STOP_TIMER(...)    ::eeng::util::Profiler::stop(__VA_ARGS__)
#define LOG_TIMER(...)     ::eeng::util::Profiler::log(__VA_ARGS__)
#define RESET_TIMER(...)   ::eeng::util::Profiler::reset(__VA_ARGS__)
