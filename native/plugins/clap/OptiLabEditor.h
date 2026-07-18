#pragma once

#include <cstdint>

struct OptiLabMeterSnapshot {
    double inputLeft = 0.0;
    double inputRight = 0.0;
    double agcReductionDb = 0.0;
    double densityReductionDb = 0.0;
    double finalReductionDb = 0.0;
};

class OptiLabEditorDelegate {
public:
    virtual ~OptiLabEditorDelegate() = default;
    virtual double parameterValue(std::uint32_t id) const noexcept = 0;
    virtual void setParameterFromEditor(std::uint32_t id, double value) = 0;
    virtual OptiLabMeterSnapshot meterSnapshot() const noexcept = 0;
    virtual void setEditorVisible(bool visible) noexcept = 0;
};

class OptiLabEditor {
public:
    explicit OptiLabEditor(OptiLabEditorDelegate& delegate);
    ~OptiLabEditor();

    bool create(void* parentWindow, std::uint32_t width, std::uint32_t height);
    void destroy();
    bool setSize(std::uint32_t width, std::uint32_t height);
    bool show();
    bool hide();
    bool exists() const noexcept;

private:
    struct Impl;
    Impl* impl = nullptr;
};
