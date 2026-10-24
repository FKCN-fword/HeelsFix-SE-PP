#include "Engine.h"
#include "Menu.h"
#include "Settings.h"
#include "Skee.h"

namespace
{
    class ObjectSink final : public RE::BSTEventSink<RE::TESObjectLoadedEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,
            RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override
        {
            if (event) HFN::Engine::Get().ObjectLoaded(event->formID);
            return RE::BSEventNotifyControl::kContinue;
        }
    };
    ObjectSink objectSink;

    class FurnitureSink final : public RE::BSTEventSink<RE::TESFurnitureEvent>
    {
    public:
        static FurnitureSink* GetSingleton()
        {
            static FurnitureSink singleton;
            return std::addressof(singleton);
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESFurnitureEvent* event,
            RE::BSTEventSource<RE::TESFurnitureEvent>*) override
        {
            if (event && event->actor) {
                if (auto* actor = event->actor->As<RE::Actor>()) {
                    HFN::Engine::Get().FurnitureChanged(
                        actor,
                        event->targetFurniture.get(),
                        event->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter);
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void Logging()
    {
        auto path = logger::log_directory();
        if (!path) SKSE::stl::report_and_fail("Unable to find SKSE log directory");
        *path /= "HeelsFixNative.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("Global", std::move(sink));
        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    }

    void Message(SKSE::MessagingInterface::Message* message)
    {
        switch (message->type) {
        case SKSE::MessagingInterface::kPostPostLoad:
            HFN::Skee::Connect();
            HFN::Menu::Register();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            HFN::Skee::Connect();
            HFN::Settings::Load();
            HFN::Engine::Get().Init();
            RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(FurnitureSink::GetSingleton());
            RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(&objectSink);
            logger::info("Registered native furniture event handler");
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            HFN::Engine::Get().SetReady(false);
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            HFN::Skee::Connect();
            HFN::Engine::Get().SetReady(true);
            HFN::Engine::Get().OnGameLoaded();
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    Logging();
    SKSE::Init(skse);
    SKSE::GetSerializationInterface()->SetRevertCallback([](SKSE::SerializationInterface*) {
        HFN::Engine::Get().SetReady(false);
    });
    const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
    logger::info("{} {} loading", plugin->GetName(), plugin->GetVersion().string());
    if (!SKSE::GetMessagingInterface()->RegisterListener(Message)) SKSE::stl::report_and_fail("Could not register SKSE message listener");
    return true;
}
