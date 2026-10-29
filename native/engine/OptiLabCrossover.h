#pragma once

#include <array>
#include <cstddef>

// Small, allocation-free crossover helpers. Keep the established
// floating-point operation order stable where exact regression parity matters.
namespace optilab_crossover {

template <typename OnePole>
inline void splitSerialResidual(double input,
                                std::array<OnePole, 5>& filters,
                                const std::array<double, 5>& coefficients,
                                bool twoPole,
                                std::array<double, 6>& bands) noexcept
{
    double remainder = input;
    for (std::size_t i = 0; i < 5; ++i) {
        const double low = twoPole
            ? filters[i].lp2(remainder, coefficients[i])
            : filters[i].lp(remainder, coefficients[i]);
        bands[i] = low;
        remainder -= low;
    }
    bands[5] = remainder;
}

// A serial six-band Linkwitz-Riley tree needs phase compensation on bands that
// have already been split off. Without it, later LR stages rotate only the
// upper remainder and unity-gain recombination is no longer coherent. For an
// LR4 split the common low+high phase is one second-order all-pass at the split
// frequency, so each earlier band crosses that all-pass before recombination.
template <typename Biquad>
struct CoherentLr4SixBand {
    std::array<std::array<Biquad, 2>, 5> low{};
    std::array<std::array<Biquad, 2>, 5> high{};
    // compensation[stage][earlierBand]; only earlierBand < stage is active.
    std::array<std::array<Biquad, 4>, 5> compensation{};

    void configure(const std::array<double, 5>& frequencies, double sampleRate) noexcept
    {
        constexpr double q = 0.70710678118654752440;
        for (std::size_t stage = 0; stage < frequencies.size(); ++stage) {
            for (auto& section : low[stage])
                section.setLowpass(frequencies[stage], q, sampleRate);
            for (auto& section : high[stage])
                section.setHighpass(frequencies[stage], q, sampleRate);
            for (std::size_t band = 0; band < stage; ++band)
                compensation[stage][band].setAllpass(frequencies[stage], q, sampleRate);
        }
    }

    void reset() noexcept
    {
        for (auto& stage : low) for (auto& section : stage) section.reset();
        for (auto& stage : high) for (auto& section : stage) section.reset();
        for (auto& stage : compensation) for (auto& section : stage) section.reset();
    }

    void split(double input, std::array<double, 6>& bands) noexcept
    {
        double remainder = input;
        for (std::size_t stage = 0; stage < 5; ++stage) {
            for (std::size_t band = 0; band < stage; ++band)
                bands[band] = compensation[stage][band].run(bands[band]);

            double lowOut = remainder;
            double highOut = remainder;
            for (auto& section : low[stage]) lowOut = section.run(lowOut);
            for (auto& section : high[stage]) highOut = section.run(highOut);
            bands[stage] = lowOut;
            remainder = highOut;
        }
        bands[5] = remainder;
    }
};

// LR8 is the square of a fourth-order Butterworth response. The fourth-order
// Butterworth factors use the Q pair below; duplicating that pair produces the
// 8th-order low/high branches. Their common phase reduces to two second-order
// all-pass sections, which are used to keep earlier bands coherent as the
// serial tree advances.
template <typename Biquad>
struct CoherentLr8SixBand {
    std::array<std::array<Biquad, 4>, 5> low{};
    std::array<std::array<Biquad, 4>, 5> high{};
    // compensation[stage][earlierBand][section].
    std::array<std::array<std::array<Biquad, 2>, 4>, 5> compensation{};

    void configure(const std::array<double, 5>& frequencies, double sampleRate) noexcept
    {
        constexpr double qLow = 0.54119610014619698440;
        constexpr double qHigh = 1.3065629648763765279;
        constexpr std::array<double, 4> qSections {qLow, qHigh, qLow, qHigh};
        constexpr std::array<double, 2> qAllpass {qLow, qHigh};

        for (std::size_t stage = 0; stage < frequencies.size(); ++stage) {
            for (std::size_t section = 0; section < qSections.size(); ++section) {
                low[stage][section].setLowpass(frequencies[stage], qSections[section], sampleRate);
                high[stage][section].setHighpass(frequencies[stage], qSections[section], sampleRate);
            }
            for (std::size_t band = 0; band < stage; ++band)
                for (std::size_t section = 0; section < qAllpass.size(); ++section)
                    compensation[stage][band][section].setAllpass(
                        frequencies[stage], qAllpass[section], sampleRate);
        }
    }

    void reset() noexcept
    {
        for (auto& stage : low) for (auto& section : stage) section.reset();
        for (auto& stage : high) for (auto& section : stage) section.reset();
        for (auto& stage : compensation)
            for (auto& band : stage)
                for (auto& section : band)
                    section.reset();
    }

    void split(double input, std::array<double, 6>& bands) noexcept
    {
        double remainder = input;
        for (std::size_t stage = 0; stage < 5; ++stage) {
            for (std::size_t band = 0; band < stage; ++band)
                for (auto& section : compensation[stage][band])
                    bands[band] = section.run(bands[band]);

            double lowOut = remainder;
            double highOut = remainder;
            for (auto& section : low[stage]) lowOut = section.run(lowOut);
            for (auto& section : high[stage]) highOut = section.run(highOut);
            bands[stage] = lowOut;
            remainder = highOut;
        }
        bands[5] = remainder;
    }
};

} // namespace optilab_crossover
