#include "IStemSeparator.h"
#include "SeparatorRegistry.h"

namespace autoremix {

class AlgorithmicSeparator : public IStemSeparator {
public:
    AlgorithmicSeparator() = default;
    std::string getId() const override { return "algorithmic"; }
    std::string getDisplayName() const override { return "Algorithmic (FFT)"; }
    bool isAvailable() const override { return true; }
    StemPaths separate(const std::filesystem::path&,
                       const std::filesystem::path&) override { return {}; }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmicSeparator)
};

AUTOREMIX_REGISTER_SEPARATOR(AlgorithmicSeparator, "algorithmic")

}  // namespace autoremix
