#include "Engine.h"
#include "Settings.h"
#include "Skee.h"

namespace HFN
{
    namespace
    {
        using Clock = std::chrono::steady_clock;
        constexpr auto kKey = "HeelsFixNG.esp";
        constexpr auto kRoot = "NPC";
        constexpr auto kAltRoot = "NPC Root [Root]";
        constexpr auto kPelvis = "CME Pelvis [Pelv]";
        constexpr auto kBody = "CME Body [Body]";
        constexpr auto kLeftHip = "CME L Hip";
        constexpr auto kRightHip = "CME R Hip";
        constexpr auto kSpine = "CME Spine [Spn0]";
        constexpr float kRadToDeg = 57.29577951308232f;

        bool HasKeyword(RE::TESForm* form, std::string_view id)
        {
            const auto* keywordForm = form ? form->As<RE::BGSKeywordForm>() : nullptr;
            return keywordForm && keywordForm->HasKeywordString(id);
        }

        bool AnyKeyword(RE::TESForm* form, std::initializer_list<std::string_view> ids)
        {
            return std::ranges::any_of(ids, [form](auto id) { return HasKeyword(form, id); });
        }

        float SafeAsin(float value) { return std::asin(std::clamp(value, -1.0f, 1.0f)) * kRadToDeg; }
        std::size_t Index(FurnitureClass value) { return static_cast<std::size_t>(value); }

        // ActorState sits at 0xB8 on SE but 0xC0 on AE 1.6.629+. Calling the inherited
        // IsSwimming()/GetSitSleepState() on Actor uses the SE offset and reads garbage on AE.
        const RE::ActorState* State(RE::Actor* actor) { return actor ? actor->AsActorState() : nullptr; }
        bool Swimming(RE::Actor* actor) { auto* s = State(actor); return s && s->IsSwimming(); }
        unsigned SitSleep(RE::Actor* actor) { auto* s = State(actor); return s ? static_cast<unsigned>(s->GetSitSleepState()) : 0; }
        bool WeaponDrawn(RE::Actor* actor) { auto* s = State(actor); return s && s->IsWeaponDrawn(); }

        // Heels Fix 1.9 used GetAnimationVariableBool("bIsRiding").
        bool Mounted(RE::Actor* actor) {
            if (!actor) return false;
            bool riding = false;
            if (actor->GetGraphVariableBool("bIsRiding", riding) && riding) return true;
            RE::NiPointer<RE::Actor> mount;
            return SitSleep(actor) == static_cast<unsigned>(RE::SIT_SLEEP_STATE::kRidingMount) &&
                actor->GetMount(mount) && mount && mount->IsAMount();
        }

    }

    Engine& Engine::Get() { static Engine singleton; return singleton; }

    void Engine::Init()
    {
        if (_started.exchange(true)) return;
        _muJointFix = std::filesystem::exists("Data/SKSE/Plugins/MuJointFix.dll");
        _scanIntervalMs = static_cast<std::uint32_t>(std::clamp(Settings::Get().scanInterval, 0.1f, 5.0f) * 1000.0f);
        logger::info("Mu Joint Fix detected: {}", _muJointFix);
        std::thread([this]() {
            for (;;) {
                const auto interval = std::chrono::milliseconds(_scanIntervalMs.load());
                std::this_thread::sleep_for(interval);
                if (_ready) QueueTick();
            }
        }).detach();
    }

    void Engine::SetReady(bool ready)
    {
        _ready = false;
        ++_generation;
        std::scoped_lock lock(_lock);
        RestoreCamera();
        _records.clear(); // never carry FormIDs, furniture or initialization across saves
        _ready = ready;
    }

    void Engine::OnGameLoaded()
    {
        SettingsChanged();
        QueueTick();
    }

    void Engine::QueueTick()
    {
        if (!_ready) return;
        const auto generation = _generation.load();
        auto pending = _queued.load();
        if (pending == generation + 1 || !_queued.compare_exchange_strong(pending, generation + 1)) return;
        SKSE::GetTaskInterface()->AddTask([this, generation]() {
            auto token = generation + 1;
            _queued.compare_exchange_strong(token, 0);
            if (generation == _generation.load() && _ready) Tick();
        });
    }

