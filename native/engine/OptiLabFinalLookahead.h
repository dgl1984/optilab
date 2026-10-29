#pragma once

#include <algorithm>
#include <cassert>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

// Delay the signal immediately before pre-limiting/clipping. A monotonic peak
// queue supplies the largest linked peak from the output sample through the
// newest input, so a peak cannot leave the detector before its audio arrives.
// Storage is fixed; configuration/reset happens outside sample processing.
class OptiLabFinalLookahead {
public:
    static constexpr int capacity = 2048; // 1.5 ms at 768 kHz needs 1152 samples.
    struct Frame {
        double left = 0.0, right = 0.0;
        double referenceLeft = 0.0, referenceRight = 0.0;
        double thresholdDrive = 1.0;
        double precedingGain = 1.0;
    };
    struct Result { Frame audio; double peak; };

    void configure(int samples) noexcept {
        assert(samples >= 0 && samples < capacity);
        samples = std::clamp(samples, 0, capacity - 1);
        if (samples != delay_) { delay_ = samples; reset(); }
    }
    void reset() noexcept {
        frames_.fill(Frame{});
        write_ = head_ = count_ = 0;
        clock_ = 0;
    }
    Result process(const Frame& incoming) noexcept {
        const double peak = std::max(std::abs(incoming.left), std::abs(incoming.right));
        if (delay_ == 0) return {incoming, peak};
        while (count_ && clock_ - times_[head_] > static_cast<std::uint64_t>(delay_)) {
            head_ = (head_ + 1) % capacity;
            --count_;
        }
        while (count_) {
            const int back = (head_ + count_ - 1) % capacity;
            if (peaks_[back] > peak) break;
            --count_;
        }
        const int tail = (head_ + count_) % capacity;
        peaks_[tail] = peak;
        times_[tail] = clock_++;
        ++count_;
        int read = write_ - delay_;
        if (read < 0) read += capacity;
        const Frame delayed = frames_[read];
        frames_[write_] = incoming;
        write_ = (write_ + 1) % capacity;
        return {delayed, peaks_[head_]};
    }
private:
    std::array<Frame, capacity> frames_{};
    std::array<double, capacity> peaks_{};
    std::array<std::uint64_t, capacity> times_{};
    int delay_ = 0, write_ = 0, head_ = 0, count_ = 0;
    std::uint64_t clock_ = 0;
};
