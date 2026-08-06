#include "config/config.h"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

namespace config {

namespace {

using Section = std::map<std::string, std::string>;
using Ini = std::map<std::string, Section>;

Config g_config;
bool g_loaded = false;

std::string Trim(const std::string &value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string Lower(std::string value) {
    for (char &character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

bool ReadRaw(const Ini &ini, const std::string &sectionName,
             const std::string &key, std::string &value) {
    const auto section = ini.find(Lower(sectionName));
    if (section == ini.end()) {
        return false;
    }

    const auto entry = section->second.find(Lower(key));
    if (entry == section->second.end()) {
        return false;
    }

    value = entry->second;
    return true;
}

bool ReadRequiredString(const Ini &ini, const std::string &section,
                        const std::string &key, std::string &value,
                        std::string &error) {
    if (ReadRaw(ini, section, key, value) && !value.empty()) {
        return true;
    }

    error = "missing required [" + section + "] " + key;
    return false;
}

bool ReadOptionalString(const Ini &ini, const std::string &section,
                        const std::string &key, const std::string &fallback,
                        std::string &value, std::string &error) {
    if (!ReadRaw(ini, section, key, value)) {
        value = fallback;
        return true;
    }

    if (value.empty()) {
        error = "empty value for [" + section + "] " + key;
        return false;
    }
    return true;
}

bool ParseBool(const std::string &text, bool &value) {
    const std::string normalized = Lower(Trim(text));
    if (normalized == "true" || normalized == "1" ||
        normalized == "yes" || normalized == "on") {
        value = true;
        return true;
    }
    if (normalized == "false" || normalized == "0" ||
        normalized == "no" || normalized == "off") {
        value = false;
        return true;
    }
    return false;
}

bool ReadOptionalBool(const Ini &ini, const std::string &section,
                      const std::string &key, bool fallback, bool &value,
                      std::string &error) {
    std::string raw;
    if (!ReadRaw(ini, section, key, raw)) {
        value = fallback;
        return true;
    }

    if (!ParseBool(raw, value)) {
        error = "invalid boolean for [" + section + "] " + key +
                ": " + raw;
        return false;
    }
    return true;
}

bool ParseSize(const std::string &text, std::size_t &value) {
    const std::string normalized = Trim(text);
    if (normalized.empty() || normalized.front() == '-') {
        return false;
    }

    char *end = nullptr;
    errno = 0;
    const unsigned long long parsed =
        std::strtoull(normalized.c_str(), &end, 0);
    if (errno == ERANGE || end == normalized.c_str() || *end != '\0' ||
        parsed > (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }

    value = static_cast<std::size_t>(parsed);
    return true;
}

bool ReadOptionalSize(const Ini &ini, const std::string &section,
                      const std::string &key, std::size_t fallback,
                      std::size_t &value, std::string &error) {
    std::string raw;
    if (!ReadRaw(ini, section, key, raw)) {
        value = fallback;
        return true;
    }

    if (!ParseSize(raw, value)) {
        error = "invalid non-negative integer for [" + section + "] " +
                key + ": " + raw;
        return false;
    }
    return true;
}

bool ParseIni(const std::string &path, Ini &ini, std::string &error) {
    std::ifstream input(path);
    if (!input.is_open()) {
        error = "could not open " + path;
        return false;
    }

    std::string currentSection;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed.front() == '#' ||
            trimmed.front() == ';') {
            continue;
        }

        if (trimmed.front() == '[') {
            if (trimmed.back() != ']') {
                error = "unterminated section at " + path + ":" +
                        std::to_string(lineNumber);
                return false;
            }

            currentSection =
                Lower(Trim(trimmed.substr(1, trimmed.size() - 2)));
            if (currentSection.empty()) {
                error = "empty section at " + path + ":" +
                        std::to_string(lineNumber);
                return false;
            }
            ini.emplace(currentSection, Section{});
            continue;
        }

        if (currentSection.empty()) {
            error = "key before section at " + path + ":" +
                    std::to_string(lineNumber);
            return false;
        }

        const auto separator = trimmed.find('=');
        if (separator == std::string::npos) {
            error = "expected key=value at " + path + ":" +
                    std::to_string(lineNumber);
            return false;
        }

        const std::string key = Lower(Trim(trimmed.substr(0, separator)));
        const std::string value = Trim(trimmed.substr(separator + 1));
        if (key.empty() || value.empty()) {
            error = "empty key or value at " + path + ":" +
                    std::to_string(lineNumber);
            return false;
        }

        auto &section = ini[currentSection];
        if (!section.emplace(key, value).second) {
            error = "duplicate key [" + currentSection + "] " + key +
                    " at " + path + ":" + std::to_string(lineNumber);
            return false;
        }
    }

    return true;
}

bool ReadSignature(const Ini &ini, const char *sectionName,
                   const char *displayName, Signature &signature,
                   std::string &error) {
    signature = {};
    signature.name = displayName;

    if (!ReadRequiredString(ini, sectionName, "module", signature.module,
                            error) ||
        !ReadRequiredString(ini, sectionName, "pattern", signature.pattern,
                            error) ||
        !ReadRequiredString(ini, sectionName, "operand", signature.operand,
                            error) ||
        !ReadRequiredString(ini, sectionName, "source", signature.source,
                            error) ||
        !ReadRequiredString(ini, sectionName, "discovery",
                            signature.discovery, error)) {
        return false;
    }

    if (!ReadOptionalSize(ini, sectionName, "operand_offset", 0,
                          signature.operandOffset, error) ||
        !ReadOptionalSize(ini, sectionName, "instruction_offset", 0,
                          signature.instructionOffset, error) ||
        !ReadOptionalSize(ini, sectionName, "instruction_length", 0,
                          signature.instructionLength, error) ||
        !ReadOptionalSize(ini, sectionName, "indirections", 0,
                          signature.indirections, error) ||
        !ReadOptionalBool(ini, sectionName, "required", true,
                          signature.required, error) ||
        !ReadOptionalBool(ini, sectionName, "validate_vtable", false,
                          signature.validateVtable, error) ||
        !ReadOptionalString(ini, sectionName, "description", "",
                            signature.description, error) ||
        !ReadOptionalString(ini, sectionName, "source_readme", "",
                            signature.sourceReadme, error) ||
        !ReadOptionalString(ini, sectionName, "source_video", "",
                            signature.sourceVideo, error) ||
        !ReadOptionalString(ini, sectionName, "notes", "",
                            signature.notes, error)) {
        return false;
    }

    const auto operand = Lower(signature.operand);
    if (operand != "abs32" && operand != "rip_rel32") {
        error = std::string("unsupported operand type for ") + displayName +
                ": " + signature.operand;
        return false;
    }
    if (operand == "rip_rel32" && signature.instructionLength == 0) {
        error = std::string("rip_rel32 requires instruction_length for ") +
                displayName;
        return false;
    }
    return true;
}

bool ReadInterface(const Ini &ini, const char *sectionName,
                   const char *displayName, Interface &interfaceDefinition,
                   std::string &error) {
    interfaceDefinition = {};

    if (!ReadRequiredString(ini, sectionName, "name",
                            interfaceDefinition.name, error) ||
        !ReadRequiredString(ini, sectionName, "module",
                            interfaceDefinition.module, error) ||
        !ReadRequiredString(ini, sectionName, "source",
                            interfaceDefinition.source, error) ||
        !ReadRequiredString(ini, sectionName, "discovery",
                            interfaceDefinition.discovery, error)) {
        return false;
    }

    if (!ReadOptionalBool(ini, sectionName, "required", true,
                          interfaceDefinition.required, error) ||
        !ReadOptionalString(ini, sectionName, "source_readme", "",
                            interfaceDefinition.sourceReadme, error) ||
        !ReadOptionalString(ini, sectionName, "notes", "",
                            interfaceDefinition.notes, error)) {
        return false;
    }

    if (interfaceDefinition.required && interfaceDefinition.name.empty()) {
        error = std::string("required interface name is empty for ") +
                displayName;
        return false;
    }
    return true;
}

bool GetSelfPath(HMODULE selfModule, std::string &path, std::string &error) {
    if (selfModule == nullptr) {
        error = "the DLL module handle is null";
        return false;
    }

    std::vector<char> buffer(512);
    for (int attempt = 0; attempt < 8; ++attempt) {
        const DWORD length = GetModuleFileNameA(
            selfModule, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            error = "GetModuleFileNameA failed (" +
                    std::to_string(GetLastError()) + ")";
            return false;
        }

        if (length < buffer.size() - 1) {
            path.assign(buffer.data(), length);
            return true;
        }
        buffer.resize(buffer.size() * 2);
    }

    error = "the DLL path is longer than the supported limit";
    return false;
}

} // namespace

bool Load(HMODULE selfModule, std::string &error) {
    g_loaded = false;
    g_config = {};

    std::string selfPath;
    if (!GetSelfPath(selfModule, selfPath, error)) {
        return false;
    }

    const auto separator = selfPath.find_last_of("/\\");
    if (separator == std::string::npos) {
        error = "could not determine the DLL directory from " + selfPath;
        return false;
    }

    Config candidate;
    candidate.path = selfPath.substr(0, separator + 1) + "signatures.ini";

    Ini ini;
    if (!ParseIni(candidate.path, ini, error)) {
        return false;
    }

    if (!ReadOptionalBool(ini, "settings", "require_unique", true,
                          candidate.settings.requireUnique, error) ||
        !ReadOptionalBool(ini, "settings", "validate_pointers", true,
                          candidate.settings.validatePointers, error) ||
        !ReadOptionalBool(ini, "settings", "log_match_offsets", true,
                          candidate.settings.logMatchOffsets, error) ||
        !ReadOptionalBool(ini, "features", "bhop", true,
                          candidate.features.bhop, error) ||
        !ReadOptionalBool(ini, "features", "aimbot", true,
                          candidate.features.aimbot, error) ||
        !ReadOptionalBool(ini, "features", "triggerbot", true,
                          candidate.features.triggerbot, error) ||
        !ReadOptionalBool(ini, "features", "norecoil", true,
                          candidate.features.norecoil, error) ||
        !ReadOptionalBool(ini, "features", "visual_norecoil", true,
                          candidate.features.visualNoRecoil, error) ||
        !ReadOptionalBool(ini, "features", "perfect_nospread", false,
                          candidate.features.perfectNoSpread, error) ||
        !ReadOptionalBool(ini, "features", "silent_angles", true,
                          candidate.features.silentAngles, error) ||
        !ReadOptionalBool(ini, "features", "menu_open", false,
                          candidate.features.menuOpen, error) ||
        !ReadSignature(ini, "signature.clientstate", "ClientState",
                       candidate.clientState, error) ||
        !ReadSignature(ini, "signature.clientmode", "ClientMode",
                       candidate.clientMode, error) ||
        !ReadInterface(ini, "interface.cliententitylist", "ClientEntityList",
                       candidate.clientEntityList, error) ||
        !ReadInterface(ini, "interface.engineclient", "EngineClient",
                       candidate.engineClient, error)) {
        return false;
    }

    if (candidate.clientState.required && candidate.clientState.indirections != 0) {
        error = "ClientState must use indirections=0";
        return false;
    }
    const auto clientModeOperand = Lower(candidate.clientMode.operand);
    if (candidate.clientMode.required &&
        ((clientModeOperand == "abs32" &&
          candidate.clientMode.indirections != 1) ||
         (clientModeOperand == "rip_rel32" &&
          candidate.clientMode.indirections != 0))) {
        error = "ClientMode indirections do not match its operand type";
        return false;
    }

    g_config = std::move(candidate);
    g_loaded = true;
    error.clear();
    return true;
}

const Config &Get() {
    return g_config;
}

bool IsLoaded() {
    return g_loaded;
}

} // namespace config
