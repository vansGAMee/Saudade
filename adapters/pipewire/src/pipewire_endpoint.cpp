#include <saudade/pipewire/pipewire_endpoint.hpp>

#include <pipewire/pipewire.h>
#include <pipewire/filter.h>
#include <spa/param/audio/format-utils.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <set>

namespace saudade::pipewire {

namespace {

#include <pipewire/extensions/metadata.h>

struct PortCandidate {
    uint32_t id{0};
    std::string dir;
    std::string channel;
    std::string name;
    std::string path;
    uint32_t node_id{0};
    bool physical{false};
};

struct RegistryContext;

static int on_metadata_property(void* data, uint32_t /*subject*/, const char* key, const char* /*type*/, const char* value);

static const struct pw_metadata_events metadata_events = {
    .version = PW_VERSION_METADATA_EVENTS,
    .property = on_metadata_property,
};

struct RegistryContext {
    struct pw_core* core{nullptr};
    struct pw_filter* filter{nullptr};
    struct pw_registry* registry{nullptr};
    std::string app_name;
    std::vector<PortCandidate> candidates;
    std::unordered_map<uint32_t, std::string> node_names;
    std::string default_sink_name;
    struct pw_metadata* metadata{nullptr};
    struct spa_hook metadata_listener{};
    std::set<std::pair<uint32_t, uint32_t>> linked_pairs;

