#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <filesystem>

#define NOMINMAX
#include <windows.h>

#include "ViewImage.h"

namespace fs = std::filesystem;

// ------------------- State -------------------
static std::vector<std::wstring> focusModes = {L"AF_S", L"AF_A", L"AF_C", L"DMF", L"MF"};
static std::wstring currentFocusMode;
static bool showDropdown = false;

// Shutter speed dropdown state
static std::vector<std::wstring> shutterSpeeds = {
    L"Bulb", L"30\"", L"25\"", L"20\"", L"15\"", L"13\"", L"10\"", L"8\"", L"6\"", L"5\"",
    L"4\"", L"3.2\"", L"2.5\"", L"2\"", L"1.6\"", L"1.3\"", L"1\"", L"0.8\"", L"0.6\"", L"0.5\"",
    L"0.4\"", L"1/3", L"1/4", L"1/5", L"1/6", L"1/8", L"1/10", L"1/13", L"1/15", L"1/20",
    L"1/25", L"1/30", L"1/40", L"1/50", L"1/60", L"1/80", L"1/100", L"1/125", L"1/160", L"1/200",
    L"1/250", L"1/320", L"1/400", L"1/500", L"1/640", L"1/800", L"1/1,000", L"1/1,250", L"1/1,600",
    L"1/2,000", L"1/2,500", L"1/3,200", L"1/4,000"};
static std::wstring currentShutterSpeedMode = shutterSpeeds[0];
static bool showShutterDropdown = false;
static int shutterDropdownScroll = 0;
static const int SHUTTER_DROPDOWN_VISIBLE = 8;

// Aperture dropdown state
static std::vector<std::wstring> apertureValues = {
    L"F2.79999", L"F3.19999", L"F3.5", L"F4", L"F4.5", L"F5", L"F5.59999", L"F6.29999", L"F7.09999",
    L"F8", L"F9", L"F10", L"F11", L"F13", L"F14", L"F16", L"F18", L"F20", L"F22"};
static std::wstring currentApertureMode = apertureValues[0];
static bool showApertureDropdown = false;
static int apertureDropdownScroll = 0;
static const int APERTURE_DROPDOWN_VISIBLE = 8;

// ISO dropdown state
static std::vector<std::wstring> isoValues = {
    L"ISO AUTO", L"ISO 50", L"ISO 64", L"ISO 80", L"ISO 100", L"ISO 125", L"ISO 160", L"ISO 200",
    L"ISO 250", L"ISO 320", L"ISO 400", L"ISO 500", L"ISO 640", L"ISO 800", L"ISO 1,000", L"ISO 1,250",
    L"ISO 1,600", L"ISO 2,000", L"ISO 2,500", L"ISO 3,200", L"ISO 4,000", L"ISO 5,000", L"ISO 6,400",
    L"ISO 8,000", L"ISO 10,000", L"ISO 12,800", L"ISO 16,000", L"ISO 20,000", L"ISO 25,600", L"ISO 32,000",
    L"ISO 40,000", L"ISO 51,200", L"ISO 64,000", L"ISO 80,000", L"ISO 102,400"};
static std::wstring currentISOMode = isoValues[0];
static bool showISODropdown = false;
static int isoDropdownScroll = 0;
static const int ISO_DROPDOWN_VISIBLE = 8;

// Exposure Program Mode dropdown state
static std::vector<std::wstring> exposureModes = {
    L"Auto", L"P_Auto", L"A_AperturePriority", L"S_ShutterSpeedPriority", L"M_Manual", L"Portrait",
    L"Sports_Action", L"Macro", L"Landscape", L"Sunset", L"Night"};
static std::wstring currentExposureProgramMode = exposureModes[0];
static bool showExposureDropdown = false;
static int exposureDropdownScroll = 0;
static const int EXPOSURE_DROPDOWN_VISIBLE = 6;

// Movie recording toggle state
static std::atomic<bool> movieRecFlag{false};

// Save destination dropdown state
static std::vector<std::string> saveDestOptions = {"PC", "Camera", "Both"};
static int currentSaveDestIndex = 0;
static bool showSaveDestDropdown = false;

// Interval input state
static std::atomic<int> autoIntervalMs{3000};
static bool editingInterval = false;
static std::string intervalBuf = "3000";

// Current read-only values (from camera)
static std::wstring currentExposureMode;
static std::wstring currentShutterSpeed;
static std::wstring currentAperture;
static std::wstring currentISO;

// Utility: convert wstring -> narrow for OpenCV text
static std::string ws2s(const std::wstring &ws)
{
    return std::string(ws.begin(), ws.end());
}

// ------------------- Layout -------------------
struct UIRects
{
    cv::Point titlePos;

    // Top-row dropdowns (compact)
    cv::Rect dropdownBox; // Focus Mode
    std::vector<cv::Rect> dropdownItems;

    cv::Rect exposureDropdownBox; // Exposure Mode (now beside Focus)
    std::vector<cv::Rect> exposureDropdownItems;

    cv::Rect shutterDropdownBox; // Shutter
    std::vector<cv::Rect> shutterDropdownItems;

    cv::Rect apertureDropdownBox; // Aperture
    std::vector<cv::Rect> apertureDropdownItems;

    cv::Rect isoDropdownBox; // ISO
    std::vector<cv::Rect> isoDropdownItems;

    // Buttons and values
    cv::Rect btnFocus;
    cv::Rect btnCapture;
    cv::Rect btnAutoCapture;

    cv::Rect intervalBox;
    cv::Rect btnMovieRec;
    cv::Rect saveDestBox;
    std::vector<cv::Rect> saveDestItems;
    cv::Rect btnOpenLatest;
    cv::Rect btnOpenLatestVideo;
    cv::Rect btnExit;
};

// Smaller control-centre window
static const int CC_WIDTH = 1120;
static const int CC_HEIGHT = 600;

