#pragma once
#include <cstdint>
#include <chrono>
#include <cmath>

namespace HFN::Rules
{
    // These rules are deliberately limited to special marker families whose behavior
    // is documented by Heels Fix 1.9. General furniture falls through to Engine::Categorize.
    enum class SpecialFurnitureRule : std::uint8_t
    {
        Default,
        NoCorrection,       // WallLeanMarker / wall-use markers
        LeanForward,        // RailLeanMarker: leaning against a rail/fence
        LeanForwardSlight,  // ovens, cooking pots and smelters
        InvisibleChair      // explicit vanilla invisible sitting markers
    };

    struct SpecialFurnitureFlags
    {
        bool wallLean{};
        bool railLean{};
        bool invisibleChair{};
        bool oven{};
        bool cookingPot{};
        bool smelter{};
    };

    constexpr SpecialFurnitureRule ClassifySpecialFurniture(const SpecialFurnitureFlags& f) noexcept
    {
        // A wall marker must never fall through into the generic TESFurniture=>Chair path.
        if (f.wallLean) return SpecialFurnitureRule::NoCorrection;
        // Some of these marker records also carry ordinary furniture keywords. The marker's
        // explicit intent takes precedence over generic chair/table classification.
        if (f.invisibleChair) return SpecialFurnitureRule::InvisibleChair;
        if (f.railLean) return SpecialFurnitureRule::LeanForward;
        if (f.oven) return SpecialFurnitureRule::LeanForwardSlight;
        if (f.cookingPot || f.smelter) return SpecialFurnitureRule::LeanForwardSlight;
        return SpecialFurnitureRule::Default;
    }

    constexpr bool IsVanillaInvisibleChair(std::uint32_t formID) noexcept
    {
        // Skyrim.esm invisible chair variants. The high byte is zero for Skyrim.esm forms.
        // Includes the standard, directional, double/triple, player and child markers.
        switch (formID) {
        case 0x0006411B: // InvisibleChairMarker
        case 0x00037A1E: // InvisibleChairMarkerF
        case 0x00037A1F: // InvisibleChairMarkerFL
        case 0x00037A29: // InvisibleChairMarkerFR
        case 0x00037A2E: // InvisibleChairMarkerLR
        case 0x00037A46: // InvisibleChairTripleMarker
        case 0x00037A47: // InvisibleChairDoubleMarker
        case 0x00037AE4: // InvisibleChairPlayerSingleFL
        case 0x00037C94: // InvisibleChairPlayerSingleFR
        case 0x000A035C: // InvisibleChairMarkerChild
        case 0x00017041: // InvisibleChairMarkerFChild
            return true;
        default:
            return false;
        }
    }
}

namespace HFN::Rules
{
    struct HeelOffsetSample
    {
        float effectiveHeel{};
        bool usingCachedHeel{};
        bool rawPresenceChanged{};
        bool stablePresence{};
        bool effectivePresence{};
    };

    // RaceMenu's NPC/internal override can disappear briefly while a furniture animation
    // transitions. Keep the last positive offset through active furniture use, and debounce
    // presence transitions so one missing poll does not clear all HFN pose transforms.
    class HeelOffsetTracker
    {
    public:
        using Clock = std::chrono::steady_clock;

        HeelOffsetSample Update(bool rawPresent, float rawHeight, bool interactionActive,
            Clock::time_point now, Clock::duration grace, Clock::duration debounce) noexcept
        {
            const bool rawChanged = _rawKnown && rawPresent != _lastRawPresent;
            _rawKnown = true;
            _lastRawPresent = rawPresent;

            if (!std::isfinite(rawHeight)) rawHeight = 0.0f;
            if (rawPresent && rawHeight > 0.01f) {
                _lastValidHeight = rawHeight;
                _lastSeen = now;
            }

            if (!_stableKnown) {
                _stableKnown = true;
                _stablePresent = rawPresent;
            } else if (rawPresent == _stablePresent) {
                _pendingKnown = false;
            } else if (!_pendingKnown || _pendingPresent != rawPresent) {
                _pendingKnown = true;
                _pendingPresent = rawPresent;
                _pendingSince = now;
            } else if (now - _pendingSince >= debounce) {
                _stablePresent = rawPresent;
                _pendingKnown = false;
            }

            bool usingCache = false;
            float effective = rawHeight > 0.01f ? rawHeight : 0.0f;
            if (effective <= 0.01f && _lastValidHeight > 0.01f) {
                const bool withinGrace = _lastSeen != Clock::time_point{} && now - _lastSeen <= grace;
                if (interactionActive || withinGrace) {
                    effective = _lastValidHeight;
                    usingCache = true;
                } else {
                    _lastValidHeight = 0.0f;
                }
            }
            // A returning positive override must take effect immediately so a temporary
            // HFN fallback does not stack with RaceMenu for another debounce interval.
            // A missing override is considered absent only after the debounce has elapsed.
            const bool effectivePresence = rawPresent || _stablePresent;
            return { effective, usingCache, rawChanged, _stablePresent, effectivePresence };
        }

    private:
        float _lastValidHeight{};
        Clock::time_point _lastSeen{};
        Clock::time_point _pendingSince{};
        bool _stablePresent{};
        bool _stableKnown{};
        bool _pendingKnown{};
        bool _pendingPresent{};
        bool _lastRawPresent{};
        bool _rawKnown{};
    };
}