    void try_link() {
        const uint32_t our_node_id = pw_filter_get_node_id(filter);

        uint32_t our_fl = 0;
        uint32_t our_fr = 0;

        // First find our own output ports
        for (const auto& p : candidates) {
            if (p.dir == "out") {
                const bool is_ours = (our_node_id != SPA_ID_INVALID && p.node_id == our_node_id) ||
                                     (!p.path.empty() && p.path.find(app_name) != std::string::npos);
                if (is_ours) {
                    if (p.name == "output_FL" || p.channel == "FL") {
                        our_fl = p.id;
                    } else if (p.name == "output_FR" || p.channel == "FR") {
                        our_fr = p.id;
                    }
                }
            }
        }

        if (our_fl == 0 || our_fr == 0) return;

        // Group physical sink inputs by node_id
        std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> sink_node_ports;
        for (const auto& p : candidates) {
            if (p.dir == "in" && p.physical) {
                if (p.channel == "FL" || p.name.find("playback_FL") != std::string::npos) {
                    sink_node_ports[p.node_id].first = p.id;
                } else if (p.channel == "FR" || p.name.find("playback_FR") != std::string::npos) {
                    sink_node_ports[p.node_id].second = p.id;
                }
            }
        }

        for (const auto& [node_id, ports] : sink_node_ports) {
            (void)node_id;
            const uint32_t sink_fl = ports.first;
            const uint32_t sink_fr = ports.second;

            if (sink_fl != 0 && sink_fr != 0) {
                if (!linked_pairs.contains({our_fl, sink_fl})) {
                    linked_pairs.insert({our_fl, sink_fl});
                    struct pw_properties* props_l = pw_properties_new(
                        PW_KEY_LINK_OUTPUT_PORT, std::to_string(our_fl).c_str(),
                        PW_KEY_LINK_INPUT_PORT, std::to_string(sink_fl).c_str(),
                        PW_KEY_OBJECT_LINGER, "false",
                        nullptr
                    );
                    pw_core_create_object(core, "link-factory", PW_TYPE_INTERFACE_Link, PW_VERSION_LINK, &props_l->dict, 0);
                }

                if (!linked_pairs.contains({our_fr, sink_fr})) {
                    linked_pairs.insert({our_fr, sink_fr});
                    struct pw_properties* props_r = pw_properties_new(
                        PW_KEY_LINK_OUTPUT_PORT, std::to_string(our_fr).c_str(),
                        PW_KEY_LINK_INPUT_PORT, std::to_string(sink_fr).c_str(),
                        PW_KEY_OBJECT_LINGER, "false",
                        nullptr
                    );
                    pw_core_create_object(core, "link-factory", PW_TYPE_INTERFACE_Link, PW_VERSION_LINK, &props_r->dict, 0);
                }
            }
        }
    }
};

static int on_metadata_property(void* data, uint32_t /*subject*/, const char* key, const char* /*type*/, const char* value) {
    auto* ctx = static_cast<RegistryContext*>(data);
    if (key && std::strcmp(key, "default.audio.sink") == 0 && value) {
        std::string val(value);
        const auto pos = val.find("\"name\":\"");
        if (pos != std::string::npos) {
            const size_t start = pos + 8;
            const size_t end = val.find('"', start);
            if (end != std::string::npos) {
                ctx->default_sink_name = val.substr(start, end - start);
                ctx->try_link();
            }
        }
    }
    return 0;
}

void on_registry_global(void* data, uint32_t id, uint32_t /*permissions*/,
                        const char* type, uint32_t /*version*/,
                        const struct spa_dict* props) {
    auto* ctx = static_cast<RegistryContext*>(data);
    if (!props || !type) return;

    if (std::strcmp(type, PW_TYPE_INTERFACE_Node) == 0) {
        const char* name = spa_dict_lookup(props, PW_KEY_NODE_NAME);
        if (name) {
            ctx->node_names[id] = name;
            ctx->try_link();
        }
    } else if (std::strcmp(type, PW_TYPE_INTERFACE_Metadata) == 0) {
        const char* name = spa_dict_lookup(props, PW_KEY_METADATA_NAME);
        if (name && std::strcmp(name, "default") == 0 && !ctx->metadata && ctx->registry) {
            ctx->metadata = static_cast<struct pw_metadata*>(
                pw_registry_bind(ctx->registry, id, type, PW_VERSION_METADATA, 0)
            );
            if (ctx->metadata) {
                pw_metadata_add_listener(ctx->metadata, &ctx->metadata_listener, &metadata_events, ctx);
            }
        }
    } else if (std::strcmp(type, PW_TYPE_INTERFACE_Port) == 0) {
        const char* dir = spa_dict_lookup(props, PW_KEY_PORT_DIRECTION);
        const char* node_id_str = spa_dict_lookup(props, PW_KEY_NODE_ID);
        const char* channel = spa_dict_lookup(props, PW_KEY_AUDIO_CHANNEL);
        const char* name = spa_dict_lookup(props, PW_KEY_PORT_NAME);
        const char* path = spa_dict_lookup(props, "object.path");
        const char* physical = spa_dict_lookup(props, "port.physical");

        PortCandidate cand;
        cand.id = id;
        cand.dir = dir ? dir : "";
        cand.channel = channel ? channel : "";
        cand.name = name ? name : "";
        cand.path = path ? path : "";
        cand.node_id = node_id_str ? static_cast<uint32_t>(std::strtoul(node_id_str, nullptr, 10)) : 0;
        cand.physical = physical && std::strcmp(physical, "true") == 0;

        ctx->candidates.push_back(std::move(cand));
        ctx->try_link();
    }
}

const struct pw_registry_events registry_events = {
    .version = PW_VERSION_REGISTRY_EVENTS,
    .global = on_registry_global,
    .global_remove = nullptr,
};

} // namespace

PipeWireEndpoint::PipeWireEndpoint(audio::AudioEngine& engine, std::string app_name)
    : engine_(engine), app_name_(std::move(app_name)) {
    pw_init(nullptr, nullptr);
}

PipeWireEndpoint::~PipeWireEndpoint() {
    stop();
    pw_deinit();
}

void PipeWireEndpoint::on_process_thunk(void* userdata, struct spa_io_position* position) {
    auto* self = static_cast<PipeWireEndpoint*>(userdata);
    if (self) {
        self->handle_process(position);
    }
}

void PipeWireEndpoint::notify_state_changed() {
    if (reg_ctx_) {
        static_cast<RegistryContext*>(reg_ctx_)->try_link();
    }
}

namespace {
void on_state_changed_thunk(void* userdata, enum pw_filter_state /*old*/,
                            enum pw_filter_state /*state*/, const char* /*error*/) {
    auto* self = static_cast<PipeWireEndpoint*>(userdata);
    if (self) {
        self->notify_state_changed();
    }
}
} // namespace

void PipeWireEndpoint::handle_process(struct spa_io_position* position) noexcept {
    if (!position || !running_.load(std::memory_order_relaxed)) {
        return;
    }

    const uint32_t n_samples = static_cast<uint32_t>(position->clock.duration);
    if (n_samples == 0) {
        return;
    }

    const double sr = position->clock.rate.denom > 0 ?
        (static_cast<double>(position->clock.rate.denom) / static_cast<double>(position->clock.rate.num)) :
        48000.0;

    sample_rate_.store(sr, std::memory_order_relaxed);
    quantum_.store(n_samples, std::memory_order_relaxed);

    float* out_l = static_cast<float*>(pw_filter_get_dsp_buffer(port_left_, n_samples));
    float* out_r = static_cast<float*>(pw_filter_get_dsp_buffer(port_right_, n_samples));

    if (out_l && out_r) {
        channel_ptrs_[0] = out_l;
        channel_ptrs_[1] = out_r;
        audio::AudioBlock output_block(channel_ptrs_, 2, n_samples);
        audio::ProcessContext ctx{sr, n_samples};

        engine_.process(output_block, ctx);
        streaming_.store(true, std::memory_order_release);
        process_count_.fetch_add(1, std::memory_order_relaxed);
    } else {
        if (out_l) std::memset(out_l, 0, n_samples * sizeof(float));
        if (out_r) std::memset(out_r, 0, n_samples * sizeof(float));
    }
}

void PipeWireEndpoint::start() {
    if (running_.load(std::memory_order_acquire)) {
        return;
    }
    running_.store(true, std::memory_order_release);
    streaming_.store(false, std::memory_order_release);
    process_count_.store(0, std::memory_order_relaxed);

    loop_thread_ = std::thread([this]() { loop_thread_fn(); });
}

void PipeWireEndpoint::loop_thread_fn() {
    loop_ = pw_main_loop_new(nullptr);
    if (!loop_) {
        running_.store(false, std::memory_order_release);
        return;
    }

    struct pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_NODE_NAME, app_name_.c_str(),
        PW_KEY_NODE_DESCRIPTION, "Saudade Audio Proof",
        PW_KEY_NODE_AUTOCONNECT, "true",
        nullptr
    );

