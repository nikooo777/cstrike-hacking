#pragma once

#include <Windows.h>

#include <cstddef>
#include <string>

namespace config {

struct Signature {
    std::string name;
    std::string module;
    std::string pattern;
    std::string operand;
    std::size_t operandOffset = 0;
    std::size_t instructionOffset = 0;
    std::size_t instructionLength = 0;
    std::size_t indirections = 0;
    bool required = true;
    bool validateVtable = false;

    std::string description;
    std::string discovery;
    std::string source;
    std::string sourceReadme;
    std::string sourceVideo;
    std::string notes;
};

struct Interface {
    std::string name;
    std::string module;
    bool required = true;

    std::string discovery;
    std::string source;
    std::string sourceReadme;
    std::string notes;
};

struct Settings {
    bool requireUnique = true;
    bool validatePointers = true;
    bool logMatchOffsets = true;
};

struct FeatureDefaults {
    bool bhop = true;
    bool aimbot = true;
    bool triggerbot = true;
    bool norecoil = true;
    bool visualNoRecoil = true;
    // Perfect nospread mutates cmd angles; keep off until diagnostics pass.
    bool perfectNoSpread = false;
    // Make the CreateMove caller leave the camera alone after cmd mutation.
    bool silentAngles = true;
    bool menuOpen = false;
};

struct Config {
    std::string path;
    Settings settings;
    FeatureDefaults features;
    Signature clientState;
    Signature clientMode;
    Interface clientEntityList;
    Interface engineClient;
};

bool Load(HMODULE selfModule, std::string &error);
const Config &Get();
bool IsLoaded();

} // namespace config
