#include "Settings.h"

namespace HFN
{
    namespace
    {
        constexpr auto kPath = "Data/SKSE/Plugins/HeelsFixNative.ini"sv;

        std::string Trim(std::string_view a_text)
        {
            const auto first = a_text.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos) return {};
            const auto last = a_text.find_last_not_of(" \t\r\n");
            return std::string(a_text.substr(first, last - first + 1));
        }

        bool Equal(std::string_view a, std::string_view b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
                return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
            });
        }

        void Set(Settings& s, std::string_view key, const std::string& value)
        {
            const bool boolean = value == "1" || Equal(value, "true");
#define BOOL_KEY(name) if (Equal(key, #name)) { s.name = boolean; return; }
                        BOOL_KEY(npcEnabled) BOOL_KEY(npcAdvanced)
            BOOL_KEY(correctScaledActors)
            BOOL_KEY(mjfFurnitureDisable) BOOL_KEY(debugLog)
#undef BOOL_KEY
            try {
                std::size_t consumed{};
                const float number = std::stof(value, &consumed);
                if (!std::isfinite(number) || consumed != value.size()) return;
#define FLOAT_KEY(name) if (Equal(key, #name)) { s.name = number; return; }
                FLOAT_KEY(thighFactor) FLOAT_KEY(thighFactorSS) FLOAT_KEY(leanForwardFactor)
                FLOAT_KEY(leanBackFactor) FLOAT_KEY(scalingDeviation) FLOAT_KEY(mjfKneeFactor)
                FLOAT_KEY(scanInterval)
#undef FLOAT_KEY
            } catch (...) {}
        }
    }

    Settings& Settings::Get()
    {
        static Settings value;
        return value;
    }

    void Settings::Load()
    {
        Settings result;
        std::ifstream in{ std::filesystem::path(kPath) };
        std::string line;
        while (std::getline(in, line)) {
            if (const auto comment = line.find_first_of(";#"); comment != std::string::npos) line.erase(comment);
            line = Trim(line);
            if (line.empty() || line.front() == '[') continue;
            const auto eq = line.find('=');
            if (eq != std::string::npos) Set(result, Trim(std::string_view(line).substr(0, eq)), Trim(std::string_view(line).substr(eq + 1)));
        }
        result.scanInterval = std::clamp(result.scanInterval, 0.1f, 5.0f);
        result.thighFactor = std::max(result.thighFactor, 1.0f);
        result.thighFactorSS = std::max(result.thighFactorSS, 1.0f);
        result.leanForwardFactor = std::max(result.leanForwardFactor, 1.0f);
        result.leanBackFactor = std::max(result.leanBackFactor, 1.0f);
        result.scalingDeviation = std::max(result.scalingDeviation, 0.0f);
        result.mjfKneeFactor = std::max(result.mjfKneeFactor, 0.0f);
        result.heelRefreshMinutes = std::max(result.heelRefreshMinutes, 0.0f);
        Get() = result;
        spdlog::set_level(result.debugLog ? spdlog::level::debug : spdlog::level::info);
    }

    bool Settings::Save()
    {
        const auto path = std::filesystem::path(kPath);
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::trunc);
        if (!out) return false;
        const auto& s = Get();
        out << "; Heels Fix Native - SKSE Menu Framework writes this file\n[Actors]\n"
            << "npcEnabled = " << s.npcEnabled
            << "\nnpcAdvanced = " << s.npcAdvanced
            << "\ncorrectScaledActors = " << s.correctScaledActors
            << "\nscanInterval = " << s.scanInterval
            << "\n\n[Factors]\nthighFactor = " << s.thighFactor << "\nthighFactorSS = " << s.thighFactorSS
            << "\nleanForwardFactor = " << s.leanForwardFactor << "\nleanBackFactor = " << s.leanBackFactor
            << "\nscalingDeviation = " << s.scalingDeviation << "\nmjfFurnitureDisable = " << s.mjfFurnitureDisable
            << "\nmjfKneeFactor = " << s.mjfKneeFactor
            << "\n\n[Debug]\ndebugLog = " << s.debugLog << '\n';
        return static_cast<bool>(out);
    }

    bool Settings::Reset()
    {
        Get() = Settings{};
        return Save();
    }
}
