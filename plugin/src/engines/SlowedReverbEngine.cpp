#include "IRemixEngine.h"
#include "RemixRegistry.h"

namespace autoremix {

class SlowedReverbEngine : public IRemixEngine {
public:
    SlowedReverbEngine() = default;
    std::string getId() const override { return "slowed_reverb"; }
    std::string getDisplayName() const override { return "Slowed + Reverb"; }
    RemixParams getDefaultParams() const override {
        RemixParams p; p.tempo_factor = 0.75f; p.reverb_mix = 0.6f; return p;
    }
    ProcessResult process(const StemPaths&, const RemixParams&,
                          const std::filesystem::path&) override { return {}; }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowedReverbEngine)
};

AUTOREMIX_REGISTER_ENGINE(SlowedReverbEngine, "slowed_reverb")

}  // namespace autoremix
