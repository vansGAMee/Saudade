#pragma once

#include <saudade/events/event_types.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <saudade/time/time_types.hpp>

#include <QString>

#include <atomic>
#include <functional>
#include <span>

namespace saudade::ui {

class ProjectExporter {
public:
    using ProgressCallback = std::function<void(double)>;

    static bool render_wav(
        const renderplan::RenderPlan& plan,
        std::span<const events::TimelineEvent> events,
        double bpm,
        float master_gain_db,
        time::BeatPosition start,
        time::BeatPosition end,
        const QString& path,
        std::atomic<bool>* cancel = nullptr,
        ProgressCallback progress = {},
        QString* error = nullptr);
};

} // namespace saudade::ui
