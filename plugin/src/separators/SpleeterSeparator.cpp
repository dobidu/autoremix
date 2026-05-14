#include "IStemSeparator.h"
#include "SeparatorRegistry.h"

namespace autoremix {

class SpleeterSeparator : public IStemSeparator {
public:
    SpleeterSeparator() = default;
    std::string getId() const override { return "spleeter_4stems"; }
    std::string getDisplayName() const override { return "Spleeter 4 Stems (ML)"; }
    bool isAvailable() const override { return false; }
    StemPaths separate(const std::filesystem::path&,
                       const std::filesystem::path&) override { return {}; }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpleeterSeparator)
};

AUTOREMIX_REGISTER_SEPARATOR(SpleeterSeparator, "spleeter_4stems")

}  // namespace autoremix
