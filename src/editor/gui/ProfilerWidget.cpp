// Created by Carl Johan Gribel 2025.
// Licensed under the MIT License. See LICENSE file for details.

#include "ProfilerWidget.hpp"

#include "ResourceManager.hpp"
#include "meta/MetaAux.h"
#include "util/Profiler.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct StorageMemoryRow
    {
        std::string type_name;
        size_t capacity = 0;
        size_t used = 0;
        size_t elem_size = 0;
        size_t bytes_total = 0;
        size_t bytes_used = 0;
    };

    struct StorageMemorySnapshot
    {
        std::vector<StorageMemoryRow> rows;
        size_t bytes_total = 0;
        size_t bytes_used = 0;
        double timestamp_s = 0.0;
    };

    struct FrameTimingSample
    {
        double total_ms = 0.0;
        double cap_ms = 0.0;
        double swap_ms = 0.0;
        double wait_ms = 0.0;
        double effective_ms = 0.0;
        int total_count = 0;
        std::vector<eeng::util::Profiler::SubtaskStats> tasks;
    };

    struct FrameTimingHistory
    {
        struct TaskAggregate
        {
            double total_ms = 0.0;
            int count = 0;
        };

        int capacity = 120;
        int count = 0;
        int head = 0;
        uint64_t last_sequence = 0;
        double total_ms_sum = 0.0;
        double cap_ms_sum = 0.0;
        double swap_ms_sum = 0.0;
        double wait_ms_sum = 0.0;
        double effective_ms_sum = 0.0;
        int total_count_sum = 0;
        std::unordered_map<std::string, TaskAggregate> task_sums;
        std::vector<FrameTimingSample> samples;

        void reset(int new_capacity)
        {
            capacity = new_capacity;
            count = 0;
            head = 0;
            last_sequence = 0;
            total_ms_sum = 0.0;
            cap_ms_sum = 0.0;
            swap_ms_sum = 0.0;
            wait_ms_sum = 0.0;
            effective_ms_sum = 0.0;
            total_count_sum = 0;
            task_sums.clear();
            samples.clear();
            samples.resize(capacity);
            for (auto& sample : samples)
                sample.tasks.reserve(16);
        }

        void remove_sample(const FrameTimingSample& sample)
        {
            total_ms_sum -= sample.total_ms;
            cap_ms_sum -= sample.cap_ms;
            swap_ms_sum -= sample.swap_ms;
            wait_ms_sum -= sample.wait_ms;
            effective_ms_sum -= sample.effective_ms;
            total_count_sum -= sample.total_count;
            for (const auto& task : sample.tasks)
            {
                auto it = task_sums.find(task.name);
                if (it == task_sums.end())
                    continue;
                it->second.total_ms -= task.total_ms;
                it->second.count -= task.count;
                if (it->second.count <= 0)
                    task_sums.erase(it);
            }
        }

        void add_sample(const eeng::util::Profiler::CategorySnapshot& snapshot)
        {
            if (samples.empty())
                reset(capacity);

            if (count == capacity)
            {
                remove_sample(samples[head]);
            }
            else
            {
                ++count;
            }

            FrameTimingSample& dst = samples[head];
            dst.total_ms = snapshot.total_ms;
            dst.cap_ms = 0.0;
            dst.swap_ms = 0.0;
            dst.total_count = snapshot.total_count;
            dst.tasks.clear();
            dst.tasks.reserve(snapshot.subtasks.size());

            for (const auto& task : snapshot.subtasks)
            {
                if (task.name == "Frame Cap")
                    dst.cap_ms = task.total_ms;
                if (task.name == "Swap")
                    dst.swap_ms = task.total_ms;
                dst.tasks.push_back(task);
                auto& agg = task_sums[task.name];
                agg.total_ms += task.total_ms;
                agg.count += task.count;
            }

            dst.wait_ms = dst.cap_ms + dst.swap_ms;
            dst.effective_ms = dst.total_ms - dst.wait_ms;
            if (dst.effective_ms < 0.0)
                dst.effective_ms = 0.0;

            total_ms_sum += snapshot.total_ms;
            cap_ms_sum += dst.cap_ms;
            swap_ms_sum += dst.swap_ms;
            wait_ms_sum += dst.wait_ms;
            effective_ms_sum += dst.effective_ms;
            total_count_sum += snapshot.total_count;
            head = (head + 1) % capacity;
        }

        bool push_snapshot(const eeng::util::Profiler::CategorySnapshot& snapshot)
        {
            if (snapshot.sequence == 0 || snapshot.sequence == last_sequence)
                return false;
            last_sequence = snapshot.sequence;
            add_sample(snapshot);
            return true;
        }
    };

    void format_bytes(double bytes, char* out, size_t out_size)
    {
        const char* units[] = { "B", "KiB", "MiB", "GiB" };
        int unit = 0;
        double value = bytes;
        while (value >= 1024.0 && unit < 3)
        {
            value /= 1024.0;
            ++unit;
        }
        std::snprintf(out, out_size, "%.2f %s", value, units[unit]);
    }
}