    float Engine::TrueScale(RE::Actor* actor) const
    {
        float scale = actor ? actor->GetScale() : 1.0f;
        if (actor) {
            if (auto* node = actor->GetNodeByName(kRoot); node && node->local.scale > 0.0f) scale *= node->local.scale;
            if (auto* node = actor->GetNodeByName(kAltRoot); node && node->local.scale > 0.0f) scale *= node->local.scale;
        }
        return std::isfinite(scale) ? std::max(scale, 0.01f) : 1.0f;
    }

    std::pair<Adjustment, FurnitureClass> Engine::Categorize(RE::Actor* actor, bool advanced, Posture posture, RE::TESObjectREFR* eventFurniture) const
    {
        if (!actor) return { A_None, FurnitureClass::None };
        // Swimming or riding: root adjust is enough (Heels Fix 1.9 CategorizeInteraction)
        if (Swimming(actor) || Mounted(actor)) return { A_Root, FurnitureClass::Ground };
        if (posture == Posture::Free) return { A_None, FurnitureClass::Ground };

        const auto occupied = actor->GetOccupiedFurniture().get();
        auto* furniture = occupied ? occupied.get() : eventFurniture;
        auto* base = furniture ? furniture->GetBaseObject() : nullptr;
        if (posture == Posture::Asleep) return { A_Root, HasKeyword(base, "FurnitureBedRoll") ? FurnitureClass::BedRoll : FurnitureClass::Bed };
        if (!base) return { A_None, FurnitureClass::Ground };
        if (!advanced) return { A_Root, FurnitureClass::Ground };

        {
            if (base->GetFormID() == 0x000F2FF5 || AnyKeyword(base, { "isPickaxeWall", "isHammerWall", "isStandingChisel", "isWalkThroughWall", "WoodPickUp", "StonePickUp", "BucketPour" }))
                return { A_None, FurnitureClass::Ground };
            if (AnyKeyword(base, { "isGreybeardMeditate", "isSitCrossLegged", "isLayDown", "isSitLedge", "isIdlePrayCrouched" }))
                return { A_Root, FurnitureClass::Ground };
            if (AnyKeyword(base, { "isLeanTable", "isPickaxeFloor", "isPickaxeTable", "isHammerTable", "isAlchemy", "isEnchanting", "isGrainMill", "isCraftingOven", "isBlacksmithAnvil", "isBlacksmithWorkbench", "isBlacksmithForge", "isWritingTable", "ActivatorLever", "BucketFill" }))
                return { A_Lean, FurnitureClass::Table };
            if (HasKeyword(base, "FurnitureWoodChoppingBlock")) return { A_Chop, FurnitureClass::Table };
            if (base->GetFormID() == 0x00070EA1 || AnyKeyword(base, { "FurnitureCounterLeanMarker", "FurnitureResourceObjectSawmill", "CraftingCookpot", "isPullChain", "isSmelter" }))
                return { A_Lean, FurnitureClass::Counter };
            if (HasKeyword(base, "isWritingChair")) return { A_Sitting, FurnitureClass::Table };
            if (HasKeyword(base, "isJarlChair")) return { A_Sitting, FurnitureClass::Throne };
            if (HasKeyword(base, "CraftingSmithingSharpeningWheel")) return { A_Sitting, FurnitureClass::Wheel };
            if (AnyKeyword(base, { "isTanning", "isKneelingChisel", "isKneeling" })) return { A_Kneel, FurnitureClass::Ground };
            if (HasKeyword(base, "isCartTravelPlayer")) return { A_Sitting, FurnitureClass::Carriage };
            if (HasKeyword(base, "FurnitureSpecial")) return { A_None, FurnitureClass::Ground };
        }
        if (HasKeyword(base, "isTable")) return { A_Sitting, FurnitureClass::Table };
        if (HasKeyword(base, "isBarStool")) return { A_Sitting, FurnitureClass::Counter };
        return base->As<RE::TESFurniture>() ? std::pair{ A_Sitting, FurnitureClass::Chair } : std::pair{ A_None, FurnitureClass::Ground };
    }

