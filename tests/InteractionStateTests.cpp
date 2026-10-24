#include "InteractionState.h"
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
    std::cout << "InteractionState: all regression checks passed\n";
}
