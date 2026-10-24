#pragma once

namespace HFN
{
    struct Settings
    {
        bool playerEnabled{ false };
        bool npcEnabled{ true };
        bool playerAdvanced{ false };
        bool npcAdvanced{ true };
        bool correctScaledActors{ true };
        bool correctScaledNoHeel{ false };
        bool correctFirstPerson{ false };
        bool correctThirdPerson{ false };
        bool mjfFurnitureDisable{ true };
        bool debugLog{ false };

        float thighFactor{ 35.0f };
        float thighFactorSS{ 35.0f };
        float leanForwardFactor{ 50.0f };
        float leanBackFactor{ 60.0f };
        float scalingDeviation{ 0.035f };
        float cameraIdleZ{ -10.0f };
        float cameraCombatZ{ 20.0f };
        float mjfKneeFactor{ 3.0f };
        float heelRefreshMinutes{ 0.0f };
        float scanInterval{ 0.25f };

        static Settings& Get();
        static void Load();
        static bool Save();
        static bool Reset();
    };
}
