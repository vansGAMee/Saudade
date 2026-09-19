#pragma once

#include <saudade/model/project.hpp>

#include <QString>

namespace saudade::ui {

/// Qt/filesystem boundary adapter for the versioned Saudade project document.
/// The canonical model remains framework-independent.
class ProjectFile {
public:
    static constexpr int kCurrentVersion = 2;

    static bool save(const model::Project& project, const QString& path,
                     QString* error = nullptr);
    static bool load(const QString& path, model::Project& project,
                     QString* error = nullptr);
};

} // namespace saudade::ui
