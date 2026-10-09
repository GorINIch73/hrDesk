#include "Fonts.h"

#include <fstream>
#include <iostream>

static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

static std::string findFont(const std::string& assetsDir, const std::string& name) {
    std::vector<std::string> candidates = {
        assetsDir + "/fonts/" + name,
        "/usr/share/fonts/noto/" + name,
        "/usr/share/fonts/TTF/" + name,
    };
    for (const auto& c : candidates)
        if (fileExists(c)) return c;
    return "";
}

LoadedFonts loadFonts(const std::string& assetsDir) {
    ImGuiIO& io = ImGui::GetIO();
    LoadedFonts fonts;

    const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();

    std::string regular = findFont(assetsDir, "NotoSans-Regular.ttf");
    std::string bold = findFont(assetsDir, "NotoSans-Bold.ttf");

    if (regular.empty()) {
        std::cerr << "Font not found, using ImGui default (no Cyrillic)\n";
        fonts.ui = io.Fonts->AddFontDefault();
        fonts.uiBold = fonts.ui;
        fonts.table = fonts.ui;
        return fonts;
    }

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;

    fonts.ui = io.Fonts->AddFontFromFileTTF(regular.c_str(), 15.0f, &cfg, ranges);
    fonts.table = io.Fonts->AddFontFromFileTTF(regular.c_str(), 14.0f, &cfg, ranges);
    if (!bold.empty())
        fonts.uiBold = io.Fonts->AddFontFromFileTTF(bold.c_str(), 16.0f, &cfg, ranges);
    else
        fonts.uiBold = fonts.ui;

    fonts.resolved.push_back(regular);
    if (!bold.empty()) fonts.resolved.push_back(bold);
    return fonts;
}