    void Engine::Apply(RE::Actor* actor, Record& record, Adjustment adjustment, FurnitureClass furniture, float heel, float trueScale)
    {
        const auto oldAdjustment = record.adjustment;
        const auto oldFurniture = record.furniture;
        Clear(actor, std::addressof(record));
        record.adjustment = adjustment;
        record.furniture = furniture;
        record.heelHeight = heel;
        record.trueScale = trueScale;
        if (actor->IsPlayerRef() || Settings::Get().debugLog) {
            logger::info("Actor {:08X}: adjustment 0x{:X}/{} -> 0x{:X}/{}, heel {:.3f}, scale {:.3f}",
                actor->GetFormID(), static_cast<unsigned>(oldAdjustment), static_cast<unsigned>(oldFurniture),
                static_cast<unsigned>(adjustment), static_cast<unsigned>(furniture), heel, trueScale);
        }
        if (adjustment == A_None) {
            if (actor->IsPlayerRef()) {
                const auto& s = Settings::Get();
                if (s.correctFirstPerson) {
                    Skee::SetPosition(actor, true, record.female, kRoot, kKey, 0, 0, heel);
                    Skee::Update(actor, true, record.female, kRoot);
                }
                SetCamera(heel);
            }
            return;
        }

        const auto& s = Settings::Get();
        const bool useScale = (heel < 0.2f && s.correctScaledNoHeel) || (heel > 0.15f && s.correctScaledActors);
        const float actorScale = useScale ? trueScale : 1.0f;
        constexpr std::array rootHeight{ -1.0f, 0.0f, 30.0f, 118.0f, 47.0f, 30.0f, 30.0f, 5.0f, 37.0f, 32.0f };
        constexpr std::array handHeight{ -1.0f, -1.0f, -1.0f, -1.0f, 80.0f, 65.0f, -1.0f, -1.0f, 50.0f, 58.0f };
        constexpr std::array footHeight{ -1.0f, 0.0f, 0.0f, 94.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f };
        const auto i = Index(furniture);
        float rootOffset = rootHeight[i] >= 0.0f ? heel * actorScale - (1.0f - actorScale) * rootHeight[i] : 0.0f;
        float kneeOffset = 0.0f;
        float thighAngle = 0.0f;
        if (footHeight[i] >= 0.0f) {
            kneeOffset = footHeight[i] * (1.0f - actorScale) + rootOffset;
            if (_muJointFix && s.mjfKneeFactor > 0.0f) {
                const float mjf = s.mjfKneeFactor * actorScale;
                kneeOffset += mjf;
                if (!s.mjfFurnitureDisable) rootOffset += mjf;
            }
            if (kneeOffset > 0.0f) thighAngle = SafeAsin(kneeOffset / ((furniture == FurnitureClass::Chair ? s.thighFactorSS : s.thighFactor) * actorScale));
        }
        float leanAngle = 0.0f;
        float handAngle = 0.0f;
        if (handHeight[i] > 0.0f) {
            leanAngle = SafeAsin((handHeight[i] * (1.0f - actorScale) - heel * actorScale) / (s.leanForwardFactor * actorScale));
            handAngle = SafeAsin(((handHeight[i] - rootHeight[i]) * (1.0f - actorScale)) / (s.leanBackFactor * actorScale));
        } else if (kneeOffset > 1.0f) {
            handAngle = SafeAsin((kneeOffset - 1.0f) / (s.leanBackFactor * actorScale));
        }

        const bool female = record.female;
        if (adjustment & A_Root) Skee::SetPosition(actor, false, female, kRoot, kKey, 0, 0, -rootOffset / (useScale ? trueScale : 1.0f));
        if (adjustment & A_Pelvis) Skee::SetRotation(actor, false, female, kPelvis, kKey, 0, 0, thighAngle);
        else if (adjustment & A_Hip) Skee::SetRotation(actor, false, female, kLeftHip, kKey, 0, 0, thighAngle);
        else if (adjustment & A_RHip) Skee::SetRotation(actor, false, female, kRightHip, kKey, 0, 0, thighAngle);
        if (adjustment & A_Spine) Skee::SetRotation(actor, false, female, kSpine, kKey, 0, 0, (adjustment & A_Lean) ? leanAngle : handAngle);
        if (adjustment & A_Body) Skee::SetPosition(actor, false, female, kBody, kKey, 0, 0, rootOffset / (useScale ? trueScale : 1.0f));
        if (adjustment & A_Root) Skee::Update(actor, false, female, kRoot);
        if (adjustment & A_Pelvis) Skee::Update(actor, false, female, kPelvis);
        else if (adjustment & A_Hip) Skee::Update(actor, false, female, kLeftHip);
        else if (adjustment & A_RHip) Skee::Update(actor, false, female, kRightHip);
        if (adjustment & A_Spine) Skee::Update(actor, false, female, kSpine);
        if (adjustment & A_Body) Skee::Update(actor, false, female, kBody);

        if (actor->IsPlayerRef()) {
            float cameraOffset = heel;
            if (Skee::HasPosition(actor, false, female, kRoot, kKey)) cameraOffset += Skee::Position(actor, false, female, kRoot, kKey).z;
            if (s.correctFirstPerson) {
                Skee::SetPosition(actor, true, female, kRoot, kKey, 0, 0, cameraOffset);
                Skee::Update(actor, true, female, kRoot);
            }
            SetCamera(cameraOffset);
        }
    }