static UIRects makeLayout()
{
    UIRects r{};
    r.titlePos = {20, 40};

    // Sizes (smaller)
    const int ddH = 40;
    const int gapX = 16;
    const int gapY = 14;

    // Top row origin
    const int topX = 20;
    const int topY = 60;

    // Focus Mode dropdown (narrower)
    const int focusW = 180;
    r.dropdownBox = {topX, topY, focusW, ddH};
    {
        const int itemH = 34, itemGap = 4, listTop = topY + ddH + 6;
        for (size_t i = 0; i < focusModes.size(); ++i)
            r.dropdownItems.push_back(cv::Rect(topX, listTop + int(i) * (itemH + itemGap), focusW, itemH));
    }

    // Exposure Mode beside Focus
    const int expoX = r.dropdownBox.x + r.dropdownBox.width + gapX;
    const int expoW = 220;
    r.exposureDropdownBox = {expoX, topY, expoW, ddH};
    {
        const int itemH = 34, itemGap = 4, listTop = topY + ddH + 6;
        for (size_t i = 0; i < exposureModes.size(); ++i)
            r.exposureDropdownItems.push_back(cv::Rect(expoX, listTop + int(i) * (itemH + itemGap), expoW, itemH));
    }

    // Shutter, Aperture, ISO continue on the row
    const int shutX = r.exposureDropdownBox.x + r.exposureDropdownBox.width + gapX;
    const int shutW = 200;
    r.shutterDropdownBox = {shutX, topY, shutW, ddH};
    {
        const int itemH = 30, itemGap = 4, listTop = topY + ddH + 6;
        for (size_t i = 0; i < shutterSpeeds.size(); ++i)
            r.shutterDropdownItems.push_back(cv::Rect(shutX, listTop + int(i) * (itemH + itemGap), shutW, itemH));
    }

    const int apX = r.shutterDropdownBox.x + r.shutterDropdownBox.width + gapX;
    const int apW = 150;
    r.apertureDropdownBox = {apX, topY, apW, ddH};
    {
        const int itemH = 30, itemGap = 4, listTop = topY + ddH + 6;
        for (size_t i = 0; i < apertureValues.size(); ++i)
            r.apertureDropdownItems.push_back(cv::Rect(apX, listTop + int(i) * (itemH + itemGap), apW, itemH));
    }

    const int isoX = r.apertureDropdownBox.x + r.apertureDropdownBox.width + gapX;
    const int isoW = 200;
    r.isoDropdownBox = {isoX, topY, isoW, ddH};
    {
        const int itemH = 30, itemGap = 4, listTop = topY + ddH + 6;
        for (size_t i = 0; i < isoValues.size(); ++i)
            r.isoDropdownItems.push_back(cv::Rect(isoX, listTop + int(i) * (itemH + itemGap), isoW, itemH));
    }

    // Buttons row beneath the dropdowns
    const int btnW = 200, btnH = 48;
    const int colGap = gapX;
    int leftX = 20;
    int y = topY + ddH + 280; // space under dropdown lists

    // Row: Focus | Capture | Latest Photo | Latest Video | Exit
    r.btnFocus = {leftX, y, btnW, btnH};
    int x2 = leftX + btnW + colGap;
    r.btnCapture = {x2, y, btnW, btnH};
    int x3 = x2 + btnW + colGap;
    r.btnOpenLatest = {x3, y, btnW, btnH};
    int x4 = x3 + btnW + colGap;
    r.btnOpenLatestVideo = {x4, y, btnW, btnH};
    int x5 = x4 + btnW + colGap;
    r.btnExit = {x5, y, btnW, btnH};

    // Next row: Auto-capture, interval input, movie record, save destination
    y += btnH + gapY;
    r.btnAutoCapture = {leftX, y, btnW, btnH};
    r.intervalBox = {leftX + btnW + colGap, y, btnW, btnH};
    r.btnMovieRec = {leftX + (btnW + colGap) * 2, y, btnW, btnH};

    const int sdX = leftX + (btnW + colGap) * 3;
    const int sdW = btnW;
    r.saveDestBox = {sdX, y, sdW, btnH};
    {
        const int itemH = 34, itemGap = 4;
        for (int i = 0; i < 3; ++i)
            r.saveDestItems.push_back(cv::Rect(sdX, y + btnH + 6 + i * (itemH + itemGap), sdW, itemH));
    }

    return r;
}

// ------------------- Drawing -------------------
static void drawButton(cv::Mat &img, const cv::Rect &rc, const std::string &label,
                       const cv::Scalar &fill, const cv::Scalar &textColor = {0, 0, 0})
{
    cv::rectangle(img, rc, fill, cv::FILLED, cv::LINE_AA);
    int baseline = 0;
    double fontScale = 0.85;
    int thickness = 2;
    auto size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
    cv::Point textOrg(rc.x + (rc.width - size.width) / 2, rc.y + (rc.height + size.height) / 2 - 3);
    cv::putText(img, label, textOrg, cv::FONT_HERSHEY_SIMPLEX, fontScale, textColor, thickness, cv::LINE_AA);
}

