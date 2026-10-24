#pragma once
#include "InteractionState.h"

namespace HFN
{
    enum class FurnitureClass : std::uint8_t { None, Ground, Chair, Carriage, Counter, Table, Bed, BedRoll, Wheel, Throne, Count };
    enum Adjustment : std::uint16_t {
        A_None = 0, A_Root = 0x1, A_Pelvis = 0x2, A_Lean = 0x4, A_Hand = 0x8,
        A_Hip = 0x10, A_RHip = 0x20, A_Body = 0x40,
        A_Sitting = A_Root | A_Pelvis | A_Hand,
        A_Kneel = A_Root | A_Hip,
        A_Spine = A_Lean | A_Hand,
        A_Chop = A_Root | A_Body | A_Lean
    };

    struct ActorInfo
    {
        float heelHeight{};
        float trueScale{ 1.0f };
        Adjustment adjustment{ A_None };
        FurnitureClass furniture{ FurnitureClass::None };
        bool tracked{};
    };

    class Engine
    {
    public:
        static Engine& Get();
        void Init();
        void SetReady(bool ready);
        void OnGameLoaded();
        void SettingsChanged();
        void ClearAll();
        void Refresh(RE::Actor* actor);
        void FurnitureChanged(RE::Actor* actor, RE::TESObjectREFR* furniture, bool entering);
        void ObjectLoaded(RE::FormID id);
        ActorInfo Info(RE::Actor* actor) const;
        std::string Status(RE::Actor* actor) const;
        RE::Actor* MenuTarget() const;
        bool MuJointFixFound() const { return _muJointFix; }

    private:
        struct Record
        {
            float heelHeight{};
            float trueScale{ 1.0f };
            Adjustment adjustment{ A_None };
            FurnitureClass furniture{ FurnitureClass::None };
            std::chrono::steady_clock::time_point lastRefresh{};
            bool female{};
            bool initialized{};
            InteractionState interaction;
            const RE::NiAVObject* skeleton{};
        };

        void QueueTick();
        void Tick();
        void UpdateActor(RE::Actor* actor);
        void Apply(RE::Actor* actor, Record& record, Adjustment adjustment, FurnitureClass furniture, float heel, float scale);
        void Clear(RE::Actor* actor, Record* record);
        std::pair<Adjustment, FurnitureClass> Categorize(RE::Actor* actor, bool advanced, Posture posture, RE::TESObjectREFR* eventFurniture) const;
        float TrueScale(RE::Actor* actor) const;
        void SetCamera(float offset);
        void RestoreCamera();
        void CycleFootwear(RE::Actor* actor);

        mutable std::mutex _lock;
        std::unordered_map<RE::FormID, Record> _records;
        std::atomic_bool _ready{};
        std::atomic_uint64_t _queued{}; // generation + 1; zero means no pending task
        std::atomic_bool _started{};
        std::atomic_uint64_t _generation{};
        bool _cameraOwned{};
        float _cameraIdleBefore{}, _cameraCombatBefore{}, _cameraIdleWritten{}, _cameraCombatWritten{};
        std::atomic_uint32_t _scanIntervalMs{ 250 };
        bool _muJointFix{};
    };
}