    void Engine::Clear(RE::Actor* actor, Record* record)
    {
        if (!actor || !Skee::Ready()) return;
        const bool female = actor->GetActorBase() && actor->GetActorBase()->GetSex() == RE::SEX::kFemale;
        bool removed = false;
        for (const auto* node : { kRoot, kBody, kPelvis, kLeftHip, kRightHip, kSpine }) {
            removed |= Skee::RemoveTransform(actor, false, false, node, kKey);
            removed |= Skee::RemoveTransform(actor, false, true, node, kKey);
            Skee::Update(actor, false, female, node);
        }
        if (actor->IsPlayerRef()) {
            removed |= Skee::RemoveTransform(actor, true, false, kRoot, kKey);
            removed |= Skee::RemoveTransform(actor, true, true, kRoot, kKey);
            Skee::Update(actor, true, female, kRoot);
            RestoreCamera();
        }
        if (removed && (actor->IsPlayerRef() || Settings::Get().debugLog)) logger::info("Actor {:08X}: removed HeelsFix transforms", actor->GetFormID());
        if (record) record->adjustment = A_None;
    }

    void Engine::UpdateActor(RE::Actor* actor)
    {
        if (!actor) return;
        if (!actor->Is3DLoaded()) {
            if (auto it = _records.find(actor->GetFormID()); it != _records.end()) {
                Clear(actor, &it->second);
                _records.erase(it);
            }
            return;
        }
        const auto& s = Settings::Get();
        const bool enabled = actor->IsPlayerRef() ? s.playerEnabled : s.npcEnabled;
        auto& record = _records[actor->GetFormID()];
        const bool female = actor->GetActorBase() && actor->GetActorBase()->GetSex() == RE::SEX::kFemale;
        if (record.initialized && (record.female != female || record.skeleton != actor->Get3D())) {
            Clear(actor, &record);
            record = Record{};
        }
        record.female = female;
        record.skeleton = actor->Get3D();
        if (!record.initialized) {
            Clear(actor, std::addressof(record));
            record.heelHeight = -1.0f;
            record.initialized = true;
            record.lastRefresh = Clock::now();
        }
        if (actor->IsDead() || actor->IsChild()) {
            if (record.heelHeight != 0.0f || record.adjustment != A_None) Clear(actor, std::addressof(record));
            record.heelHeight = 0.0f;
            return;
        }
        if (!enabled) {
            if (record.heelHeight != 0.0f || record.adjustment != A_None) Clear(actor, std::addressof(record));
            record.heelHeight = 0.0f;
            return;
        }

        float heel = Skee::HeelHeight(actor, record.female);
        if (!std::isfinite(heel)) { Clear(actor, &record); record.heelHeight = -1.0f; return; }
        const float scale = TrueScale(actor);
        const bool advanced = actor->IsPlayerRef() ? s.playerAdvanced : s.npcAdvanced;
        const bool scaledNoHeel = advanced && s.correctScaledNoHeel && std::abs(scale - 1.0f) > s.scalingDeviation;
        const bool wearing = heel > 0.01f;
        const bool marker = heel > 0.5f;
        if (!wearing && !scaledNoHeel) {
            if (record.heelHeight > 0.0f || record.adjustment != A_None) Clear(actor, std::addressof(record));
            record.heelHeight = 0.0f;
            return;
        }
        if (!wearing) heel = 0.1f;

        const auto sitSleep = SitSleep(actor);
        const auto posture = record.interaction.Observe(sitSleep);
        auto* eventFurniture = record.interaction.furniture ? RE::TESForm::LookupByID<RE::TESObjectREFR>(record.interaction.furniture) : nullptr;
        const auto [adjustment, furniture] = Categorize(actor, advanced, posture, eventFurniture);
        if (record.adjustment != adjustment || record.furniture != furniture || std::abs(record.heelHeight - heel) > 0.001f ||
            std::abs(record.trueScale - scale) > 0.001f) {
            if (actor->IsPlayerRef() || s.debugLog) {
                const auto occupied = actor->GetOccupiedFurniture().get();
                auto* ref = occupied ? occupied.get() : eventFurniture;
                auto* base = ref ? ref->GetBaseObject() : nullptr;
                logger::info("Actor {:08X} state: swim {}, mounted {}, sit {}, furniture {:08X}, base {:08X} '{}', advanced {}",
                    actor->GetFormID(), Swimming(actor), Mounted(actor), sitSleep,
                    ref ? ref->GetFormID() : 0, base ? base->GetFormID() : 0, base ? base->GetName() : "", advanced);
            }
            Apply(actor, record, adjustment, furniture, heel, scale);
        }

        if (s.heelRefreshMinutes > 0.0f && marker && Clock::now() - record.lastRefresh >= std::chrono::duration<float>(s.heelRefreshMinutes * 60.0f)) {
            CycleFootwear(actor);
            record.lastRefresh = Clock::now();
        }
    }

