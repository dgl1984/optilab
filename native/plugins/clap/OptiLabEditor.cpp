#include "OptiLabEditor.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>

namespace {

constexpr COLORREF background = RGB(16, 19, 22);
constexpr COLORREF panel = RGB(24, 29, 34);
constexpr COLORREF accent = RGB(20, 184, 212);
constexpr COLORREF text = RGB(229, 237, 244);
constexpr COLORREF muted = RGB(168, 180, 194);
constexpr COLORREF border = RGB(70, 84, 102);
constexpr COLORREF activity = RGB(249, 115, 22);
constexpr COLORREF warning = RGB(250, 204, 21);
constexpr COLORREF safe = RGB(34, 197, 94);
constexpr COLORREF danger = RGB(239, 68, 68);

constexpr int modeControlId = 1001;
constexpr int inputTrackId = 1002;
constexpr int adaptTrackId = 1003;
constexpr int inputValueId = 1004;
constexpr UINT_PTR meterTimerId = 1;
constexpr UINT reaperWrapperReadyMessage = WM_APP + 1;

struct Layout {
    RECT mode{};
    RECT inputSlider{};
    RECT adaptSlider{};
    RECT controlsPanel{};
    RECT activityPanel{};
    std::array<RECT, 4> meters{};
    int scaleLine = 1;
};

double clamp(double value, double lo, double hi) {
    return std::min(std::max(value, lo), hi);
}

double toDb(double value) {
    return 20.0 * std::log10(std::max(value, 0.000001));
}

const wchar_t* modeName(int mode) {
    if (mode == 0) return L"Podcast Leveler";
    if (mode == 1) return L"Stream polish";
    return L"Smooth Limiter";
}

bool isReaperPluginWrapper(HWND window) {
    // This wrapper can intercept Tab and hit testing. Keep the editor as its
    // sibling; docs/CLAP_ACCESSIBILITY.md records the required host behavior.
    wchar_t className[64]{};
    return window &&
           GetClassNameW(window, className, static_cast<int>(std::size(className))) != 0 &&
           std::wcscmp(className, L"reaperPluginHostWrapProc") == 0;
}

void fill(HDC dc, const RECT& rect, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void frame(HDC dc, const RECT& rect, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

} // namespace

struct OptiLabEditor::Impl {
    explicit Impl(OptiLabEditorDelegate& owner) : delegate(owner) {}

    OptiLabEditorDelegate& delegate;
    HWND window = nullptr;
    HWND reaperWrapper = nullptr;
    HWND modeLabel = nullptr;
    HWND modeControl = nullptr;
    HWND inputLabel = nullptr;
    HWND inputValue = nullptr;
    HWND inputTrack = nullptr;
    HWND adaptLabel = nullptr;
    HWND adaptValue = nullptr;
    HWND adaptTrack = nullptr;
    std::uint32_t width = 600;
    std::uint32_t height = 300;
    Layout layout{};
    double fontScale = 0.0;
    HFONT normalFont = nullptr;
    HFONT boldFont = nullptr;
    HFONT titleFont = nullptr;
    HFONT smallFont = nullptr;
    HBRUSH panelBrush = CreateSolidBrush(panel);

    ~Impl() {
        destroyFonts();
        if (panelBrush) DeleteObject(panelBrush);
    }

    void destroyFonts() {
        if (normalFont) DeleteObject(normalFont);
        if (boldFont) DeleteObject(boldFont);
        if (titleFont) DeleteObject(titleFont);
        if (smallFont) DeleteObject(smallFont);
        normalFont = boldFont = titleFont = smallFont = nullptr;
    }

    void ensureFonts(double scale) {
        if (normalFont && std::abs(scale - fontScale) < 0.01) return;
        destroyFonts();
        fontScale = scale;
        const auto size = [scale](int logical, int minimum) {
            return -std::max(minimum, static_cast<int>(std::floor(logical * scale)));
        };
        normalFont = CreateFontW(size(11, 7), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        boldFont = CreateFontW(size(11, 7), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        titleFont = CreateFontW(size(16, 10), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        smallFont = CreateFontW(size(9, 6), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        applyControlFonts();
    }

    void applyControlFonts() {
        const auto setFont = [](HWND control, HFONT font) {
            if (control && font) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        };
        setFont(modeLabel, normalFont);
        setFont(modeControl, normalFont);
        setFont(inputLabel, normalFont);
        setFont(inputValue, boldFont);
        setFont(adaptLabel, normalFont);
        setFont(adaptValue, boldFont);
    }

    void drawText(HDC dc, const wchar_t* value, RECT rect, COLORREF color, HFONT font,
                  UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, color);
        HGDIOBJ oldFont = SelectObject(dc, font);
        DrawTextW(dc, value, -1, &rect, format);
        SelectObject(dc, oldFont);
    }

    Layout calculateLayout(int pixelWidth, int pixelHeight, double& scaleOut) {
        const double scale = clamp(std::min(pixelWidth / 600.0, pixelHeight / 300.0), 0.25, 2.0);
        scaleOut = scale;
        const double logicalWidth = pixelWidth / scale;
        const double logicalHeight = pixelHeight / scale;
        const auto px = [scale](double value) { return static_cast<int>(std::lround(value * scale)); };
        const auto makeRect = [&px](double x, double y, double w, double h) {
            return RECT{px(x), px(y), px(x + w), px(y + h)};
        };

        const double margin = logicalWidth < 420 ? 10 : 18;
        const double gap = logicalWidth < 420 ? 8 : 12;
        const double headerHeight = logicalHeight < 220 ? 34 : 45;
        const double footerHeight = logicalHeight < 190 ? 0 : 32;
        const double contentTop = headerHeight + (logicalHeight < 220 ? 6 : 15);
        const double footerY = logicalHeight - footerHeight;
        const double contentBottom = footerY - (footerHeight > 0 ? 10 : 6);
        const double contentHeight = std::max(80.0, contentBottom - contentTop);
        const bool stacked = pixelWidth < pixelHeight * 1.45;

        double controlsX = margin;
        double controlsY = contentTop;
        double controlsWidth = 0.0;
        double controlsHeight = 0.0;
        double activityX = 0.0;
        double activityY = 0.0;
        double activityWidth = 0.0;
        double activityHeight = 0.0;

        if (stacked) {
            controlsWidth = logicalWidth - margin * 2;
            controlsHeight = std::min(240.0, std::max(170.0, contentHeight * 0.36));
            activityX = margin;
            activityY = controlsY + controlsHeight + gap;
            activityWidth = controlsWidth;
            activityHeight = std::max(80.0, contentBottom - activityY);
        } else {
            const double availableWidth = logicalWidth - margin * 2 - gap;
            controlsWidth = availableWidth * 0.58;
            controlsHeight = contentHeight;
            activityX = controlsX + controlsWidth + gap;
            activityY = contentTop;
            activityWidth = logicalWidth - margin - activityX;
            activityHeight = contentHeight;
        }

        Layout result;
        result.scaleLine = std::max(1, px(1));
        result.controlsPanel = makeRect(controlsX, controlsY, controlsWidth, controlsHeight);
        result.activityPanel = makeRect(activityX, activityY, activityWidth, activityHeight);
        const double controlsInnerX = controlsX + 14;
        const double controlsInnerWidth = controlsWidth - 28;
        const double modeY = controlsY + 35;
        const double sliderOneY = std::min(controlsY + controlsHeight - 93,
                                           controlsY + std::max(76.0, controlsHeight * 0.45));
        const double sliderTwoY = controlsY + controlsHeight - 48;
        result.mode = makeRect(controlsInnerX, modeY, controlsInnerWidth, 30);
        result.inputSlider = makeRect(controlsInnerX, sliderOneY, controlsInnerWidth, 40);
        result.adaptSlider = makeRect(controlsInnerX, sliderTwoY, controlsInnerWidth, 40);

        const double meterTop = activityY + 36;
        const double meterHeight = std::max(36.0, activityHeight - 78);
        const double meterAreaX = activityX + 21;
        const double meterAreaWidth = std::max(120.0, activityWidth - 42);
        const double meterWidth = std::min(40.0, std::max(22.0, meterAreaWidth * 0.11));
        const double peakWidth = meterWidth + 2;
        const double meterGap = std::max(6.0, (meterAreaWidth - peakWidth - meterWidth * 3) / 3);
        const std::array<double, 4> meterX{
            meterAreaX,
            meterAreaX + peakWidth + meterGap,
            meterAreaX + peakWidth + meterGap + meterWidth + meterGap,
            meterAreaX + peakWidth + meterGap + (meterWidth + meterGap) * 2,
        };
        result.meters[0] = makeRect(meterX[0], meterTop, peakWidth, meterHeight);
        for (std::size_t i = 1; i < result.meters.size(); ++i) {
            result.meters[i] = makeRect(meterX[i], meterTop, meterWidth, meterHeight);
        }
        return result;
    }

    static void placeControl(HWND control, const RECT& bounds) {
        if (!control) return;
        SetWindowPos(control, nullptr, bounds.left, bounds.top,
                     std::max(1L, bounds.right - bounds.left),
                     std::max(1L, bounds.bottom - bounds.top),
                     SWP_NOACTIVATE | SWP_NOZORDER);
    }

    POINT editorOrigin() const {
        POINT origin{0, 0};
        if (!window || !reaperWrapper || !IsWindow(reaperWrapper)) return origin;
        RECT wrapperBounds{};
        if (!GetWindowRect(reaperWrapper, &wrapperBounds)) return origin;
        origin = {wrapperBounds.left, wrapperBounds.top};
        ScreenToClient(GetParent(window), &origin);
        return origin;
    }

    void positionControls() {
        if (!window) return;
        RECT client{};
        GetClientRect(window, &client);
        double scale = 1.0;
        layout = calculateLayout(std::max(1L, client.right), std::max(1L, client.bottom), scale);
        ensureFonts(scale);

        const int labelHeight = std::max(14, static_cast<int>(18 * scale));
        const int trackHeight = std::max(24, static_cast<int>(28 * scale));
        RECT modeLabelBounds{layout.mode.left, layout.mode.top - labelHeight,
                             layout.mode.right, layout.mode.top};
        RECT inputLabelBounds{layout.inputSlider.left, layout.inputSlider.top,
                              (layout.inputSlider.left + layout.inputSlider.right) / 2,
                              layout.inputSlider.top + labelHeight};
        RECT inputValueBounds{inputLabelBounds.right, inputLabelBounds.top,
                              layout.inputSlider.right, inputLabelBounds.bottom};
        RECT inputTrackBounds{layout.inputSlider.left, inputLabelBounds.bottom,
                              layout.inputSlider.right,
                              std::min(layout.inputSlider.bottom, inputLabelBounds.bottom + trackHeight)};
        RECT adaptLabelBounds{layout.adaptSlider.left, layout.adaptSlider.top,
                              (layout.adaptSlider.left + layout.adaptSlider.right) / 2,
                              layout.adaptSlider.top + labelHeight};
        RECT adaptValueBounds{adaptLabelBounds.right, adaptLabelBounds.top,
                              layout.adaptSlider.right, adaptLabelBounds.bottom};
        RECT adaptTrackBounds{layout.adaptSlider.left, adaptLabelBounds.bottom,
                              layout.adaptSlider.right,
                              std::min(layout.adaptSlider.bottom, adaptLabelBounds.bottom + trackHeight)};

        placeControl(modeLabel, modeLabelBounds);
        placeControl(modeControl, layout.mode);
        placeControl(inputLabel, inputLabelBounds);
        placeControl(inputValue, inputValueBounds);
        placeControl(inputTrack, inputTrackBounds);
        placeControl(adaptLabel, adaptLabelBounds);
        placeControl(adaptValue, adaptValueBounds);
        placeControl(adaptTrack, adaptTrackBounds);
    }

    void syncControlValues() {
        if (!modeControl) return;
        const int mode = static_cast<int>(std::lround(delegate.parameterValue(0)));
        const double inputDb = delegate.parameterValue(1);
        const int inputPosition = static_cast<int>(std::lround(inputDb * 10.0));
        const int adaptPosition = static_cast<int>(std::lround(delegate.parameterValue(2)));
        if (ComboBox_GetCurSel(modeControl) != mode) ComboBox_SetCurSel(modeControl, mode);
        if (SendMessageW(inputTrack, TBM_GETPOS, 0, 0) != inputPosition) {
            SendMessageW(inputTrack, TBM_SETPOS, TRUE, inputPosition);
        }
        if (SendMessageW(adaptTrack, TBM_GETPOS, 0, 0) != adaptPosition) {
            SendMessageW(adaptTrack, TBM_SETPOS, TRUE, adaptPosition);
        }
        wchar_t valueText[32]{};
        std::swprintf(valueText, std::size(valueText), L"%+.1f dB", inputDb);
        wchar_t currentText[64]{};
        GetWindowTextW(inputValue, currentText, static_cast<int>(std::size(currentText)));
        if (std::wcscmp(currentText, valueText) != 0) {
            SetWindowTextW(inputValue, valueText);
            NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, inputValue, OBJID_CLIENT, CHILDID_SELF);
        }
        wchar_t accessibleName[64]{};
        std::swprintf(accessibleName, std::size(accessibleName),
                      L"Input Drive, %+.1f dB", inputDb);
        GetWindowTextW(inputTrack, currentText, static_cast<int>(std::size(currentText)));
        if (std::wcscmp(currentText, accessibleName) != 0) {
            SetWindowTextW(inputTrack, accessibleName);
            NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, inputTrack, OBJID_CLIENT, CHILDID_SELF);
        }
        std::swprintf(valueText, std::size(valueText), L"%.0f percent", delegate.parameterValue(2));
        GetWindowTextW(adaptValue, currentText, static_cast<int>(std::size(currentText)));
        if (std::wcscmp(currentText, valueText) != 0) {
            SetWindowTextW(adaptValue, valueText);
        }
    }

    void focusControl(HWND current, bool previous) {
        const std::array<HWND, 3> controls{modeControl, inputValue, adaptTrack};
        auto found = std::find(controls.begin(), controls.end(), current);
        std::size_t index = found == controls.end()
                                ? 0
                                : static_cast<std::size_t>(found - controls.begin());
        if (previous) {
            index = index == 0 ? controls.size() - 1 : index - 1;
        } else {
            index = (index + 1) % controls.size();
        }
        SetFocus(controls[index]);
    }

    void adjustInputFromKeyboard(WPARAM key) {
        double value = delegate.parameterValue(1);
        switch (key) {
        case VK_LEFT:
        case VK_DOWN:
            value -= 0.1;
            break;
        case VK_RIGHT:
        case VK_UP:
            value += 0.1;
            break;
        case VK_NEXT:
            value -= 1.0;
            break;
        case VK_PRIOR:
            value += 1.0;
            break;
        case VK_HOME:
            value = -12.0;
            break;
        case VK_END:
            value = 18.0;
            break;
        default:
            return;
        }
        value = std::round(clamp(value, -12.0, 18.0) * 10.0) / 10.0;
        delegate.setParameterFromEditor(1, value);
        syncControlValues();
        InvalidateRect(window, nullptr, FALSE);
    }

    static LRESULT CALLBACK controlSubclassProc(HWND control,
                                                UINT message,
                                                WPARAM wParam,
                                                LPARAM lParam,
                                                UINT_PTR,
                                                DWORD_PTR reference) {
        auto* self = reinterpret_cast<Impl*>(reference);
        if (message == WM_GETDLGCODE && wParam == VK_TAB) {
            return DLGC_WANTTAB;
        }
        if (message == WM_KEYDOWN && wParam == VK_TAB) {
            self->focusControl(control, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            return 0;
        }
        if (message == WM_KEYDOWN && wParam == VK_F6) {
            SetFocus(GetParent(self->window));
            return 0;
        }
        if (message == WM_KEYDOWN && control == self->inputValue) {
            self->adjustInputFromKeyboard(wParam);
            return 0;
        }
        return DefSubclassProc(control, message, wParam, lParam);
    }

    void createControls() {
        const HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE));
        const auto createStatic = [&](const wchar_t* name, DWORD alignment) {
            return CreateWindowExW(0, L"STATIC", name,
                                   WS_CHILD | WS_VISIBLE | alignment,
                                   0, 0, 1, 1, window, nullptr, instance, nullptr);
        };
        modeLabel = createStatic(L"Mode", SS_LEFT | SS_NOPREFIX);
        modeControl = CreateWindowExW(
            0, L"COMBOBOX", L"Mode",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            0, 0, 1, 120, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(modeControlId)), instance, nullptr);
        ComboBox_AddString(modeControl, L"Podcast Leveler");
        ComboBox_AddString(modeControl, L"Stream polish");
        ComboBox_AddString(modeControl, L"Smooth Limiter");

        inputLabel = createStatic(L"Input Drive", SS_LEFT | SS_NOPREFIX);
        inputValue = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_RIGHT | ES_AUTOHSCROLL,
            0, 0, 1, 1, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(inputValueId)), instance, nullptr);
        inputTrack = CreateWindowExW(
            0, TRACKBAR_CLASSW, L"Input Drive",
            WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS | TBS_TOOLTIPS,
            0, 0, 1, 1, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(inputTrackId)), instance, nullptr);
        SendMessageW(inputTrack, TBM_SETRANGE, TRUE, MAKELPARAM(-120, 180));
        SendMessageW(inputTrack, TBM_SETLINESIZE, 0, 1);
        SendMessageW(inputTrack, TBM_SETPAGESIZE, 0, 10);

        adaptLabel = createStatic(L"Auto-Adapt", SS_LEFT | SS_NOPREFIX);
        adaptValue = createStatic(L"", SS_RIGHT | SS_NOPREFIX);
        adaptTrack = CreateWindowExW(
            0, TRACKBAR_CLASSW, L"Auto-Adapt",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS | TBS_TOOLTIPS,
            0, 0, 1, 1, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(adaptTrackId)), instance, nullptr);
        SendMessageW(adaptTrack, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(adaptTrack, TBM_SETLINESIZE, 0, 1);
        SendMessageW(adaptTrack, TBM_SETPAGESIZE, 0, 10);

        for (HWND control : {modeControl, inputValue, adaptTrack}) {
            SetWindowSubclass(control, controlSubclassProc, 1,
                              reinterpret_cast<DWORD_PTR>(this));
        }
        positionControls();
        syncControlValues();
    }

    void drawPeakMeter(HDC dc, const RECT& bounds, double left, double right) {
        fill(dc, bounds, panel);
        frame(dc, bounds, border, layout.scaleLine);
        const double leftDb = toDb(left);
        const double rightDb = toDb(right);
        const int innerHeight =
            std::max(1, static_cast<int>(bounds.bottom - bounds.top - 4));
        const int barWidth =
            std::max(2, static_cast<int>((bounds.right - bounds.left - 6) / 2));
        const auto drawBar = [&](double db, int x) {
            const double amount = clamp((db + 60.0) / 66.0, 0.0, 1.0);
            const int barHeight = static_cast<int>(innerHeight * amount);
            const COLORREF color = db > 0.0 ? danger : (db > -12.0 ? warning : safe);
            RECT bar{x, bounds.bottom - 2 - barHeight, x + barWidth, bounds.bottom - 2};
            fill(dc, bar, color);
        };
        drawBar(leftDb, bounds.left + 2);
        drawBar(rightDb, bounds.left + 4 + barWidth);
    }

    void drawReductionMeter(HDC dc, const RECT& bounds, double reductionDb) {
        fill(dc, bounds, panel);
        frame(dc, bounds, border, layout.scaleLine);
        const double amount = clamp(reductionDb / 24.0, 0.0, 1.0);
        const int fillHeight = static_cast<int>((bounds.bottom - bounds.top - 4) * amount);
        const COLORREF color = reductionDb > 18.0 ? danger : (reductionDb > 12.0 ? warning : activity);
        RECT bar{bounds.left + 2, bounds.top + 2, bounds.right - 2, bounds.top + 2 + fillHeight};
        fill(dc, bar, color);
    }

    void paint() {
        PAINTSTRUCT paintStruct{};
        HDC target = BeginPaint(window, &paintStruct);
        RECT client{};
        GetClientRect(window, &client);
        const int clientWidth = std::max(1L, client.right - client.left);
        const int clientHeight = std::max(1L, client.bottom - client.top);
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, clientWidth, clientHeight);
        HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

        double scale = 1.0;
        layout = calculateLayout(clientWidth, clientHeight, scale);
        ensureFonts(scale);
        fill(dc, client, background);

        const int headerHeight = std::max(34, static_cast<int>(45 * std::min(scale, 1.0)));
        RECT header{0, 0, clientWidth, headerHeight};
        fill(dc, header, panel);
        HPEN linePen = CreatePen(PS_SOLID, layout.scaleLine, border);
        HGDIOBJ oldPen = SelectObject(dc, linePen);
        MoveToEx(dc, 0, header.bottom - 1, nullptr);
        LineTo(dc, clientWidth, header.bottom - 1);
        SelectObject(dc, oldPen);
        DeleteObject(linePen);

        RECT titleRect{std::max(8, static_cast<int>(15 * scale)), 0, clientWidth - 8, header.bottom};
        drawText(dc, L"OPTILAB CORE", titleRect, accent, titleFont);

        fill(dc, layout.controlsPanel, panel);
        frame(dc, layout.controlsPanel, border, layout.scaleLine);
        fill(dc, layout.activityPanel, panel);
        frame(dc, layout.activityPanel, border, layout.scaleLine);

        RECT controlsTitle = layout.controlsPanel;
        controlsTitle.left += std::max(8, static_cast<int>(14 * scale));
        controlsTitle.top += std::max(4, static_cast<int>(4 * scale));
        controlsTitle.bottom = controlsTitle.top + std::max(16, static_cast<int>(24 * scale));
        drawText(dc, L"CORE CONTROLS", controlsTitle, accent, boldFont);
        RECT activityTitle = layout.activityPanel;
        activityTitle.left += std::max(8, static_cast<int>(14 * scale));
        activityTitle.top += std::max(4, static_cast<int>(4 * scale));
        activityTitle.bottom = activityTitle.top + std::max(16, static_cast<int>(24 * scale));
        drawText(dc, L"CORE ACTIVITY", activityTitle, accent, boldFont);

        const auto meters = delegate.meterSnapshot();
        drawPeakMeter(dc, layout.meters[0], meters.inputLeft, meters.inputRight);
        drawReductionMeter(dc, layout.meters[1], meters.agcReductionDb);
        drawReductionMeter(dc, layout.meters[2], meters.densityReductionDb);
        drawReductionMeter(dc, layout.meters[3], meters.finalReductionDb);
        const std::array<const wchar_t*, 4> labels{L"Input", L"AGC", L"Density", L"Final"};
        const std::array<double, 4> values{
            std::max(toDb(meters.inputLeft), toDb(meters.inputRight)),
            meters.agcReductionDb,
            meters.densityReductionDb,
            meters.finalReductionDb,
        };
        for (std::size_t i = 0; i < layout.meters.size(); ++i) {
            RECT labelRect = layout.meters[i];
            labelRect.top = labelRect.bottom + std::max(2, static_cast<int>(4 * scale));
            labelRect.bottom = labelRect.top + std::max(12, static_cast<int>(16 * scale));
            drawText(dc, labels[i], labelRect, muted, smallFont,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            wchar_t number[16]{};
            if (i == 0) {
                std::swprintf(number, std::size(number), L"%.0f", values[i]);
            } else {
                std::swprintf(number, std::size(number), values[i] > 0.05 ? L"-%.0f" : L"0", values[i]);
            }
            RECT numberRect = layout.meters[i];
            numberRect.top = numberRect.bottom - std::max(12, static_cast<int>(18 * scale));
            drawText(dc, number, numberRect, i == 0 ? text : RGB(255, 224, 181), smallFont,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        const int footerHeight = std::max(0, static_cast<int>(32 * scale));
        if (clientHeight >= static_cast<int>(190 * scale) && footerHeight > 0) {
            RECT footer{0, clientHeight - footerHeight, clientWidth, clientHeight};
            fill(dc, footer, panel);
            wchar_t footerText[96]{};
            std::swprintf(footerText, std::size(footerText), L"%ls | ceiling -0.1 dBFS",
                          modeName(static_cast<int>(std::lround(delegate.parameterValue(0)))));
            footer.left += std::max(8, static_cast<int>(15 * scale));
            drawText(dc, footerText, footer, muted, smallFont);
        }

        BitBlt(target, 0, 0, clientWidth, clientHeight, dc, 0, 0, SRCCOPY);
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
        EndPaint(window, &paintStruct);
    }

    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_PAINT:
            paint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SETFOCUS:
            if (modeControl) SetFocus(modeControl);
            return 0;
        case WM_GETDLGCODE:
            return DLGC_WANTTAB;
        case reaperWrapperReadyMessage:
            // REAPER shows its wrapper after gui.show(), so defer hiding it.
            if (reaperWrapper && IsWindow(reaperWrapper)) {
                ShowWindow(reaperWrapper, SW_HIDE);
            }
            if (modeControl) SetFocus(modeControl);
            return 0;
        case WM_TIMER:
            if (wParam != meterTimerId) return 0;
            syncControlValues();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == modeControlId && HIWORD(wParam) == CBN_SELCHANGE) {
                const int mode = ComboBox_GetCurSel(modeControl);
                if (mode >= 0 && mode <= 2) {
                    delegate.setParameterFromEditor(0, static_cast<double>(mode));
                    delegate.setParameterFromEditor(1, mode == 0 ? 3.5 : mode == 1 ? 4.5 : 0.0);
                    syncControlValues();
                    InvalidateRect(window, nullptr, FALSE);
                }
            }
            return 0;
        case WM_HSCROLL: {
            const HWND source = reinterpret_cast<HWND>(lParam);
            if (source == inputTrack) {
                delegate.setParameterFromEditor(
                    1, static_cast<double>(SendMessageW(inputTrack, TBM_GETPOS, 0, 0)) / 10.0);
            } else if (source == adaptTrack) {
                delegate.setParameterFromEditor(
                    2, static_cast<double>(SendMessageW(adaptTrack, TBM_GETPOS, 0, 0)));
            }
            syncControlValues();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkColor(dc, panel);
            SetTextColor(dc, reinterpret_cast<HWND>(lParam) == inputValue ||
                                     reinterpret_cast<HWND>(lParam) == adaptValue
                                 ? text
                                 : muted);
            return reinterpret_cast<LRESULT>(panelBrush);
        }
        case WM_NOTIFY: {
            const auto* notification = reinterpret_cast<const NMHDR*>(lParam);
            if (notification && notification->code == TTN_GETDISPINFOW) {
                auto* info = reinterpret_cast<NMTTDISPINFOW*>(lParam);
                if (notification->idFrom == reinterpret_cast<UINT_PTR>(inputTrack)) {
                    info->lpszText = const_cast<wchar_t*>(L"Input Drive");
                } else if (notification->idFrom == reinterpret_cast<UINT_PTR>(adaptTrack)) {
                    info->lpszText = const_cast<wchar_t*>(L"Auto-Adapt");
                }
            }
            return 0;
        }
        case WM_SIZE:
            width = LOWORD(lParam);
            height = HIWORD(lParam);
            positionControls();
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        default:
            return DefWindowProcW(window, message, wParam, lParam);
        }
    }

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<Impl*>(create->lpCreateParams);
            self->window = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->handleMessage(message, wParam, lParam)
                    : DefWindowProcW(hwnd, message, wParam, lParam);
    }
};