namespace eeng::gui
{
    ProfilerWidget::ProfilerWidget(EngineContext& ctx)
        : ctx(ctx)
    {
    }

    void ProfilerWidget::draw()
    {
        if (ImGui::CollapsingHeader("Frame Timings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static FrameTimingHistory history{};
            static int history_window = 120;
            static int plot_points = 120;
            static bool show_total_plot = false;
            static bool show_wait_plot = true;
            constexpr int kMaxWindow = 240;

            if (history.samples.empty())
                history.reset(history_window);

            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::SliderInt("Average Window (frames)", &history_window, 1, kMaxWindow))
                history.reset(history_window);

            util::Profiler::CategorySnapshot snapshot{};
            if (util::Profiler::get_last_snapshot("Frame", snapshot))
                history.push_snapshot(snapshot);

            if (history.count == 0)
            {
                ImGui::TextDisabled("No frame timing data yet.");
            }
            else
            {
                const double avg_total_ms = history.total_ms_sum / history.count;
                const double avg_cap_ms = history.cap_ms_sum / history.count;
                const double avg_swap_ms = history.swap_ms_sum / history.count;
                const double avg_wait_ms = history.wait_ms_sum / history.count;
                const double avg_effective_ms = history.effective_ms_sum / history.count;
                const double fps = avg_effective_ms > 0.0 ? (1000.0 / avg_effective_ms) : 0.0;
                ImGui::Text("Avg over %d frame(s): %.2f ms (%.1f FPS)", history.count, avg_effective_ms, fps);
                ImGui::Text("Wait: %.2f ms (cap %.2f, swap %.2f) | total: %.2f ms",
                    avg_wait_ms, avg_cap_ms, avg_swap_ms, avg_total_ms);
                ImGui::Separator();

                static float target_frame_ms = 16.67f;
                static float spike_ms = 100.0f;
                float budget_ms = target_frame_ms - static_cast<float>(avg_effective_ms);

                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputFloat("Target frame (ms)", &target_frame_ms, 0.5f, 1.0f, "%.2f");
                if (target_frame_ms < 1.0f)
                    target_frame_ms = 1.0f;

                ImGui::SameLine();
                if (ctx.engine_config && ImGui::Button("Use cap"))
                    target_frame_ms = ctx.engine_config->get_value(EngineValue::MinFrameTime);

                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputFloat("Spike (ms)", &spike_ms, 1.0f, 5.0f, "%.1f");
                if (spike_ms < 0.0f)
                    spike_ms = 0.0f;

                ImGui::Text("Budget: %.2f ms per frame (target - effective)", budget_ms);
                if (budget_ms > 0.01f && spike_ms > 0.0f)
                {
                    const float frames_needed = spike_ms / budget_ms;
                    ImGui::Text("Estimate: %.1f frames to amortize %.1f ms", frames_needed, spike_ms);
                }
                else
                {
                    ImGui::TextDisabled("No positive budget to amortize spikes.");
                }

                struct DisplayTask
                {
                    std::string name;
                    double avg_ms = 0.0;
                    double avg_count = 0.0;
                };

                std::vector<DisplayTask> tasks;
                tasks.reserve(history.task_sums.size());
                double sum_avg_ms = 0.0;

                for (const auto& [name, agg] : history.task_sums)
                {
                    if (name == "Frame" || name == "Frame Cap" || name == "Swap")
                        continue;
                    const double avg_ms = agg.total_ms / history.count;
                    const double avg_count = static_cast<double>(agg.count) / history.count;
                    tasks.push_back(DisplayTask{ name, avg_ms, avg_count });
                    sum_avg_ms += avg_ms;
                }

                std::sort(tasks.begin(), tasks.end(),
                    [](const auto& a, const auto& b)
                    {
                        return a.avg_ms > b.avg_ms;
                    });

                double uncategorized_ms = avg_effective_ms - sum_avg_ms;
                if (uncategorized_ms < 0.0)
                    uncategorized_ms = 0.0;

                if (ImGui::BeginTable("FrameTimingsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Task");
                    ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                    ImGui::TableSetupColumn("Avg Count", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                    ImGui::TableHeadersRow();

                    auto emit_row = [&](const char* name, double avg_ms, double avg_count)
                    {
                        const double pct = avg_effective_ms > 0.0 ? (avg_ms / avg_effective_ms) * 100.0 : 0.0;

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(name);

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%.2f", avg_ms);

                        ImGui::TableSetColumnIndex(2);
                        if (avg_count > 0.0)
                            ImGui::Text("%.2f", avg_count);
                        else
                            ImGui::TextUnformatted("-");

                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%.1f", pct);
                    };

                    for (const auto& task : tasks)
                        emit_row(task.name.c_str(), task.avg_ms, task.avg_count);

                    if (uncategorized_ms > 0.01)
                        emit_row("Uncategorized", uncategorized_ms, 0.0);

                    ImGui::EndTable();
                }

                ImGui::SetNextItemWidth(140.0f);
                ImGui::SliderInt("Plot Points", &plot_points, 16, kMaxWindow);
                plot_points = std::max(16, std::min(plot_points, history.capacity));

                ImGui::Checkbox("Show total plot", &show_total_plot);
                ImGui::SameLine();
                ImGui::Checkbox("Show wait plot", &show_wait_plot);

                const int sample_count = history.count;
                const int points = std::min(plot_points, sample_count);
                if (points > 0)
                {
                    static std::vector<float> effective_plot;
                    static std::vector<float> wait_plot;
                    static std::vector<float> total_plot;
                    effective_plot.resize(points);
                    if (show_wait_plot)
                        wait_plot.resize(points);
                    if (show_total_plot)
                        total_plot.resize(points);

                    const int start = (history.head - history.count + history.capacity) % history.capacity;
                    float max_effective = 0.0f;
                    float max_wait = 0.0f;
                    float max_total = 0.0f;
                    for (int i = 0; i < points; ++i)
                    {
                        const int sample_index = (points == 1)
                            ? 0
                            : (i * (sample_count - 1)) / (points - 1);
                        const int slot = (start + sample_index) % history.capacity;
                        const auto& sample = history.samples[slot];
                        effective_plot[i] = static_cast<float>(sample.effective_ms);
                        max_effective = std::max(max_effective, effective_plot[i]);
                        if (show_wait_plot)
                        {
                            wait_plot[i] = static_cast<float>(sample.wait_ms);
                            max_wait = std::max(max_wait, wait_plot[i]);
                        }
                        if (show_total_plot)
                        {
                            total_plot[i] = static_cast<float>(sample.total_ms);
                            max_total = std::max(max_total, total_plot[i]);
                        }
                    }
                    if (max_effective < 0.001f)
                        max_effective = 1.0f;
                    if (max_wait < 0.001f)
                        max_wait = 1.0f;
                    if (max_total < 0.001f)
                        max_total = 1.0f;

                    ImGui::TextUnformatted("Frame time history (ms)");
                    char effective_label[64];
                    std::snprintf(effective_label, sizeof(effective_label),
                        "Effective (max %.2f ms)##frametime", max_effective);
                    ImGui::PlotLines(effective_label, effective_plot.data(), points, 0, nullptr, 0.0f, max_effective, ImVec2(0, 60));
                    if (show_wait_plot)
                    {
                        char wait_label[64];
                        std::snprintf(wait_label, sizeof(wait_label),
                            "Wait (max %.2f ms)##frametime", max_wait);
                        ImGui::PlotLines(wait_label, wait_plot.data(), points, 0, nullptr, 0.0f, max_wait, ImVec2(0, 40));
                    }
                    if (show_total_plot)
                    {
                        char total_label[64];
                        std::snprintf(total_label, sizeof(total_label),
                            "Total (max %.2f ms)##frametime", max_total);
                        ImGui::PlotLines(total_label, total_plot.data(), points, 0, nullptr, 0.0f, max_total, ImVec2(0, 60));
                    }
                }
                else
                {
                    ImGui::TextDisabled("Not enough samples for plot.");
                }
            }
        }

        if (ctx.particle_monitor_stats && ctx.particle_monitor_stats->valid
            && ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const auto& stats = *ctx.particle_monitor_stats;
            ImGui::Text("Emitters: %zu (%zu visible)", stats.emitter_count, stats.visible_emitter_count);
            ImGui::Text("Live particles: %zu", stats.live_particles);
            ImGui::Text("Rendered particles: %zu", stats.rendered_particles);
            ImGui::Text("Draw batches: %zu", stats.draw_batches);
            ImGui::Text("Hit events emitted: %zu", stats.hit_events_emitted);
            ImGui::Text("Collisions requested: %s", stats.collisions_requested ? "yes" : "no");
            ImGui::Text(
                "Threaded sim: %s (%s)",
                stats.threaded_simulation_enabled ? "enabled" : "disabled",
                stats.threaded_simulation_used ? "used this frame" : "not used this frame");
        }

        if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static StorageMemorySnapshot snapshot{};
            static bool has_snapshot = false;
            static bool auto_refresh = false;
            static float refresh_interval_s = 1.0f;

            const double now_s = ImGui::GetTime();
            bool request_refresh = false;

            if (ImGui::Button("Refresh"))
                request_refresh = true;

            ImGui::SameLine();
            ImGui::Checkbox("Auto refresh", &auto_refresh);

            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputFloat("Interval (s)", &refresh_interval_s, 0.1f, 1.0f, "%.1f");
            if (refresh_interval_s < 0.1f)
                refresh_interval_s = 0.1f;

            if (auto_refresh && (now_s - snapshot.timestamp_s) >= refresh_interval_s)
                request_refresh = true;

            if (request_refresh)
            {
                snapshot = StorageMemorySnapshot{};
                snapshot.timestamp_s = now_s;

                if (ctx.resource_manager)
                {
                    auto& storage = static_cast<ResourceManager&>(*ctx.resource_manager).storage();
                    for (const auto& pool_stats : storage.pool_stats())
                    {
                        StorageMemoryRow row{};
                        const auto meta_type = entt::resolve(pool_stats.type_id);
                        row.type_name = meta::get_meta_type_id_string(meta_type);
                        row.capacity = pool_stats.capacity;
                        row.used = row.capacity - pool_stats.free_count;
                        row.elem_size = pool_stats.element_size;
                        row.bytes_total = row.capacity * row.elem_size;
                        row.bytes_used = row.used * row.elem_size;

                        snapshot.bytes_total += row.bytes_total;
                        snapshot.bytes_used += row.bytes_used;
                        snapshot.rows.push_back(row);
                    }

                    std::sort(snapshot.rows.begin(), snapshot.rows.end(),
                        [](const auto& a, const auto& b)
                        {
                            return a.bytes_used > b.bytes_used;
                        });
                }

                has_snapshot = true;
            }

            if (!ctx.resource_manager)
            {
                ImGui::TextDisabled("No resource manager available.");
            }
            else if (!has_snapshot)
            {
                ImGui::TextDisabled("No snapshot yet. Click Refresh.");
            }
            else
            {
                char used_buf[32];
                char total_buf[32];
                format_bytes(static_cast<double>(snapshot.bytes_used), used_buf, sizeof(used_buf));
                format_bytes(static_cast<double>(snapshot.bytes_total), total_buf, sizeof(total_buf));

                ImGui::Text("Storage pools: %zu", snapshot.rows.size());
                ImGui::Text("Storage bytes: %s / %s", used_buf, total_buf);
                ImGui::TextDisabled("Pool bytes only (excludes dynamic allocations inside assets).");

                if (snapshot.rows.empty())
                {
                    ImGui::TextDisabled("No storage pools found.");
                }
                else if (ImGui::BeginTable("StorageMemoryTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Type");
                    ImGui::TableSetupColumn("Used / Capacity");
                    ImGui::TableSetupColumn("Element Size", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                    ImGui::TableSetupColumn("Used Bytes", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                    ImGui::TableSetupColumn("Total Bytes", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                    ImGui::TableHeadersRow();

                    for (const auto& row : snapshot.rows)
                    {
                        char used_bytes[32];
                        char total_bytes[32];
                        format_bytes(static_cast<double>(row.bytes_used), used_bytes, sizeof(used_bytes));
                        format_bytes(static_cast<double>(row.bytes_total), total_bytes, sizeof(total_bytes));

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", row.type_name.c_str());

                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%zu / %zu", row.used, row.capacity);

                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%zu", row.elem_size);

                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%s", used_bytes);

                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%s", total_bytes);
                    }

                    ImGui::EndTable();
                }
            }
        }
    }
} // namespace eeng::gui
