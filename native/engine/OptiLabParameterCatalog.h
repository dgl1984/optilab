#pragma once
#include "OptiLabStereoType.h"
#include "OptiLabCoreProfiles.h"
#include <array>
#include <initializer_list>
#include <string_view>
#include <utility>
namespace optilab_parameters {
enum class Exposure { Runtime, Internal };
enum class Hook { None, Keep, LatencyProfile, ClipperStyle };
enum class Policy : unsigned { None=0, Structural=1, Latency=2, StreamAuthored=4 };
constexpr Policy operator|(Policy a, Policy b) { return Policy(unsigned(a)|unsigned(b)); }
constexpr bool has(Policy p, Policy flag) { return (unsigned(p)&unsigned(flag))!=0; }
struct ProfileValues {
    double base; std::array<double, profileSlots> modes;
    constexpr double forMode(int mode) const noexcept { return mode>=0 && mode<int(modes.size())?modes[std::size_t(mode)]:base; }
};
constexpr ProfileValues profileDefaults(double base, std::initializer_list<std::pair<CoreMode,double>> overrides={}) {
    ProfileValues result{base,{}};result.modes.fill(base);
    for(const auto& e:overrides) result.modes[std::size_t(e.first)]=e.second;
    return result;
}
struct Descriptor {
    const char* key; double minimum,maximum; bool integer; Exposure exposure;
    Policy policy; Hook hook; ProfileValues defaults;
    constexpr bool structural() const { return exposure==Exposure::Internal || has(policy,Policy::Structural); }
    constexpr bool latencyAffecting() const { return has(policy,Policy::Latency); }
};
enum class Index {
#define OP_PARAMETER(type, member, initial, ...) member,
#include "OptiLabParameters.def"
#undef OP_PARAMETER
    Count
};
inline constexpr std::size_t coreCount=std::size_t(Index::Count);
inline constexpr std::array<Descriptor,coreCount> catalog {{
#define OP_PARAMETER(type, member, initial, ...) Descriptor{__VA_ARGS__},
#include "OptiLabParameters.def"
#undef OP_PARAMETER
}};
constexpr const Descriptor& get(Index index) noexcept { return catalog[std::size_t(index)]; }
constexpr bool validCatalog() {
    for(std::size_t i=0;i<coreCount;++i) {
        const auto& d=catalog[i]; if(d.minimum>d.maximum) return false;
        for(const auto& f:profiles) { const auto v=d.defaults.forMode(int(f.mode)); if(v<d.minimum || v>d.maximum) return false; }
        for(std::size_t j=0;j<i;++j) if(std::string_view(d.key)==catalog[j].key) return false;
    }
    return true;
}
static_assert(validCatalog());
}