OptiLabEditor::OptiLabEditor(OptiLabEditorDelegate& delegate)
    : impl(new Impl(delegate)) {}

OptiLabEditor::~OptiLabEditor() {
    destroy();
    delete impl;
}

bool OptiLabEditor::create(void* parentWindow, std::uint32_t newWidth, std::uint32_t newHeight) {
    if (impl->window) return true;
    INITCOMMONCONTROLSEX commonControls{sizeof(commonControls), ICC_BAR_CLASSES};
    InitCommonControlsEx(&commonControls);
    HINSTANCE instance = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&Impl::windowProc), &instance);
    constexpr const wchar_t* className = L"OptiLabCoreClapEditor";
    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = Impl::windowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;
    RegisterClassExW(&windowClass);

    impl->width = newWidth;
    impl->height = newHeight;
    HWND actualParent = static_cast<HWND>(parentWindow);
    if (isReaperPluginWrapper(actualParent) && GetParent(actualParent)) {
        impl->reaperWrapper = actualParent;
        actualParent = GetParent(actualParent);
    }
    POINT origin{0, 0};
    if (impl->reaperWrapper) {
        RECT wrapperBounds{};
        if (GetWindowRect(impl->reaperWrapper, &wrapperBounds)) {
            origin = {wrapperBounds.left, wrapperBounds.top};
            ScreenToClient(actualParent, &origin);
        }
    }
    impl->window = CreateWindowExW(
        WS_EX_CONTROLPARENT, className, L"OptiLab Core",
        WS_CHILD | WS_TABSTOP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        origin.x, origin.y, static_cast<int>(newWidth), static_cast<int>(newHeight),
        actualParent, nullptr, instance, impl);
    if (!impl->window) return false;
    impl->createControls();
    return true;
}

