#include "core/modules.h"

#include <Windows.h>
#include <map>

namespace core {

std::uintptr_t GetModule(const std::string &module) {
    static std::map<std::string, std::uintptr_t> cache;

    auto it = cache.find(module);
    if (it != cache.end()) {
        return it->second;
    }

    auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA(module.c_str()));
    cache.insert({module, base});
    return base;
}

} // namespace core
