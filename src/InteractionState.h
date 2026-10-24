#pragma once
#include <cstdint>

namespace HFN {
    // Mirrors Heels Fix 1.9: GetSitState() >= 3 (sitting / wants to stand) and
    // GetSleepState() >= 3 (sleeping / wants to wake). Values are RE::SIT_SLEEP_STATE and
    // must be read through Actor::AsActorState() - the ActorState base moved in AE 1.6.629.
    // No engine dependencies: tested independently of Skyrim.
    enum class Posture : std::uint8_t { Free, Seated, Asleep };

    struct InteractionState {
        std::uint32_t furniture{};
        void Enter(std::uint32_t id) { furniture = id; }
        void Exit(std::uint32_t id) {
            if (!id || id == furniture) furniture = 0; // a late exit from older furniture keeps the new one
        }
        Posture Observe(unsigned state) {
            if (state == 0) furniture = 0;
            if (state == 3 || state == 4) return Posture::Seated;
            if (state == 7 || state == 8) return Posture::Asleep;
            return Posture::Free;
        }
    };
}