void OptiLabEditor::destroy() {
    if (!impl || !impl->window) return;
    impl->delegate.setEditorVisible(false);
    KillTimer(impl->window, meterTimerId);
    DestroyWindow(impl->window);
    impl->window = nullptr;
    impl->reaperWrapper = nullptr;
}

bool OptiLabEditor::setSize(std::uint32_t newWidth, std::uint32_t newHeight) {
    impl->width = newWidth;
    impl->height = newHeight;
    if (!impl->window) return true;
    const POINT origin = impl->editorOrigin();
    return SetWindowPos(impl->window, nullptr, origin.x, origin.y, static_cast<int>(newWidth),
                        static_cast<int>(newHeight), SWP_NOACTIVATE | SWP_NOZORDER) != FALSE;
}

bool OptiLabEditor::show() {
    if (!impl->window) return false;
    impl->syncControlValues();
    SetTimer(impl->window, meterTimerId, 33, nullptr);
    impl->delegate.setEditorVisible(true);
    ShowWindow(impl->window, SW_SHOW);
    if (impl->reaperWrapper) {
        PostMessageW(impl->window, reaperWrapperReadyMessage, 0, 0);
    } else {
        SetFocus(impl->modeControl);
    }
    return true;
}

bool OptiLabEditor::hide() {
    if (!impl->window) return false;
    impl->delegate.setEditorVisible(false);
    KillTimer(impl->window, meterTimerId);
    ShowWindow(impl->window, SW_HIDE);
    return true;
}

bool OptiLabEditor::exists() const noexcept {
    return impl && impl->window;
}
