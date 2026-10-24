#include "Menu.h"
#include "Engine.h"
#include "Settings.h"
#include "Skee.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace HFN::Menu
{
    namespace
    {
        void Commit() { Settings::Save(); Engine::Get().SettingsChanged(); }
        void Help(const char* text) { if (text && ImGuiMCP::IsItemHovered()) ImGuiMCP::SetTooltip("%s", text); }
        void Toggle(const char* label, bool& value, const char* help = nullptr) { if (ImGuiMCP::Checkbox(label, &value)) Commit(); Help(help); }
        void Slider(const char* label, float& value, float min, float max, const char* format, const char* help = nullptr) { if (ImGuiMCP::SliderFloat(label, &value, min, max, format)) Commit(); Help(help); }
        void Heading(const char* label) { ImGuiMCP::SeparatorText(label); }

        void __stdcall Actors()
        {
            auto& s = Settings::Get();
            auto& engine = Engine::Get();
            Heading("NPC furniture correction");
            Toggle("Enable NPC correction", s.npcEnabled);
            Toggle("Advanced NPC correction", s.npcAdvanced);
            Toggle("Correct scaled heel wearers", s.correctScaledActors);
            Slider("Thigh factor", s.thighFactor, 10.0f, 200.0f, "%.1f");
            Slider("Thigh factor (simple sit)", s.thighFactorSS, 10.0f, 200.0f, "%.1f");
            Slider("Lean forward factor", s.leanForwardFactor, 10.0f, 200.0f, "%.1f");
            Slider("Lean back factor", s.leanBackFactor, 10.0f, 200.0f, "%.1f");
            Slider("Scaling deviation", s.scalingDeviation, 0.005f, 0.1f, "%.3f");
            if (engine.MuJointFixFound()) {
                Heading("Mu Joint Fix");
                Toggle("Temporarily disable Mu Joint Fix on furniture", s.mjfFurnitureDisable);
                Slider("Mu Joint Fix knee factor", s.mjfKneeFactor, 0.0f, 10.0f, "%.1f");
            }
            Slider("Native scan interval", s.scanInterval, 0.1f, 2.0f, "%.2f s");
        }



        void __stdcall Info()
        {
            auto& engine = Engine::Get();
            Heading("Runtime");
            ImGuiMCP::Text("RaceMenu NiTransform: %s", Skee::Ready() ? "connected" : "missing");
            ImGuiMCP::Text("Mu Joint Fix: %s", engine.MuJointFixFound() ? "found" : "not found");
            Heading("Settings");
            if (ImGuiMCP::Button("Reload INI")) { Settings::Load(); engine.SettingsChanged(); }
            ImGuiMCP::SameLine();
            if (ImGuiMCP::Button("Reset defaults")) { Settings::Reset(); engine.SettingsChanged(); }
            bool debug = Settings::Get().debugLog;
            if (ImGuiMCP::Checkbox("Verbose log", &debug)) { Settings::Get().debugLog = debug; Commit(); }
            ImGuiMCP::TextWrapped("Data\\SKSE\\Plugins\\HeelsFixNative.ini");
        }
    }

    bool Register()
    {
        if (!GetMenuFrameworkModule()) { logger::warn("SKSE Menu Framework is not loaded"); return false; }
        SKSEMenuFramework::SetSection("Heels Fix Native");
        SKSEMenuFramework::AddSectionItem("NPC furniture correction", Actors);
        SKSEMenuFramework::AddSectionItem("Info / maintenance", Info);
        logger::info("Registered SKSE Menu Framework UI v{}", SKSEMenuFramework::GetMenuFrameworkVersion());
        return true;
    }
}
