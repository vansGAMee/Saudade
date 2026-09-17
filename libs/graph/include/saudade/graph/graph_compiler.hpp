#pragma once

#include <saudade/graph/graph_model.hpp>
#include <saudade/renderplan/render_plan.hpp>
#include <memory>
#include <string>
#include <stdexcept>

namespace saudade::graph {

class GraphCompilationException : public std::runtime_error {
public:
    explicit GraphCompilationException(const std::string& message)
        : std::runtime_error(message) {}
};

/// Validates an editable GraphModel and compiles it into an immutable RenderPlan.
class GraphCompiler {
public:
    /// Compiles graph into an immutable RenderPlan.
    /// Throws GraphCompilationException if validation fails.
    [[nodiscard]] static std::shared_ptr<renderplan::RenderPlan> compile(const GraphModel& graph);
};

} // namespace saudade::graph
