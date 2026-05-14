#pragma once
#include "IStemSeparator.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

namespace autoremix {

class SeparatorRegistry {
public:
    using Factory = std::function<std::unique_ptr<IStemSeparator>()>;

    static SeparatorRegistry& getInstance() {
        static SeparatorRegistry instance;
        return instance;
    }

    void registerSeparator(const std::string& id, Factory factory) {
        factories_[id] = std::move(factory);
    }

    std::unique_ptr<IStemSeparator> create(const std::string& id) const {
        auto it = factories_.find(id);
        if (it == factories_.end()) return nullptr;
        return it->second();
    }

    std::vector<std::string> getRegisteredIds() const {
        std::vector<std::string> ids;
        for (auto& [id, _] : factories_) ids.push_back(id);
        return ids;
    }

private:
    SeparatorRegistry() = default;
    std::unordered_map<std::string, Factory> factories_;
};

// Convenience macro for self-registration at static init time
#define AUTOREMIX_REGISTER_SEPARATOR(ClassName, id_str)                    \
    static bool _##ClassName##_registered = []() {                         \
        autoremix::SeparatorRegistry::getInstance().registerSeparator(     \
            id_str,                                                         \
            []() -> std::unique_ptr<autoremix::IStemSeparator> {           \
                return std::make_unique<ClassName>();                       \
            });                                                             \
        return true;                                                        \
    }();

} // namespace autoremix
