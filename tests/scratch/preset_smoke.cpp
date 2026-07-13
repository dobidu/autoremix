// Smoke test for NativePresetLoaders: loads the built-in presets embedded in
// AutoRemixPresetsData, checks routing (remix vs mashup), id presence, and
// that every effect step survived op-name validation.
//
// Build: cmake --build build --target preset_smoke
// Run:   ./build/tests/scratch/preset_smoke

#include <cstdio>
#include <cstdlib>

#include "dsp/NativePresetLoaders.h"

int main()
{
    using namespace autoremix::dsp;

    const auto remix  = presets::load_remix_presets();
    const auto mashup = presets::load_mashup_presets();

    std::printf("remix presets:  %zu\n", remix.size());
    std::printf("mashup presets: %zu\n", mashup.size());

    bool ok = true;

    if (remix.size() < 16) {
        std::printf("FAIL: expected >= 16 built-in remix presets\n");
        ok = false;
    }
    if (mashup.size() < 8) {
        std::printf("FAIL: expected >= 8 built-in mashup presets\n");
        ok = false;
    }

    for (const auto& p : remix) {
        if (p.id.empty()) {
            std::printf("FAIL: remix preset with empty id\n");
            ok = false;
        }
        for (const auto& step : p.effects) {
            if (!engines::is_known_op(step.op)) {
                std::printf("FAIL: %s carries unknown op %s past load\n",
                            p.id.c_str(), step.op.c_str());
                ok = false;
            }
        }
        std::printf("  remix  %-28s engine=%-16s effects=%zu structure=%s\n",
                    p.id.c_str(), p.engine.c_str(), p.effects.size(),
                    p.structure.enabled ? "yes" : "no");
    }
    for (const auto& p : mashup) {
        if (p.id.empty()) {
            std::printf("FAIL: mashup preset with empty id\n");
            ok = false;
        }
        std::printf("  mashup %-28s bpm_mode=%s\n",
                    p.id.c_str(), p.target_bpm_mode.c_str());
    }

    std::printf(ok ? "PRESET SMOKE OK\n" : "PRESET SMOKE FAILED\n");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