    void Engine::Tick()
    {
        if (!_ready) return;
        if (!Skee::Ready() && !Skee::Connect()) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->Is3DLoaded()) return;
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) return;
        std::scoped_lock lock(_lock);
        std::unordered_set<RE::FormID> seen;
        UpdateActor(player);
        seen.insert(player->GetFormID());
        if (Settings::Get().npcEnabled) {
            if (auto* lists = RE::ProcessLists::GetSingleton()) {
                for (auto& handle : lists->highActorHandles) {
                    const auto ptr = handle.get();
                    auto* actor = ptr.get();
                    if (!actor || actor == player) continue;
                    seen.insert(actor->GetFormID());
                    UpdateActor(actor);
                }
            }
        }
        for (auto it = _records.begin(); it != _records.end();) {
            if (!seen.contains(it->first)) {
                if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(it->first)) Clear(actor, std::addressof(it->second));
                it = _records.erase(it);
            } else ++it;
        }
    }

    void Engine::RestoreCamera()
    {
        if (!_cameraOwned) return;
        if (auto* ini = RE::INISettingCollection::GetSingleton()) {
            if (auto* v = ini->GetSetting("fOverShoulderPosZ:Camera"); v && v->GetFloat() == _cameraIdleWritten) v->SetFloat(_cameraIdleBefore);
            if (auto* v = ini->GetSetting("fOverShoulderCombatPosZ:Camera"); v && v->GetFloat() == _cameraCombatWritten) v->SetFloat(_cameraCombatBefore);
        }
        _cameraOwned = false;
        if (auto* camera = RE::PlayerCamera::GetSingleton()) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            camera->UpdateThirdPerson(WeaponDrawn(player));
        }
    }

    void Engine::SetCamera(float offset)
    {
        const auto& s = Settings::Get();
        if (!s.correctThirdPerson) { RestoreCamera(); return; }
        auto* ini = RE::INISettingCollection::GetSingleton();
        if (!ini) return;
        auto* idle = ini->GetSetting("fOverShoulderPosZ:Camera");
        auto* combat = ini->GetSetting("fOverShoulderCombatPosZ:Camera");
        if (!idle || !combat) return;
        if (!_cameraOwned) { _cameraIdleBefore = idle->GetFloat(); _cameraCombatBefore = combat->GetFloat(); _cameraOwned = true; }
        idle->SetFloat(_cameraIdleWritten = s.cameraIdleZ + offset);
        combat->SetFloat(_cameraCombatWritten = s.cameraCombatZ + offset);
        if (auto* camera = RE::PlayerCamera::GetSingleton()) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            camera->UpdateThirdPerson(WeaponDrawn(player));
        }
    }

    void Engine::CycleFootwear(RE::Actor* actor)
    {
        if (!actor || !_ready) return;
        auto* armor = actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
        auto* manager = RE::ActorEquipManager::GetSingleton();
        if (!armor || !manager) return;
        const auto id = actor->GetFormID();
        manager->UnequipObject(actor, armor, nullptr, 1, nullptr, true, false, false);
        const auto generation = _generation.load();
        const auto armorID = armor->GetFormID();
        SKSE::GetTaskInterface()->AddTask([this, id, armorID, generation]() {
            if (!_ready || generation != _generation.load()) return;
            auto* item = RE::TESForm::LookupByID<RE::TESObjectARMO>(armorID);
            if (auto* target = RE::TESForm::LookupByID<RE::Actor>(id); target && item && !target->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet))
                RE::ActorEquipManager::GetSingleton()->EquipObject(target, item, nullptr, 1, nullptr, true, false, false);
        });
    }

    void Engine::Refresh(RE::Actor* actor)
    {
        if (!actor) return;
        std::scoped_lock lock(_lock);
        CycleFootwear(actor);
        if (auto it = _records.find(actor->GetFormID()); it != _records.end()) it->second.heelHeight = -1.0f;
    }

    void Engine::FurnitureChanged(RE::Actor* actor, RE::TESObjectREFR* furniture, bool entering)
    {
        if (!actor || !_ready) return;
        const auto id = actor->GetFormID();
        const auto furnitureID = furniture ? furniture->GetFormID() : 0;
        const auto generation = _generation.load();
        SKSE::GetTaskInterface()->AddTask([this, id, furnitureID, entering, generation]() {
            if (!_ready || generation != _generation.load()) return;
            {
                std::scoped_lock lock(_lock);
                auto* target = RE::TESForm::LookupByID<RE::Actor>(id);
                if (!target) return;
                auto& record = _records[id];
                record.female = target->GetActorBase() && target->GetActorBase()->GetSex() == RE::SEX::kFemale;
                if (entering) record.interaction.Enter(furnitureID);
                else record.interaction.Exit(furnitureID);
                record.heelHeight = -1.0f; // force re-evaluation on the next tick
                if (target->IsPlayerRef() || Settings::Get().debugLog)
                    logger::info("Actor {:08X}: furniture {} event, reference {:08X}", id, entering ? "enter" : "exit", furnitureID);
            }
            QueueTick();
        });
    }

    void Engine::ObjectLoaded(RE::FormID id)
    {
        if (!_ready) return;
        const auto generation = _generation.load();
        SKSE::GetTaskInterface()->AddTask([this, id, generation]() {
            if (!_ready || generation != _generation.load()) return;
            std::scoped_lock lock(_lock);
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!actor) return;
            Clear(actor, nullptr);
            _records.erase(id);
        });
    }

    void Engine::ClearAll()
    {
        std::scoped_lock lock(_lock);
        for (auto& [id, record] : _records) if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(id)) Clear(actor, std::addressof(record));
        RestoreCamera();
        _records.clear();
    }

    void Engine::SettingsChanged()
    {
        _scanIntervalMs = static_cast<std::uint32_t>(std::clamp(Settings::Get().scanInterval, 0.1f, 5.0f) * 1000.0f);
        std::scoped_lock lock(_lock);
        for (auto& [id, record] : _records) {
            if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(id)) Clear(actor, std::addressof(record));
            record.heelHeight = -1.0f;
        }
        RestoreCamera();
        QueueTick();
    }

    ActorInfo Engine::Info(RE::Actor* actor) const
    {
        if (!actor) return {};
        std::scoped_lock lock(_lock);
        if (const auto it = _records.find(actor->GetFormID()); it != _records.end())
            return { it->second.heelHeight, it->second.trueScale, it->second.adjustment, it->second.furniture, true };
        const bool female = actor->GetActorBase() && actor->GetActorBase()->GetSex() == RE::SEX::kFemale;
        return { Skee::HeelHeight(actor, female), TrueScale(actor), A_None, FurnitureClass::None, false };
    }

    RE::Actor* Engine::MenuTarget() const
    {
        if (const auto selected = RE::Console::GetSelectedRef(); selected) if (auto* actor = selected->As<RE::Actor>()) return actor;
        return RE::PlayerCharacter::GetSingleton();
    }

    std::string Engine::Status(RE::Actor* actor) const
    {
        if (!actor) return "No actor";
        const auto info = Info(actor);
        auto* shoes = actor->GetWornArmor(RE::BGSBipedObjectForm::BipedObjectSlot::kFeet);
        return std::format("{} [{:08X}] | shoes: {} | heel {:.2f} | scale {:.3f} | adjustment 0x{:X} | tracked {}",
            actor->GetName(), actor->GetFormID(), shoes ? shoes->GetName() : "none", info.heelHeight, info.trueScale,
            static_cast<unsigned>(info.adjustment), info.tracked ? "yes" : "no");
    }
}
