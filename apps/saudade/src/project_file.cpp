#include <saudade/ui/project_file.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace saudade::ui {
namespace {

QJsonObject note_to_json(const model::Note& note) {
    return {
        {"id", QString::number(note.note_id)},
        {"startTicks", QString::number(note.start.ticks)},
        {"durationTicks", QString::number(note.duration.ticks)},
        {"pitch", note.pitch},
        {"velocity", note.velocity},
        {"releaseVelocity", note.release_velocity},
    };
}

bool read_i64(const QJsonObject& object, const char* key, int64_t& value) {
    bool ok = false;
    value = object.value(QLatin1String(key)).toString().toLongLong(&ok);
    return ok;
}

bool fail(QString* error, const QString& message) {
    if (error) {
        *error = message;
    }
    return false;
}

} // namespace

bool ProjectFile::save(const model::Project& project, const QString& path,
                       QString* error) {
    QJsonObject root{
        {"format", "saudade-project"},
        {"version", kCurrentVersion},
        {"name", QString::fromStdString(project.name())},
        {"bpm", project.bpm()},
        {"masterGainDb", project.master_gain_db()},
    };

    QJsonArray tracks;
    for (const auto& track : project.tracks()) {
        tracks.append(QJsonObject{
            {"id", QString::number(track.id)},
            {"name", QString::fromStdString(track.name)},
            {"gainDb", track.mixer.gain_db},
            {"pan", track.mixer.pan},
            {"muted", track.mixer.muted},
            {"solo", track.mixer.solo},
        });
    }
    root["tracks"] = tracks;

    const auto& patch = project.synth_patch();
    root["synthPatch"] = QJsonObject{
        {"attack", patch.attack_seconds},
        {"decay", patch.decay_seconds},
        {"sustain", patch.sustain},
        {"release", patch.release_seconds},
        {"cutoff", patch.cutoff_hz},
        {"resonance", patch.resonance},
        {"character", patch.character},
    };

    QJsonArray patterns;
    for (const auto& pattern : project.patterns()) {
        QJsonObject pattern_object{
            {"id", QString::number(pattern.id())},
            {"name", QString::fromStdString(pattern.name())},
            {"lengthTicks", QString::number(pattern.length().ticks)},
        };
        QJsonArray lanes;
        for (const auto& lane : pattern.lanes()) {
            QJsonArray notes;
            for (const auto& note : lane.notes().notes()) {
                notes.append(note_to_json(note));
            }
            lanes.append(QJsonObject{
                {"id", QString::number(lane.id())},
                {"name", QString::fromStdString(lane.name())},
                {"notes", notes},
            });
        }
        pattern_object["lanes"] = lanes;
        patterns.append(pattern_object);
    }
    root["patterns"] = patterns;

    QJsonArray clips;
    for (const auto& clip : project.clips()) {
        clips.append(QJsonObject{
            {"id", QString::number(clip.id)},
            {"trackId", QString::number(clip.track_id)},
            {"patternId", QString::number(clip.pattern_id)},
            {"startTicks", QString::number(clip.start.ticks)},
            {"durationTicks", QString::number(clip.duration.ticks)},
            {"patternOffsetTicks", QString::number(clip.pattern_offset.ticks)},
        });
    }
    root["clips"] = clips;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(error, file.errorString());
    }
    const auto bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        return fail(error, file.errorString());
    }
    return true;
}

