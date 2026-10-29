#pragma once
#include <array>
#include <cstddef>
namespace optilab_parameters {
enum class CoreMode { PodcastLeveler=0, StreamPolish=1, SmoothLimiter=2, StreamUpper=3, SmoothUpper=6 };
enum class PipelinePreset { StreamUpper=0, PodcastLeveler=3, StreamPolish=4, SmoothBase=6, SmoothUpper=7 };
struct ProfileInfo { CoreMode mode; PipelinePreset preset; int lookaheadProfile; bool coreUsesAuthoredTrim; };
inline constexpr std::array profiles {
    ProfileInfo{CoreMode::PodcastLeveler,PipelinePreset::PodcastLeveler,0,true},
    ProfileInfo{CoreMode::StreamPolish,PipelinePreset::StreamPolish,1,true},
    ProfileInfo{CoreMode::StreamUpper,PipelinePreset::StreamUpper,1,false},
    ProfileInfo{CoreMode::SmoothLimiter,PipelinePreset::SmoothBase,2,false},
    ProfileInfo{CoreMode::SmoothUpper,PipelinePreset::SmoothUpper,2,false}
};
inline constexpr std::size_t profileCount=profiles.size();
inline constexpr std::size_t profileSlots=7;
constexpr const ProfileInfo* findProfile(PipelinePreset preset) noexcept {
    for(const auto& f:profiles) if(f.preset==preset) return &f;
    return nullptr;
}
constexpr const ProfileInfo* findProfile(CoreMode mode) noexcept {
    for(const auto& f:profiles) if(f.mode==mode) return &f;
    return nullptr;
}
constexpr int profileIndex(PipelinePreset preset) noexcept {
    for(std::size_t i=0;i<profiles.size();++i) if(profiles[i].preset==preset) return int(i);
    return 0;
}
constexpr const ProfileInfo& profileAt(int index) noexcept {
    return profiles[std::size_t(index>=0 && index<int(profileCount)?index:0)];
}
}