    static const struct pw_filter_events filter_events = {
        .version = PW_VERSION_FILTER_EVENTS,
        .destroy = nullptr,
        .state_changed = on_state_changed_thunk,
        .io_changed = nullptr,
        .param_changed = nullptr,
        .add_buffer = nullptr,
        .remove_buffer = nullptr,
        .process = on_process_thunk,
        .drained = nullptr,
        .command = nullptr,
    };

    filter_ = pw_filter_new_simple(
        pw_main_loop_get_loop(loop_),
        app_name_.c_str(),
        props,
        &filter_events,
        this
    );

    if (!filter_) {
        pw_main_loop_destroy(loop_);
        loop_ = nullptr;
        running_.store(false, std::memory_order_release);
        return;
    }

    port_left_ = pw_filter_add_port(
        filter_,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        0,
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "output_FL",
            PW_KEY_AUDIO_CHANNEL, "FL",
            nullptr
        ),
        nullptr, 0
    );

    port_right_ = pw_filter_add_port(
        filter_,
        PW_DIRECTION_OUTPUT,
        PW_FILTER_PORT_FLAG_MAP_BUFFERS,
        0,
        pw_properties_new(
            PW_KEY_FORMAT_DSP, "32 bit float mono audio",
            PW_KEY_PORT_NAME, "output_FR",
            PW_KEY_AUDIO_CHANNEL, "FR",
            nullptr
        ),
        nullptr, 0
    );

    int res = pw_filter_connect(filter_, PW_FILTER_FLAG_RT_PROCESS, nullptr, 0);
    if (res < 0) {
        pw_filter_destroy(filter_);
        pw_main_loop_destroy(loop_);
        filter_ = nullptr;
        loop_ = nullptr;
        running_.store(false, std::memory_order_release);
        return;
    }

    // Set up Registry listener for automatic port linking to default sink
    struct pw_core* core = pw_filter_get_core(filter_);
    RegistryContext reg_ctx;
    reg_ctx.core = core;
    reg_ctx.filter = filter_;
    reg_ctx.app_name = app_name_;
    reg_ctx_ = &reg_ctx;

    struct pw_registry* registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
    reg_ctx.registry = registry;
    struct spa_hook registry_listener{};
    pw_registry_add_listener(registry, &registry_listener, &registry_events, &reg_ctx);

    // Run the main loop until stop() is called
    pw_main_loop_run(loop_);

    // Teardown
    reg_ctx_ = nullptr;
    if (reg_ctx.metadata) {
        spa_hook_remove(&reg_ctx.metadata_listener);
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(reg_ctx.metadata));
    }
    spa_hook_remove(&registry_listener);
    pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(registry));

    pw_filter_destroy(filter_);
    pw_main_loop_destroy(loop_);
    filter_ = nullptr;
    loop_ = nullptr;
}

void PipeWireEndpoint::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (loop_) {
        pw_main_loop_quit(loop_);
    }
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
    streaming_.store(false, std::memory_order_release);
}

bool PipeWireEndpoint::is_running() const noexcept {
    return running_.load(std::memory_order_acquire);
}

double PipeWireEndpoint::sample_rate() const noexcept {
    return sample_rate_.load(std::memory_order_relaxed);
}

uint32_t PipeWireEndpoint::quantum() const noexcept {
    return quantum_.load(std::memory_order_relaxed);
}

bool PipeWireEndpoint::wait_for_stream(uint32_t timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (streaming_.load(std::memory_order_acquire) && process_count_.load(std::memory_order_relaxed) > 0) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return streaming_.load(std::memory_order_acquire);
}

} // namespace saudade::pipewire
