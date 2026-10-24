#include <chrono>
#include <cmath>
#include <cstdint>
#include "Skee.h"

namespace HFN::Skee
{
    namespace {
        SKEE::INiTransformInterface* g_transform{};
        SKEE::Legacy::INiTransformInterface* g_legacyTransform{};
        // RaceMenu's legacy OverrideVariant keys: node position=31, node rotation=32.
        // These are API keys, not guessed transform values.
        constexpr std::uint16_t kPositionKey = 31;
        constexpr std::uint16_t kRotationKey = 32;

        bool LegacyRead(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key, std::uint16_t transformKey, RE::NiTransform& result)
        {
            if (!g_legacyTransform || !actor) return false;
            SKEE::Legacy::FixedString n(node);
            SKEE::Legacy::FixedString k(key);
            result = {};
            return g_legacyTransform->GetOverrideNodeTransform(actor, firstPerson, female, n, k, transformKey, &result);
        }

        void LegacyWriteFloat(RE::Actor* actor, bool firstPerson, bool female, const char* node, const char* key,
            std::uint16_t transformKey, std::int8_t index, float value)
        {
            if (!g_legacyTransform || !actor || !std::isfinite(value)) return;
            SKEE::Legacy::FixedString n(node);
            SKEE::Legacy::FixedString k(key);
            SKEE::Legacy::OverrideVariant v;
            v.SetFloat(transformKey, value);
            v.index = index;
            g_legacyTransform->AddNodeTransform(actor, firstPerson, female, n, k, v);
        }
    }

    bool Connect()
    {
        if (g_transform || g_legacyTransform) return true;
        static auto lastWarning = std::chrono::steady_clock::time_point{};
        static auto lastAttempt = std::chrono::steady_clock::time_point{};
        const auto now = std::chrono::steady_clock::now();
        if (lastAttempt != std::chrono::steady_clock::time_point{} && now - lastAttempt < std::chrono::seconds(2)) return false;
        lastAttempt = now;
        const auto map = SKEE::GetInterfaceMap();
        auto* base = map ? map->QueryInterface("NiTransform") : nullptr;
        if (!base) {
            if (lastWarning == std::chrono::steady_clock::time_point{} || now - lastWarning >= std::chrono::seconds(15)) {
                logger::warn("RaceMenu NiTransform interface not ready; retrying without disabling the plugin");
                lastWarning = now;
            }
            return false;
        }
        const auto version = base->GetVersion();
        if (version >= SKEE::INiTransformInterface::Version) {
            g_transform = static_cast<SKEE::INiTransformInterface*>(base);
            logger::info("Connected to RaceMenu NiTransform v{}", version);
            return true;
        }
        if (version == 2) {
            g_legacyTransform = reinterpret_cast<SKEE::Legacy::INiTransformInterface*>(base);
            logger::info("Connected to RaceMenu NiTransform v2 (legacy SE ABI)");
            return true;
        }
        if (lastWarning == std::chrono::steady_clock::time_point{} || now - lastWarning >= std::chrono::seconds(15)) {
            logger::warn("RaceMenu NiTransform v{} is unsupported; expected v2 or v3+", version);
            lastWarning = now;
        }
        return false;
    }
    bool Ready() { return g_transform != nullptr || g_legacyTransform != nullptr; }

