#pragma once

#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))) \
    || (defined(__SSE__) && (defined(__i386__) || defined(__x86_64__)))
#include <xmmintrin.h>
#define OPTILAB_HAS_MXCSR 1
#else
#define OPTILAB_HAS_MXCSR 0
#endif

// Recursive filters eventually enter the IEEE-754 subnormal range after audio
// decays to silence. On x86 that can make otherwise simple multiply/add code
// dramatically slower. Scope FTZ/DAZ to one processing block and restore the
// caller's floating-point mode on exit.
class OptiLabDenormalGuard {
public:
    OptiLabDenormalGuard() noexcept {
#if OPTILAB_HAS_MXCSR
        previous_ = _mm_getcsr();
        constexpr unsigned int denormalsAreZero = 0x0040u;
        constexpr unsigned int flushToZero = 0x8000u;
        _mm_setcsr(previous_ | denormalsAreZero | flushToZero);
#endif
    }

    ~OptiLabDenormalGuard() noexcept {
#if OPTILAB_HAS_MXCSR
        _mm_setcsr(previous_);
#endif
    }

    OptiLabDenormalGuard(const OptiLabDenormalGuard&) = delete;
    OptiLabDenormalGuard& operator=(const OptiLabDenormalGuard&) = delete;

private:
#if OPTILAB_HAS_MXCSR
    unsigned int previous_ = 0;
#endif
};

#undef OPTILAB_HAS_MXCSR
