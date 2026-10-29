#pragma once
#include "OptiLabParameterCatalog.h"
#include <array>
#include <string_view>

namespace optilab_internal {
using namespace optilab_parameters;
struct Descriptor {
    const char* key;
    bool integer;
    optilab_parameters::Hook hook;
    optilab_parameters::ProfileValues defaults;
};
enum class Index {
#define OP_INTERNAL(type, member, initial, ...) member,
#include "OptiLabInternalState.def"
#undef OP_INTERNAL
    Count
};
inline constexpr std::size_t count = std::size_t(Index::Count);
inline constexpr std::array<Descriptor, count> catalog {{
#define OP_INTERNAL(type, member, initial, key, integer, hook, defaults) Descriptor{key, integer, hook, defaults},
#include "OptiLabInternalState.def"
#undef OP_INTERNAL
}};
constexpr const Descriptor& get(Index index) noexcept { return catalog[std::size_t(index)]; }
inline constexpr bool contains(std::string_view key) noexcept {
    for (const auto& d : catalog) if (key == d.key) return true;
    return false;
}
}
