#include "InteractionState.h"
#include "FurnitureRules.h"
#include <array>
#include <cstdlib>
#include <iostream>

static void Check(bool value, const char* test) {
    if (!value) { std::cerr << "FAIL: " << test << '\n'; std::exit(1); }
}
int main() {
    using HFN::Posture;
    HFN::InteractionState s;
    Check(s.Observe(0) == Posture::Free, "standing is free");
    s.Enter(10);
    Check(s.Observe(1) == Posture::Free && s.furniture == 10, "walking to furniture is not seated yet");
    Check(s.Observe(2) == Posture::Free, "waiting for sit animation is not seated yet");
    Check(s.Observe(3) == Posture::Seated, "sitting");
    Check(s.Observe(4) == Posture::Seated, "getting up keeps the furniture pose like GetSitState() >= 3");
    s.Enter(20);
    s.Exit(10);
    Check(s.furniture == 20, "old furniture exit cannot cancel new enter");
    s.Exit(20);
    Check(s.furniture == 0, "exit forgets furniture");
    s.Enter(30);
    Check(s.Observe(0) == Posture::Free && !s.furniture, "standing discards stale enter reference");
    Check(s.Observe(5) == Posture::Free && s.Observe(6) == Posture::Free, "not asleep before the sleep animation");
    Check(s.Observe(7) == Posture::Asleep, "sleep");
    Check(s.Observe(8) == Posture::Asleep, "waking keeps the bed pose like GetSleepState() >= 3");
    using HFN::Rules::ClassifySpecialFurniture;
    using HFN::Rules::SpecialFurnitureFlags;
    using HFN::Rules::SpecialFurnitureRule;
    Check(ClassifySpecialFurniture({ .wallLean = true }) == SpecialFurnitureRule::NoCorrection,
        "wall lean marker must not fall through to chair correction");
    Check(ClassifySpecialFurniture({ .wallLean = true, .invisibleChair = true }) == SpecialFurnitureRule::NoCorrection,
        "wall marker takes priority over any generic/invisible-chair classification");
    Check(ClassifySpecialFurniture({ .railLean = true }) == SpecialFurnitureRule::LeanForward,
        "rail lean marker uses forward lean");
    Check(ClassifySpecialFurniture({ .oven = true }) == SpecialFurnitureRule::LeanForwardSlight,
        "oven uses slight forward lean");
    Check(ClassifySpecialFurniture({ .railLean = true, .oven = true }) == SpecialFurnitureRule::LeanForward,
        "rail marker takes priority over the generic cooking-station rule");
    Check(ClassifySpecialFurniture({ .cookingPot = true }) == SpecialFurnitureRule::LeanForwardSlight,
        "cooking pot uses slight forward lean");
    Check(ClassifySpecialFurniture({ .smelter = true }) == SpecialFurnitureRule::LeanForwardSlight,
        "smelter uses slight forward lean");
    Check(ClassifySpecialFurniture({ .wallLean = false, .invisibleChair = true }) == SpecialFurnitureRule::InvisibleChair,
        "invisible chair marker receives chair correction");
    constexpr std::array<std::uint32_t, 11> invisibleChairForms{
        0x0006411B, 0x00037A1E, 0x00037A1F, 0x00037A29, 0x00037A2E,
        0x00037A46, 0x00037A47, 0x00037AE4, 0x00037C94, 0x000A035C, 0x00017041
    };
    for (const auto formID : invisibleChairForms) {
        Check(HFN::Rules::IsVanillaInvisibleChair(formID), "known vanilla invisible chair marker recognized");
    }
    Check(!HFN::Rules::IsVanillaInvisibleChair(0x00052FF5), "wall marker must not be recognized as invisible chair");

    using Tracker = HFN::Rules::HeelOffsetTracker;
    using Clock = Tracker::Clock;
    Tracker tracker;
    const auto t0 = Clock::time_point{} + std::chrono::seconds(10);
    auto sample = tracker.Update(true, 7.6f, false, t0, std::chrono::milliseconds(1500), std::chrono::milliseconds(500));
    Check(sample.effectiveHeel == 7.6f && sample.stablePresence && sample.effectivePresence, "initial RaceMenu heel offset is cached");
    sample = tracker.Update(false, 0.0f, true, t0 + std::chrono::milliseconds(250), std::chrono::milliseconds(1500), std::chrono::milliseconds(500));
    Check(sample.effectiveHeel == 7.6f && sample.usingCachedHeel && sample.stablePresence && sample.effectivePresence,
        "single missing poll during furniture use preserves heel height");
    sample = tracker.Update(true, 7.6f, true, t0 + std::chrono::milliseconds(500), std::chrono::milliseconds(1500), std::chrono::milliseconds(500));
    Check(sample.effectiveHeel == 7.6f && sample.stablePresence,
        "short override disappearance does not flip stable presence");
    sample = tracker.Update(false, 0.0f, true, t0 + std::chrono::milliseconds(750), std::chrono::milliseconds(1500), std::chrono::milliseconds(500));
    Check(sample.stablePresence, "root override remains stable during debounce");
    sample = tracker.Update(false, 0.0f, true, t0 + std::chrono::milliseconds(1300), std::chrono::milliseconds(1500), std::chrono::milliseconds(500));
    Check(!sample.stablePresence && !sample.effectivePresence && sample.effectiveHeel == 7.6f && sample.usingCachedHeel,
        "persistent root override loss is recognized without losing the cached heel height");
    sample = tracker.Update(true, 7.6f, true, t0 + std::chrono::milliseconds(1400), std::chrono::milliseconds(1500), std::chrono::milliseconds(500));
    Check(!sample.stablePresence && sample.effectivePresence && sample.effectiveHeel == 7.6f,
        "returning RaceMenu offset disables HFN fallback immediately, before stable state settles");
    sample = tracker.Update(false, 0.0f, false, t0 + std::chrono::seconds(3), std::chrono::milliseconds(1500), std::chrono::milliseconds(500));
    Check(sample.effectiveHeel == 0.0f && !sample.usingCachedHeel,
        "cached heel height expires after furniture interaction and grace period");
    std::cout << "InteractionState + furniture marker + heel-offset tracker: all regression checks passed\n";
}
