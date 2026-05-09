#include "IRemixEngine.h"
#include "RemixRegistry.h"

namespace autoremix {

class DrumAndBassEngine : public IRemixEngine {
public:
    DrumAndBassEngine() = default;
    std::string getId() const override { return "drum_and_bass"; }
    std::string getDisplayName() const override { return "Drum and Bass"; }
    RemixParams getDefaultParams() const override {
        RemixParams p; p.drums_tempo_factor = 2.0f; p.bass_boost_db = 6.0f; return p;
    }
    ProcessResult process(const StemPaths&, const RemixParams&,
                          const std::filesystem::path&) override { return {}; }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumAndBassEngine)
};

AUTOREMIX_REGISTER_ENGINE(DrumAndBassEngine, "drum_and_bass")

}  // namespace autoremix
