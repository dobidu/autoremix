#include "IRemixEngine.h"
#include "RemixRegistry.h"

namespace autoremix {

class ChoppedAndScrewedEngine : public IRemixEngine {
public:
    ChoppedAndScrewedEngine() = default;
    std::string getId() const override { return "chopped_screwed"; }
    std::string getDisplayName() const override { return "Chopped & Screwed"; }
    RemixParams getDefaultParams() const override {
        RemixParams p; p.tempo_factor = 0.7f; p.pitch_shift_semi = -4.0f; return p;
    }
    ProcessResult process(const StemPaths&, const RemixParams&,
                          const std::filesystem::path&) override { return {}; }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChoppedAndScrewedEngine)
};

AUTOREMIX_REGISTER_ENGINE(ChoppedAndScrewedEngine, "chopped_screwed")

}  // namespace autoremix
