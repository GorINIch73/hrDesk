#pragma once

#include <imgui.h>
#include <string>
#include <vector>

struct LoadedFonts {
    ImFont* ui = nullptr;
    ImFont* uiBold = nullptr;
    ImFont* table = nullptr;
    std::vector<std::string> resolved;
};

LoadedFonts loadFonts(const std::string& assetsDir);