static void drawInputBox(cv::Mat &img, const cv::Rect &rc, const std::string &text, bool focused)
{
    cv::Scalar bg = {245, 245, 245};
    cv::rectangle(img, rc, bg, cv::FILLED, cv::LINE_AA);
    cv::Scalar border = focused ? cv::Scalar(0, 0, 255) : cv::Scalar(150, 150, 150);
    cv::rectangle(img, rc, border, 2, cv::LINE_AA);
    double fontScale = 0.85;
    int thickness = 2, baseline = 0;
    auto sz = cv::getTextSize(text.empty() ? "enter ms" : text,
                              cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
    cv::Scalar tc = text.empty() ? cv::Scalar(120, 120, 120) : cv::Scalar(0, 0, 0);
    cv::Point org(rc.x + 10, rc.y + (rc.height + sz.height) / 2 - 3);
    cv::putText(img, text.empty() ? "enter ms" : text, org,
                cv::FONT_HERSHEY_SIMPLEX, fontScale, tc, thickness, cv::LINE_AA);
    std::string label = "Auto Capture Interval (ms)";
    cv::putText(img, label, {rc.x, rc.y - 6}, cv::FONT_HERSHEY_SIMPLEX, 0.55, {60, 60, 60}, 1, cv::LINE_AA);
}

static void drawLabelAbove(cv::Mat &img, const cv::Rect &rc, const std::string &label)
{
    cv::putText(img, label, {rc.x, rc.y - 6}, cv::FONT_HERSHEY_SIMPLEX, 0.55, {60, 60, 60}, 1, cv::LINE_AA);
}

static void drawControlCentreUI(cv::Mat &settingsWindow, std::atomic<bool> &autoCaptureFlag, const UIRects &r)
{
    int baseline = 0;
    cv::putText(settingsWindow, "Camera Control Centre", r.titlePos, cv::FONT_HERSHEY_SIMPLEX, 1.2, {0, 0, 0}, 3, cv::LINE_AA);

    // Focus
    drawLabelAbove(settingsWindow, r.dropdownBox, "Focus Mode");
    cv::rectangle(settingsWindow, r.dropdownBox, {200, 200, 200}, cv::FILLED, cv::LINE_AA);
    std::string currentStr(currentFocusMode.begin(), currentFocusMode.end());
    auto sz = cv::getTextSize(currentStr, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
    cv::putText(settingsWindow, currentStr,
                {r.dropdownBox.x + 10, r.dropdownBox.y + (r.dropdownBox.height + sz.height) / 2},
                cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);

    if (showDropdown)
    {
        for (size_t i = 0; i < r.dropdownItems.size(); ++i)
        {
            const auto &itemRc = r.dropdownItems[i];
            cv::rectangle(settingsWindow, itemRc, {220, 220, 220}, cv::FILLED, cv::LINE_AA);
            std::string s(focusModes[i].begin(), focusModes[i].end());
            auto sz2 = cv::getTextSize(s, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
            cv::putText(settingsWindow, s,
                        {itemRc.x + 10, itemRc.y + (itemRc.height + sz2.height) / 2},
                        cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);
        }
    }

    // Exposure (now beside Focus)
    drawLabelAbove(settingsWindow, r.exposureDropdownBox, "Exposure Mode");
    cv::rectangle(settingsWindow, r.exposureDropdownBox, {200, 200, 200}, cv::FILLED, cv::LINE_AA);
    std::string currentExpStr = ws2s(currentExposureMode);
    auto szExp = cv::getTextSize(currentExpStr, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
    cv::putText(settingsWindow, currentExpStr,
                {r.exposureDropdownBox.x + 10, r.exposureDropdownBox.y + (r.exposureDropdownBox.height + szExp.height) / 2},
                cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);

    if (showExposureDropdown)
    {
        int totalItems = static_cast<int>(exposureModes.size());
        int startIdx = exposureDropdownScroll;
        int endIdx = std::min(startIdx + EXPOSURE_DROPDOWN_VISIBLE, totalItems);
        int itemX = r.exposureDropdownBox.x;
        int itemY = r.exposureDropdownBox.y + r.exposureDropdownBox.height + 6;
        int itemW = r.exposureDropdownBox.width;
        int itemH = 34;
        int itemGap = 4;

        for (int visIdx = 0, i = startIdx; i < endIdx; ++i, ++visIdx)
        {
            int y = itemY + visIdx * (itemH + itemGap);
            cv::Rect itemRc(itemX, y, itemW, itemH);
            cv::rectangle(settingsWindow, itemRc, {220, 220, 220}, cv::FILLED, cv::LINE_AA);
            std::string s(exposureModes[i].begin(), exposureModes[i].end());
            auto sz2 = cv::getTextSize(s, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
            cv::putText(settingsWindow, s,
                        {itemRc.x + 10, itemRc.y + (itemRc.height + sz2.height) / 2},
                        cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);
        }

        int barX = r.exposureDropdownBox.x + r.exposureDropdownBox.width - 16;
        int barY = itemY;
        int barW = 12;
        int barH = (itemH + itemGap) * EXPOSURE_DROPDOWN_VISIBLE - itemGap;
        cv::rectangle(settingsWindow, cv::Rect(barX, barY, barW, barH), {180, 180, 180}, cv::FILLED, cv::LINE_AA);

        float thumbFrac = float(EXPOSURE_DROPDOWN_VISIBLE) / totalItems;
        int thumbH = std::max(int(barH * thumbFrac), 16);
        int thumbY = barY + int(barH * (float(exposureDropdownScroll) / totalItems));
        cv::rectangle(settingsWindow, cv::Rect(barX, thumbY, barW, thumbH), {100, 100, 100}, cv::FILLED, cv::LINE_AA);
    }

    // Shutter
    drawLabelAbove(settingsWindow, r.shutterDropdownBox, "Shutter Speed");
    cv::rectangle(settingsWindow, r.shutterDropdownBox, {200, 200, 200}, cv::FILLED, cv::LINE_AA);
    std::string currentShutterStr = ws2s(currentShutterSpeed);
    auto szShutter = cv::getTextSize(currentShutterStr, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
    cv::putText(settingsWindow, currentShutterStr,
                {r.shutterDropdownBox.x + 10, r.shutterDropdownBox.y + (r.shutterDropdownBox.height + szShutter.height) / 2},
                cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);

    if (showShutterDropdown)
    {
        int totalItems = static_cast<int>(shutterSpeeds.size());
        int startIdx = shutterDropdownScroll;
        int endIdx = std::min(startIdx + SHUTTER_DROPDOWN_VISIBLE, totalItems);

        int itemX = r.shutterDropdownBox.x;
        int itemY = r.shutterDropdownBox.y + r.shutterDropdownBox.height + 6;
        int itemW = r.shutterDropdownBox.width;
        int itemH = 30;
        int itemGap = 4;

        for (int visIdx = 0, i = startIdx; i < endIdx; ++i, ++visIdx)
        {
            int y = itemY + visIdx * (itemH + itemGap);
            cv::Rect itemRc(itemX, y, itemW, itemH);
            cv::rectangle(settingsWindow, itemRc, {220, 220, 220}, cv::FILLED, cv::LINE_AA);
            std::string s(shutterSpeeds[i].begin(), shutterSpeeds[i].end());
            auto sz2 = cv::getTextSize(s, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
            cv::putText(settingsWindow, s,
                        {itemRc.x + 10, itemRc.y + (itemRc.height + sz2.height) / 2},
                        cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);
        }

        int barX = r.shutterDropdownBox.x + r.shutterDropdownBox.width - 16;
        int barY = itemY;
        int barW = 12;
        int barH = (itemH + itemGap) * SHUTTER_DROPDOWN_VISIBLE - itemGap;
        cv::rectangle(settingsWindow, cv::Rect(barX, barY, barW, barH), {180, 180, 180}, cv::FILLED, cv::LINE_AA);

        float thumbFrac = float(SHUTTER_DROPDOWN_VISIBLE) / totalItems;
        int thumbH = std::max(int(barH * thumbFrac), 16);
        int thumbY = barY + int(barH * (float(shutterDropdownScroll) / totalItems));
        cv::rectangle(settingsWindow, cv::Rect(barX, thumbY, barW, thumbH), {100, 100, 100}, cv::FILLED, cv::LINE_AA);
    }

    // Aperture
    drawLabelAbove(settingsWindow, r.apertureDropdownBox, "Aperture");
    cv::rectangle(settingsWindow, r.apertureDropdownBox, {200, 200, 200}, cv::FILLED, cv::LINE_AA);
    std::string currentAperStr = ws2s(currentAperture);
    auto szAper = cv::getTextSize(currentAperStr, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
    cv::putText(settingsWindow, currentAperStr,
                {r.apertureDropdownBox.x + 10, r.apertureDropdownBox.y + (r.apertureDropdownBox.height + szAper.height) / 2},
                cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);

    if (showApertureDropdown)
    {
        int totalItems = static_cast<int>(apertureValues.size());
        int startIdx = apertureDropdownScroll;
        int endIdx = std::min(startIdx + APERTURE_DROPDOWN_VISIBLE, totalItems);

        int itemX = r.apertureDropdownBox.x;
        int itemY = r.apertureDropdownBox.y + r.apertureDropdownBox.height + 6;
        int itemW = r.apertureDropdownBox.width;
        int itemH = 30;
        int itemGap = 4;

        for (int visIdx = 0, i = startIdx; i < endIdx; ++i, ++visIdx)
        {
            int y = itemY + visIdx * (itemH + itemGap);
            cv::Rect itemRc(itemX, y, itemW, itemH);
            cv::rectangle(settingsWindow, itemRc, {220, 220, 220}, cv::FILLED, cv::LINE_AA);
            std::string s(apertureValues[i].begin(), apertureValues[i].end());
            auto sz2 = cv::getTextSize(s, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
            cv::putText(settingsWindow, s,
                        {itemRc.x + 10, itemRc.y + (itemRc.height + sz2.height) / 2},
                        cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);
        }

        int barX = r.apertureDropdownBox.x + r.apertureDropdownBox.width - 16;
        int barY = itemY;
        int barW = 12;
        int barH = (itemH + itemGap) * APERTURE_DROPDOWN_VISIBLE - itemGap;
        cv::rectangle(settingsWindow, cv::Rect(barX, barY, barW, barH), {180, 180, 180}, cv::FILLED, cv::LINE_AA);

        float thumbFrac = float(APERTURE_DROPDOWN_VISIBLE) / totalItems;
        int thumbH = std::max(int(barH * thumbFrac), 16);
        int thumbY = barY + int(barH * (float(apertureDropdownScroll) / totalItems));
        cv::rectangle(settingsWindow, cv::Rect(barX, thumbY, barW, thumbH), {100, 100, 100}, cv::FILLED, cv::LINE_AA);
    }

    // ISO
    drawLabelAbove(settingsWindow, r.isoDropdownBox, "ISO");
    cv::rectangle(settingsWindow, r.isoDropdownBox, {200, 200, 200}, cv::FILLED, cv::LINE_AA);
    std::string currentISOStr = ws2s(currentISO);
    auto szISO = cv::getTextSize(currentISOStr, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
    cv::putText(settingsWindow, currentISOStr,
                {r.isoDropdownBox.x + 10, r.isoDropdownBox.y + (r.isoDropdownBox.height + szISO.height) / 2},
                cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);

    if (showISODropdown)
    {
        int totalItems = static_cast<int>(isoValues.size());
        int startIdx = isoDropdownScroll;
        int endIdx = std::min(startIdx + ISO_DROPDOWN_VISIBLE, totalItems);

        int itemX = r.isoDropdownBox.x;
        int itemY = r.isoDropdownBox.y + r.isoDropdownBox.height + 6;
        int itemW = r.isoDropdownBox.width;
        int itemH = 30;
        int itemGap = 4;

        for (int visIdx = 0, i = startIdx; i < endIdx; ++i, ++visIdx)
        {
            int y = itemY + visIdx * (itemH + itemGap);
            cv::Rect itemRc(itemX, y, itemW, itemH);
            cv::rectangle(settingsWindow, itemRc, {220, 220, 220}, cv::FILLED, cv::LINE_AA);
            std::string s(isoValues[i].begin(), isoValues[i].end());
            auto sz2 = cv::getTextSize(s, cv::FONT_HERSHEY_SIMPLEX, 0.95, 2, &baseline);
            cv::putText(settingsWindow, s,
                        {itemRc.x + 10, itemRc.y + (itemRc.height + sz2.height) / 2},
                        cv::FONT_HERSHEY_SIMPLEX, 0.95, {0, 0, 0}, 2, cv::LINE_AA);
        }

        int barX = r.isoDropdownBox.x + r.isoDropdownBox.width - 16;
        int barY = itemY;
        int barW = 12;
        int barH = (itemH + itemGap) * ISO_DROPDOWN_VISIBLE - itemGap;
        cv::rectangle(settingsWindow, cv::Rect(barX, barY, barW, barH), {180, 180, 180}, cv::FILLED, cv::LINE_AA);

        float thumbFrac = float(ISO_DROPDOWN_VISIBLE) / totalItems;
        int thumbH = std::max(int(barH * thumbFrac), 16);
        int thumbY = barY + int(barH * (float(isoDropdownScroll) / totalItems));
        cv::rectangle(settingsWindow, cv::Rect(barX, thumbY, barW, thumbH), {100, 100, 100}, cv::FILLED, cv::LINE_AA);
    }

    // Buttons + misc
    drawButton(settingsWindow, r.btnFocus, "Focus", {180, 180, 180});
    drawButton(settingsWindow, r.btnCapture, "Capture", {180, 180, 180});
    drawButton(settingsWindow, r.btnAutoCapture, "Auto Capture",
               autoCaptureFlag.load() ? cv::Scalar(100, 250, 100) : cv::Scalar(180, 180, 180));

    drawInputBox(settingsWindow, r.intervalBox, intervalBuf, editingInterval);
    drawButton(settingsWindow, r.btnMovieRec, "Record Video",
               movieRecFlag.load() ? cv::Scalar(0, 0, 220) : cv::Scalar(180, 180, 180),
               movieRecFlag.load() ? cv::Scalar(255, 255, 255) : cv::Scalar(0, 0, 0));

    // Save destination dropdown
    drawLabelAbove(settingsWindow, r.saveDestBox, "Save To");
    cv::rectangle(settingsWindow, r.saveDestBox, {200, 200, 200}, cv::FILLED, cv::LINE_AA);
    {
        int bl = 0;
        std::string sdLabel = saveDestOptions[currentSaveDestIndex];
        auto sdSz = cv::getTextSize(sdLabel, cv::FONT_HERSHEY_SIMPLEX, 0.85, 2, &bl);
        cv::putText(settingsWindow, sdLabel,
                    {r.saveDestBox.x + 10, r.saveDestBox.y + (r.saveDestBox.height + sdSz.height) / 2},
                    cv::FONT_HERSHEY_SIMPLEX, 0.85, {0, 0, 0}, 2, cv::LINE_AA);
    }
    if (showSaveDestDropdown)
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto &itemRc = r.saveDestItems[i];
            cv::Scalar bg = (i == currentSaveDestIndex) ? cv::Scalar(160, 210, 160) : cv::Scalar(220, 220, 220);
            cv::rectangle(settingsWindow, itemRc, bg, cv::FILLED, cv::LINE_AA);
            int bl = 0;
            auto sz2 = cv::getTextSize(saveDestOptions[i], cv::FONT_HERSHEY_SIMPLEX, 0.85, 2, &bl);
            cv::putText(settingsWindow, saveDestOptions[i],
                        {itemRc.x + 10, itemRc.y + (itemRc.height + sz2.height) / 2},
                        cv::FONT_HERSHEY_SIMPLEX, 0.85, {0, 0, 0}, 2, cv::LINE_AA);
        }
    }

    drawButton(settingsWindow, r.btnOpenLatest, "Latest Photo", {100, 180, 250}, {255, 255, 255});
    drawButton(settingsWindow, r.btnOpenLatestVideo, "Latest Video", {200, 120, 90}, {255, 255, 255});
    drawButton(settingsWindow, r.btnExit, "Exit", {50, 50, 50}, {255, 255, 255});
}

// ------------------- File Search -------------------
static std::string toLower(const std::string &s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

static std::string findLatestDSCImage(const std::string &folder)
{
    std::string latestFile;
    std::time_t latestTime = 0;
    for (const auto &entry : fs::directory_iterator(folder))
    {
        if (!entry.is_regular_file())
            continue;
        std::string name = entry.path().filename().string();
        std::string lower = toLower(name);
        if (lower.rfind("dsc", 0) == 0 && lower.size() >= 8 &&
            lower.substr(lower.size() - 4) == ".jpg")
        {
            auto ftime = fs::last_write_time(entry);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
            if (cftime > latestTime)
            {
                latestTime = cftime;
                latestFile = entry.path().string();
            }
        }
    }
    return latestFile;
}

static std::string findLatestVideo(const std::string &folder)
{
    static const std::vector<std::string> videoExts = {".mp4", ".mov", ".mts", ".m4v"};
    std::string latestFile;
    std::time_t latestTime = 0;
    for (const auto &entry : fs::directory_iterator(folder))
    {
        if (!entry.is_regular_file())
            continue;
        std::string lower = toLower(entry.path().filename().string());
        bool isVideo = false;
        for (const auto &ext : videoExts)
        {
            if (lower.size() >= ext.size() &&
                lower.compare(lower.size() - ext.size(), ext.size(), ext) == 0)
            {
                isVideo = true;
                break;
            }
        }
        if (!isVideo)
            continue;
        auto ftime = fs::last_write_time(entry);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        if (cftime > latestTime)
        {
            latestTime = cftime;
            latestFile = entry.path().string();
        }
    }
    return latestFile;
}

// ------------------- ViewImage methods -------------------
void ViewImage::runUI(std::shared_ptr<cli::CameraDevice> camera,
                      std::atomic<bool> &exitFlag,
                      std::atomic<bool> &autoCaptureFlag)
{
    movieRecFlag.store(false);
    CallbackContext context{camera, &exitFlag, &autoCaptureFlag, &movieRecFlag};
    std::thread autoCaptureThread([&]()
                                  {

        while (!exitFlag)
        {
            if (autoCaptureFlag.load())
            {
                std::cout << "[Auto Capture] Taking photo...\n";
                auto start = std::chrono::high_resolution_clock::now();
                camera->capture_image();
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> duration_ms = end - start;
                std::cout << "Execution time: " << duration_ms.count() << " ms\n";
                int ms = std::max(0, autoIntervalMs.load());
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        } });

    // Live View window removed. Only Control Centre remains.
    cv::namedWindow("Control Centre", cv::WINDOW_NORMAL);
    cv::resizeWindow("Control Centre", CC_WIDTH, CC_HEIGHT);

    while (!exitFlag)
    {
        // Refresh camera state when not actively auto-capturing
        if (!autoCaptureFlag.load())
        {
            camera->get_live_view();
            currentFocusMode = camera->get_focus_mode_output();
            currentExposureMode = camera->get_exposure_program_mode_output();
            currentShutterSpeed = camera->get_shutter_speed_output();
            currentAperture = camera->get_aperture_output();
            currentISO = camera->get_iso_output();
        }

        // Load the background image
        cv::Mat bg = cv::imread("cris_logo.png");

        // Create the settings window
        cv::Mat settingsWindow(CC_HEIGHT, CC_WIDTH, CV_8UC3);

        // If background loads successfully, resize and use it
        if (!bg.empty())
        {
            cv::resize(bg, bg, settingsWindow.size());
            bg.copyTo(settingsWindow);
        }
        else
        {
            // Fallback color if image not found
            settingsWindow = cv::Scalar(240, 240, 240);
        }

        // Draw UI on top of the background
        auto layout = makeLayout();
        drawControlCentreUI(settingsWindow, autoCaptureFlag, layout);

        cv::setMouseCallback("Control Centre", onMouse, &context);
        cv::imshow("Control Centre", settingsWindow);

        int key = cv::waitKey(30);
        if (editingInterval)
        {
            int k = key & 0xFF;
            if (k >= '0' && k <= '9')
            {
                if (intervalBuf.size() < 7)
                    intervalBuf.push_back(static_cast<char>(k));
            }
            else if (k == 8 || k == 127)
            {
                if (!intervalBuf.empty())
                    intervalBuf.pop_back();
            }
            else if (k == 13 || k == 10)
            {
                int v = 0;
                try
                {
                    v = intervalBuf.empty() ? 0 : std::stoi(intervalBuf);
                }
                catch (...)
                {
                    v = 0;
                }
                v = std::max(0, std::min(v, 600000));
                autoIntervalMs.store(v);
                editingInterval = false;
                std::cout << "[Auto Capture] Interval set to " << v << " ms\n";
            }
            else if (k == 27)
            {
                editingInterval = false;
            }
        }
        else
        {
            if ((key & 0xFF) == 27)
            {
                exitFlag = true;
            }
        }
    }

    if (autoCaptureThread.joinable())
        autoCaptureThread.join();

    cv::destroyAllWindows();
}

// ------------------- Mouse Handling -------------------
static bool pointIn(const cv::Point &p, const cv::Rect &r) { return r.contains(p); }

void ViewImage::onMouse(int event, int x, int y, int flags, void *userdata)
{
    auto *context = static_cast<CallbackContext *>(userdata);
    auto camera = context->camera;
    auto exitFlag = context->exitFlag;
    auto autoCaptureFlag = context->autoCaptureFlag;
    auto movieRecFlagPtr = context->movieRecFlag;

    const auto r = makeLayout();
    const cv::Point pt{x, y};

    // Scroll handlers
    if (showShutterDropdown && (event == cv::EVENT_MOUSEWHEEL || event == cv::EVENT_MOUSEHWHEEL))
    {
        int totalItems = static_cast<int>(shutterSpeeds.size());
        int delta = cv::getMouseWheelDelta(flags);
        if (delta > 0)
            shutterDropdownScroll = std::max(0, shutterDropdownScroll - 1);
        else if (delta < 0)
            shutterDropdownScroll = std::min(std::max(0, totalItems - SHUTTER_DROPDOWN_VISIBLE), shutterDropdownScroll + 1);
        return;
    }
    if (showApertureDropdown && (event == cv::EVENT_MOUSEWHEEL || event == cv::EVENT_MOUSEHWHEEL))
    {
        int totalItems = static_cast<int>(apertureValues.size());
        int delta = cv::getMouseWheelDelta(flags);
        if (delta > 0)
            apertureDropdownScroll = std::max(0, apertureDropdownScroll - 1);
        else if (delta < 0)
            apertureDropdownScroll = std::min(std::max(0, totalItems - APERTURE_DROPDOWN_VISIBLE), apertureDropdownScroll + 1);
        return;
    }
    if (showISODropdown && (event == cv::EVENT_MOUSEWHEEL || event == cv::EVENT_MOUSEHWHEEL))
    {
        int totalItems = static_cast<int>(isoValues.size());
        int delta = cv::getMouseWheelDelta(flags);
        if (delta > 0)
            isoDropdownScroll = std::max(0, isoDropdownScroll - 1);
        else if (delta < 0)
            isoDropdownScroll = std::min(std::max(0, totalItems - ISO_DROPDOWN_VISIBLE), isoDropdownScroll + 1);
        return;
    }
    if (showExposureDropdown && (event == cv::EVENT_MOUSEWHEEL || event == cv::EVENT_MOUSEHWHEEL))
    {
        int totalItems = static_cast<int>(exposureModes.size());
        int delta = cv::getMouseWheelDelta(flags);
        if (delta > 0)
            exposureDropdownScroll = std::max(0, exposureDropdownScroll - 1);
        else if (delta < 0)
            exposureDropdownScroll = std::min(std::max(0, totalItems - EXPOSURE_DROPDOWN_VISIBLE), exposureDropdownScroll + 1);
        return;
    }

    if (event != cv::EVENT_LBUTTONDOWN)
        return;

    // Interval edit
    if (pointIn(pt, r.intervalBox))
    {
        editingInterval = true;
        return;
    }
    else
    {
        editingInterval = false;
    }

    // Save destination dropdown toggle
    if (pointIn(pt, r.saveDestBox))
    {
        showSaveDestDropdown = !showSaveDestDropdown;
        if (showSaveDestDropdown)
        {
            showDropdown = false;
            showShutterDropdown = false;
            showApertureDropdown = false;
            showISODropdown = false;
            showExposureDropdown = false;
        }
        return;
    }
    if (showSaveDestDropdown)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (pointIn(pt, r.saveDestItems[i]))
            {
                currentSaveDestIndex = i;
                camera->set_still_image_store_destination(i);
                showSaveDestDropdown = false;
                std::cout << "[Save To] Set to: " << saveDestOptions[i] << "\n";
                return;
            }
        }
        showSaveDestDropdown = false;
        return;
    }

    // Focus dropdown toggle
    if (pointIn(pt, r.dropdownBox))
    {
        showDropdown = !showDropdown;
        if (showDropdown)
        {
            showShutterDropdown = false;
            showApertureDropdown = false;
            showISODropdown = false;
            showExposureDropdown = false;
            showSaveDestDropdown = false;
        }
        return;
    }

    if (showDropdown)
    {
        for (size_t i = 0; i < r.dropdownItems.size(); ++i)
        {
            if (pointIn(pt, r.dropdownItems[i]))
            {
                currentFocusMode = focusModes[i];
                camera->set_focus_mode_new(currentFocusMode, static_cast<int>(i));
                showDropdown = false;
                std::string s(currentFocusMode.begin(), currentFocusMode.end());
                std::cout << "[Focus Mode] Set to: " << s << "\n";
                return;
            }
        }
    }

    // Exposure dropdown toggle
    if (pointIn(pt, r.exposureDropdownBox))
    {
        showExposureDropdown = !showExposureDropdown;
        if (showExposureDropdown)
        {
            showDropdown = false;
            showShutterDropdown = false;
            showApertureDropdown = false;
            showISODropdown = false;
            showSaveDestDropdown = false;
        }
        return;
    }

    if (showExposureDropdown)
    {
        int totalItems = static_cast<int>(exposureModes.size());
        int startIdx = exposureDropdownScroll;
        int endIdx = std::min(startIdx + EXPOSURE_DROPDOWN_VISIBLE, totalItems);

        int itemX = r.exposureDropdownBox.x;
        int itemY = r.exposureDropdownBox.y + r.exposureDropdownBox.height + 6;
        int itemW = r.exposureDropdownBox.width;
        int itemH = 34;
        int itemGap = 4;

        int barX = r.exposureDropdownBox.x + r.exposureDropdownBox.width - 16;
        int barY = itemY;
        int barW = 12;
        int barH = (itemH + itemGap) * EXPOSURE_DROPDOWN_VISIBLE - itemGap;
        cv::Rect scrollbarRect(barX, barY, barW, barH);
        if (pointIn(pt, scrollbarRect))
            return;

        for (int visIdx = 0, i = startIdx; i < endIdx; ++i, ++visIdx)
        {
            int y = itemY + visIdx * (itemH + itemGap);
            cv::Rect itemRc(itemX, y, itemW, itemH);
            if (pointIn(pt, itemRc))
            {
                currentExposureProgramMode = exposureModes[i];
                camera->set_exposure_program_mode_new(currentExposureProgramMode, i);
                showExposureDropdown = false;
                std::string s(currentExposureProgramMode.begin(), currentExposureProgramMode.end());
                std::cout << "[Exposure Mode] Set to: " << s << "\n";
                return;
            }
        }
    }

    // Shutter dropdown toggle
    if (pointIn(pt, r.shutterDropdownBox))
    {
        showShutterDropdown = !showShutterDropdown;
        if (showShutterDropdown)
        {
            showDropdown = false;
            showApertureDropdown = false;
            showISODropdown = false;
            showExposureDropdown = false;
            showSaveDestDropdown = false;
        }
        return;
    }

    if (showShutterDropdown)
    {
        int totalItems = static_cast<int>(shutterSpeeds.size());
        int startIdx = shutterDropdownScroll;
        int endIdx = std::min(startIdx + SHUTTER_DROPDOWN_VISIBLE, totalItems);

        int itemX = r.shutterDropdownBox.x;
        int itemY = r.shutterDropdownBox.y + r.shutterDropdownBox.height + 6;
        int itemW = r.shutterDropdownBox.width;
        int itemH = 30;
        int itemGap = 4;

        int barX = r.shutterDropdownBox.x + r.shutterDropdownBox.width - 16;
        int barY = itemY;
        int barW = 12;
        int barH = (itemH + itemGap) * SHUTTER_DROPDOWN_VISIBLE - itemGap;
        cv::Rect scrollbarRect(barX, barY, barW, barH);
        if (pointIn(pt, scrollbarRect))
            return;

        for (int visIdx = 0, i = startIdx; i < endIdx; ++i, ++visIdx)
        {
            int y = itemY + visIdx * (itemH + itemGap);
            cv::Rect itemRc(itemX, y, itemW, itemH);
            if (pointIn(pt, itemRc))
            {
                currentShutterSpeedMode = shutterSpeeds[i];
                camera->set_shutter_speed_new(currentShutterSpeedMode, i);
                showShutterDropdown = false;
                std::string s(currentShutterSpeedMode.begin(), currentShutterSpeedMode.end());
                std::cout << "[Shutter Speed] Set to: " << s << "\n";
                return;
            }
        }
    }

    // Aperture dropdown toggle
    if (pointIn(pt, r.apertureDropdownBox))
    {
        showApertureDropdown = !showApertureDropdown;
        if (showApertureDropdown)
        {
            showDropdown = false;
            showShutterDropdown = false;
            showISODropdown = false;
            showExposureDropdown = false;
            showSaveDestDropdown = false;
        }
        return;
    }

    if (showApertureDropdown)
    {
        int totalItems = static_cast<int>(apertureValues.size());
        int startIdx = apertureDropdownScroll;
        int endIdx = std::min(startIdx + APERTURE_DROPDOWN_VISIBLE, totalItems);

        int itemX = r.apertureDropdownBox.x;
        int itemY = r.apertureDropdownBox.y + r.apertureDropdownBox.height + 6;
        int itemW = r.apertureDropdownBox.width;
        int itemH = 30;
        int itemGap = 4;

        int barX = r.apertureDropdownBox.x + r.apertureDropdownBox.width - 16;
        int barY = itemY;
        int barW = 12;
        int barH = (itemH + itemGap) * APERTURE_DROPDOWN_VISIBLE - itemGap;
        cv::Rect scrollbarRect(barX, barY, barW, barH);
        if (pointIn(pt, scrollbarRect))
            return;

        for (int visIdx = 0, i = startIdx; i < endIdx; ++i, ++visIdx)
        {
            int y = itemY + visIdx * (itemH + itemGap);
            cv::Rect itemRc(itemX, y, itemW, itemH);
            if (pointIn(pt, itemRc))
            {
                currentApertureMode = apertureValues[i];
                camera->set_aperture_new(currentApertureMode, i);
                showApertureDropdown = false;
                std::string s(currentApertureMode.begin(), currentApertureMode.end());
                std::cout << "[Aperture] Set to: " << s << "\n";
                return;
            }
        }
    }

    // ISO dropdown toggle
    if (pointIn(pt, r.isoDropdownBox))
    {
        showISODropdown = !showISODropdown;
        if (showISODropdown)
        {
            showDropdown = false;
            showShutterDropdown = false;
            showApertureDropdown = false;
            showExposureDropdown = false;
            showSaveDestDropdown = false;
        }
        return;
    }

    if (showISODropdown)
    {
        int totalItems = static_cast<int>(isoValues.size());
        int startIdx = isoDropdownScroll;
        int endIdx = std::min(startIdx + ISO_DROPDOWN_VISIBLE, totalItems);

        int itemX = r.isoDropdownBox.x;
        int itemY = r.isoDropdownBox.y + r.isoDropdownBox.height + 6;
        int itemW = r.isoDropdownBox.width;
        int itemH = 30;
        int itemGap = 4;

        int barX = r.isoDropdownBox.x + r.isoDropdownBox.width - 16;
        int barY = itemY;
        int barW = 12;
        int barH = (itemH + itemGap) * ISO_DROPDOWN_VISIBLE - itemGap;
        cv::Rect scrollbarRect(barX, barY, barW, barH);
        if (pointIn(pt, scrollbarRect))
            return;

        for (int visIdx = 0, i = startIdx; i < endIdx; ++i, ++visIdx)
        {
            int y = itemY + visIdx * (itemH + itemGap);
            cv::Rect itemRc(itemX, y, itemW, itemH);
            if (pointIn(pt, itemRc))
            {
                currentISOMode = isoValues[i];
                camera->set_iso_new(currentISOMode, i);
                showISODropdown = false;
                std::string s(currentISOMode.begin(), currentISOMode.end());
                std::cout << "[ISO] Set to: " << s << "\n";
                return;
            }
        }
    }

    // Buttons
    if (pointIn(pt, r.btnFocus))
    {
        std::cout << "[Focus] clicked\n";
        std::cout << "\nSent START\n";
        camera->s1_shooting();
        std::cout << "\nSent STOP\n";
        return;
    }
    if (pointIn(pt, r.btnCapture))
    {
        std::cout << "[Capture] clicked\n";
        std::cout << "\nSent START\n";
        camera->capture_image();
        std::cout << "\nSent STOP\n";
        return;
    }
    if (pointIn(pt, r.btnAutoCapture))
    {
        *autoCaptureFlag = !(*autoCaptureFlag);
        std::cout << "[Auto Capture] toggled to: " << (*autoCaptureFlag ? "ON" : "OFF") << "\n";
        return;
    }
    if (pointIn(pt, r.btnMovieRec))
    {
        bool nowRecording = !movieRecFlagPtr->load();
        movieRecFlagPtr->store(nowRecording);
        if (nowRecording)
            camera->start_movie_rec();
        else
            camera->stop_movie_rec();
        std::cout << "[Movie Rec] " << (nowRecording ? "Started recording\n" : "Stopped recording\n");
        return;
    }
    if (pointIn(pt, r.btnOpenLatest))
    {
        std::cout << "[Open Latest] clicked\n";
        // Run the Contents Transfer "get contents" command, download the newest
        // still as 2M into the working (Release) folder, and open it.
        std::string latest = camera->openLatestStill2M();
        if (latest.empty())
        {
            // Fall back to the newest DSCxxxxx.JPG already in the working folder.
            latest = findLatestDSCImage(fs::current_path().string());
        }
        if (!latest.empty())
        {
            std::cout << "Opening in default viewer: " << latest << "\n";
            ShellExecuteA(NULL, "open", latest.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        else
        {
            std::cout << "No still image available to open.\n";
        }
        return;
    }
    if (pointIn(pt, r.btnOpenLatestVideo))
    {
        std::cout << "[Latest Video] clicked\n";
        // Run the Contents Transfer "get contents" command, download the newest
        // movie at its original size into the working (Release) folder, and open it.
        std::string latest = camera->openLatestMovie();
        if (latest.empty())
        {
            // Fall back to the newest video already in the working folder.
            latest = findLatestVideo(fs::current_path().string());
        }
        if (!latest.empty())
        {
            std::cout << "Opening in default viewer: " << latest << "\n";
            ShellExecuteA(NULL, "open", latest.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        else
        {
            std::cout << "No video available to open.\n";
        }
        return;
    }
    if (pointIn(pt, r.btnExit))
    {
        std::cout << "[Exit] clicked\n";
        *exitFlag = true;
        return;
    }
}