bool ProjectFile::load(const QString& path, model::Project& project,
                       QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(error, file.errorString());
    }
    QJsonParseError parse_error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(error, QStringLiteral("Invalid project document: %1")
                               .arg(parse_error.errorString()));
    }
    const auto root = document.object();
    const int version = root.value("version").toInt();
    if (root.value("format").toString() != "saudade-project" ||
        version < 1 || version > kCurrentVersion) {
        return fail(error, QStringLiteral("Unsupported Saudade project version"));
    }

    model::Project loaded;
    loaded.clear();
    loaded.set_name(root.value("name").toString("Untitled").toStdString());
    if (!loaded.set_bpm(root.value("bpm").toDouble(120.0))) {
        return fail(error, QStringLiteral("Project contains an invalid tempo"));
    }
    loaded.set_master_gain_db(
        static_cast<float>(root.value("masterGainDb").toDouble(0.0)));
    if (version >= 2) {
        const auto patch_object = root.value("synthPatch").toObject();
        model::SynthPatch patch{
            .attack_seconds = static_cast<float>(patch_object.value("attack").toDouble(0.008)),
            .decay_seconds = static_cast<float>(patch_object.value("decay").toDouble(0.20)),
            .sustain = static_cast<float>(patch_object.value("sustain").toDouble(0.65)),
            .release_seconds = static_cast<float>(patch_object.value("release").toDouble(0.12)),
            .cutoff_hz = static_cast<float>(patch_object.value("cutoff").toDouble(3200.0)),
            .resonance = static_cast<float>(patch_object.value("resonance").toDouble(0.18)),
            .character = static_cast<float>(patch_object.value("character").toDouble(0.78)),
        };
        loaded.set_synth_patch(patch);
    }

    for (const auto track_value : root.value("tracks").toArray()) {
        const auto object = track_value.toObject();
        bool ok = false;
        const auto id = object.value("id").toString().toULongLong(&ok);
        model::Track track{
            .id = id,
            .name = object.value("name").toString("Instrument").toStdString(),
            .mixer = {
                .gain_db = static_cast<float>(object.value("gainDb").toDouble()),
                .pan = static_cast<float>(object.value("pan").toDouble()),
                .muted = object.value("muted").toBool(),
                .solo = object.value("solo").toBool(),
            },
        };
        if (!ok || !loaded.add_track(std::move(track))) {
            return fail(error, QStringLiteral("Invalid or duplicate track"));
        }
    }

    for (const auto pattern_value : root.value("patterns").toArray()) {
        const auto object = pattern_value.toObject();
        bool id_ok = false;
        const auto id = object.value("id").toString().toULongLong(&id_ok);
        int64_t length_ticks = 0;
        if (!id_ok || !read_i64(object, "lengthTicks", length_ticks)) {
            return fail(error, QStringLiteral("Invalid pattern identity"));
        }
        model::Pattern pattern(
            id, object.value("name").toString("Pattern").toStdString(),
            time::BeatDuration::from_ticks(length_ticks));
        for (const auto lane_value : object.value("lanes").toArray()) {
            const auto lane_object = lane_value.toObject();
            bool lane_ok = false;
            const auto lane_id = lane_object.value("id").toString().toULongLong(&lane_ok);
            model::PatternLane lane(
                lane_id, lane_object.value("name").toString("Notes").toStdString());
            if (!lane_ok) {
                return fail(error, QStringLiteral("Invalid lane identity"));
            }
            for (const auto note_value : lane_object.value("notes").toArray()) {
                const auto note_object = note_value.toObject();
                bool note_id_ok = false;
                const auto note_id = note_object.value("id").toString().toULongLong(&note_id_ok);
                int64_t start_ticks = 0;
                int64_t duration_ticks = 0;
                model::Note note{
                    .note_id = note_id,
                    .pitch = note_object.value("pitch").toDouble(),
                    .velocity = static_cast<float>(note_object.value("velocity").toDouble()),
                    .release_velocity = static_cast<float>(
                        note_object.value("releaseVelocity").toDouble()),
                };
                if (!note_id_ok || !read_i64(note_object, "startTicks", start_ticks) ||
                    !read_i64(note_object, "durationTicks", duration_ticks)) {
                    return fail(error, QStringLiteral("Invalid note identity or timing"));
                }
                note.start = time::BeatPosition::from_ticks(start_ticks);
                note.duration = time::BeatDuration::from_ticks(duration_ticks);
                if (!lane.notes().add_note(note)) {
                    return fail(error, QStringLiteral("Invalid or duplicate note"));
                }
            }
            if (!pattern.add_lane(std::move(lane))) {
                return fail(error, QStringLiteral("Invalid or duplicate lane"));
            }
        }
        if (!loaded.add_pattern(std::move(pattern))) {
            return fail(error, QStringLiteral("Invalid or duplicate pattern"));
        }
    }

    for (const auto clip_value : root.value("clips").toArray()) {
        const auto object = clip_value.toObject();
        bool id_ok = false;
        bool track_ok = false;
        bool pattern_ok = false;
        int64_t start_ticks = 0;
        int64_t duration_ticks = 0;
        int64_t offset_ticks = 0;
        model::ClipInstance clip{
            .id = object.value("id").toString().toULongLong(&id_ok),
            .pattern_id = object.value("patternId").toString().toULongLong(&pattern_ok),
            .track_id = object.value("trackId").toString().toULongLong(&track_ok),
        };
        if (!id_ok || !track_ok || !pattern_ok ||
            !read_i64(object, "startTicks", start_ticks) ||
            !read_i64(object, "durationTicks", duration_ticks) ||
            !read_i64(object, "patternOffsetTicks", offset_ticks)) {
            return fail(error, QStringLiteral("Invalid clip identity or timing"));
        }
        clip.start = time::BeatPosition::from_ticks(start_ticks);
        clip.duration = time::BeatDuration::from_ticks(duration_ticks);
        clip.pattern_offset = time::BeatPosition::from_ticks(offset_ticks);
        if (!loaded.add_clip(clip)) {
            return fail(error, QStringLiteral("Invalid clip reference"));
        }
    }

    if (loaded.tracks().empty() || loaded.patterns().empty()) {
        return fail(error, QStringLiteral("Project has no editable instrument content"));
    }
    project = std::move(loaded);
    return true;
}

} // namespace saudade::ui