    bool HasPosition(RE::Actor* a, bool fp, bool f, const char* n, const char* k)
    {
        if (g_transform) return g_transform->HasNodeTransformPosition(a, fp, f, n, k);
        RE::NiTransform result{};
        return LegacyRead(a, fp, f, n, k, kPositionKey, result);
    }
    bool HasRotation(RE::Actor* a, bool fp, bool f, const char* n, const char* k)
    {
        if (g_transform) return g_transform->HasNodeTransformRotation(a, fp, f, n, k);
        RE::NiTransform result{};
        return LegacyRead(a, fp, f, n, k, kRotationKey, result);
    }
    SKEE::INiTransformInterface::Position Position(RE::Actor* a, bool fp, bool f, const char* n, const char* k)
    {
        if (g_transform) return g_transform->GetNodeTransformPosition(a, fp, f, n, k);
        RE::NiTransform result{};
        if (!LegacyRead(a, fp, f, n, k, kPositionKey, result)) return {};
        return { result.translate.x, result.translate.y, result.translate.z };
    }
    float HeelHeight(RE::Actor* a, bool f)
    {
        return HasPosition(a, false, f, "NPC", "internal") ? Position(a, false, f, "NPC", "internal").z : 0.0f;
    }
    void SetPosition(RE::Actor* a, bool fp, bool f, const char* n, const char* k, float x, float y, float z)
    {
        if (g_transform) {
            if (!a) return;
            SKEE::INiTransformInterface::Position v{ x, y, z };
            g_transform->AddNodeTransformPosition(a, fp, f, n, k, v);
            return;
        }
        LegacyWriteFloat(a, fp, f, n, k, kPositionKey, 0, x);
        LegacyWriteFloat(a, fp, f, n, k, kPositionKey, 1, y);
        LegacyWriteFloat(a, fp, f, n, k, kPositionKey, 2, z);
    }
    void SetRotation(RE::Actor* a, bool fp, bool f, const char* n, const char* k, float x, float y, float z)
    {
        if (g_transform) {
            if (!a) return;
            SKEE::INiTransformInterface::Rotation v{ x, y, z };
            g_transform->AddNodeTransformRotation(a, fp, f, n, k, v);
            return;
        }
        // Legacy v2 stores a 3x3 rotation matrix (9 float components), not the v3 Euler triplet.
        constexpr float kDegToRad = 0.01745329251994329577f;
        const float h = x * kDegToRad, p = y * kDegToRad, b = z * kDegToRad;
        const float ch = std::cos(h), sh = std::sin(h);
        const float cp = std::cos(p), sp = std::sin(p);
        const float cb = std::cos(b), sb = std::sin(b);
        const float m[9] = {
            ch * cp, ch * sp * sb - sh * cb, ch * sp * cb + sh * sb,
            sh * cp, sh * sp * sb + ch * cb, sh * sp * cb - ch * sb,
            -sp, cp * sb, cp * cb
        };
        for (std::int8_t i = 0; i < 9; ++i) LegacyWriteFloat(a, fp, f, n, k, kRotationKey, i, m[i]);
    }
    bool RemovePosition(RE::Actor* a, bool fp, bool f, const char* n, const char* k)
    {
        if (g_transform) return g_transform->RemoveNodeTransformPosition(a, fp, f, n, k);
        if (!g_legacyTransform || !a) return false;
        SKEE::Legacy::FixedString node(n), name(k);
        bool removed = false;
        for (std::uint16_t i = 0; i < 3; ++i) removed |= g_legacyTransform->RemoveNodeTransformComponent(a, fp, f, node, name, kPositionKey, i);
        return removed;
    }
    bool RemoveRotation(RE::Actor* a, bool fp, bool f, const char* n, const char* k)
    {
        if (g_transform) return g_transform->RemoveNodeTransformRotation(a, fp, f, n, k);
        if (!g_legacyTransform || !a) return false;
        SKEE::Legacy::FixedString node(n), name(k);
        bool removed = false;
        for (std::uint16_t i = 0; i < 9; ++i) removed |= g_legacyTransform->RemoveNodeTransformComponent(a, fp, f, node, name, kRotationKey, i);
        return removed;
    }
    bool RemoveTransform(RE::Actor* a, bool fp, bool f, const char* n, const char* k)
    {
        if (g_transform) return g_transform->RemoveNodeTransform(a, fp, f, n, k);
        if (!g_legacyTransform || !a) return false;
        SKEE::Legacy::FixedString node(n), name(k);
        return g_legacyTransform->RemoveNodeTransform(a, fp, f, node, name);
    }
    void Update(RE::Actor* a, bool fp, bool f, const char* n)
    {
        if (!a || !a->Is3DLoaded()) return;
        if (g_transform) { g_transform->UpdateNodeTransforms(a, fp, f, n); return; }
        if (g_legacyTransform) {
            SKEE::Legacy::FixedString node(n);
            g_legacyTransform->UpdateNodeTransforms(a, fp, f, node);
        }
    }
}
