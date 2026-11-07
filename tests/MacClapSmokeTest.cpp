// Load the delivered CLAP binary rather than another copy of the processor.
#include <clap/clap.h>
#include <clap/ext/params.h>
#include <dlfcn.h>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static const void* extension(const clap_host_t*, const char*) { return nullptr; }
static void request(const clap_host_t*) {}
static uint32_t eventCount(const clap_input_events_t*) { return 0; }
static const clap_event_header_t* eventAt(const clap_input_events_t*, uint32_t) { return nullptr; }
static bool output(const clap_output_events_t*, const clap_event_header_t*) { return true; }
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected bundle path");
        const std::string binary = std::string(argv[1]) + "/Contents/MacOS/OptiLab Core";
        void* library = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!library) throw std::runtime_error(dlerror());
        auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(library, "clap_entry"));
        require(entry && entry->init(argv[1]), "CLAP entry init failed");
        auto* factory = static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        require(factory && factory->get_plugin_count(factory) == 1, "Expected one CLAP plugin");
        auto* desc = factory->get_plugin_descriptor(factory, 0);
        require(desc && std::strcmp(desc->id, "com.lanesaudio.optilab-core") == 0, "CLAP identity changed");
        clap_host_t host{CLAP_VERSION, nullptr, "Core smoke test", "Lanes Audio", "", "1", extension, request, request, request};
        auto* plugin = factory->create_plugin(factory, &host, desc->id);
        require(plugin && plugin->init(plugin), "CLAP plugin init failed");
        auto* params = static_cast<const clap_plugin_params_t*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
        require(params && params->count(plugin) == 3, "Expected the same three host controls");
        require(plugin->activate(plugin, 48000.0, 64, 64), "CLAP activation failed");
        require(plugin->start_processing(plugin), "CLAP start failed");
        std::array<float, 64> left{}, right{}, outLeft{}, outRight{};
        left[0] = right[0] = 0.25f;
        float* in[] = {left.data(), right.data()};
        float* out[] = {outLeft.data(), outRight.data()};
        clap_audio_buffer_t input{}, result{};
        input.data32 = in; input.channel_count = 2;
        result.data32 = out; result.channel_count = 2;
        clap_input_events_t events{nullptr, eventCount, eventAt};
        clap_output_events_t outputs{nullptr, output};
        clap_process_t block{};
        block.frames_count = 64; block.audio_inputs = &input; block.audio_inputs_count = 1;
        block.audio_outputs = &result; block.audio_outputs_count = 1;
        block.in_events = &events; block.out_events = &outputs;
        for (int n = 0; n < 32; ++n) {
            block.steady_time = n * 64;
            require(plugin->process(plugin, &block) != CLAP_PROCESS_ERROR, "CLAP processing failed");
            for (float v : outLeft) require(std::isfinite(v), "Non-finite CLAP audio");
            for (float v : outRight) require(std::isfinite(v), "Non-finite CLAP audio");
        }
        plugin->stop_processing(plugin); plugin->deactivate(plugin); plugin->destroy(plugin);
        entry->deinit(); dlclose(library);
        std::cout << "Mac CLAP load, parameters and processing passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
