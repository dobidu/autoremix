#pragma once
#include "IRemixEngine.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>

namespace autoremix {

class RemixRegistry {
public:
    using Factory = std::function<std::unique_ptr<IRemixEngine>()>;

    static RemixRegistry& getInstance() {
        static RemixRegistry instance;
        return instance;
    }

    void registerEngine(const std::string& id, Factory factory) {
        factories_[id] = std::move(factory);
    }

    std::unique_ptr<IRemixEngine> create(const std::string& id) const {
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
    RemixRegistry() = default;
    std::unordered_map<std::string, Factory> factories_;
};

#define AUTOREMIX_REGISTER_ENGINE(ClassName, id_str)                       \
    static bool _##ClassName##_registered = []() {                         \
        autoremix::RemixRegistry::getInstance().registerEngine(            \
            id_str,                                                         \
            []() -> std::unique_ptr<autoremix::IRemixEngine> {             \
                return std::make_unique<ClassName>();                       \
            });                                                             \
        return true;                                                        \
    }();

} // namespace autoremix
