#!/usr/bin/env python3
"""Deterministic static preflight for the checked-in HeelsFixNG C++ source."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
ERRORS: list[str] = []

def require(condition: bool, message: str) -> None:
    if not condition:
        ERRORS.append(message)

def read(path: Path) -> str:
    if not path.is_file():
        ERRORS.append(f"missing required file: {path.relative_to(ROOT)}")
        return ""
    return path.read_text(encoding="utf-8-sig")

required_sources = [
    "Engine.cpp", "Engine.h", "InteractionState.h", "Menu.cpp", "Menu.h",
    "PCH.h", "Settings.cpp", "Settings.h", "Skee.cpp", "Skee.h", "SkeeAPI.h", "main.cpp",
]
source_text: dict[str, str] = {name: read(SRC / name) for name in required_sources}
all_cpp_text = "\n".join(source_text.values())
engine = source_text["Engine.cpp"]
engine_h = source_text["Engine.h"]
skee = source_text["Skee.cpp"]
api = source_text["SkeeAPI.h"]
settings_h = source_text["Settings.h"]
settings_cpp = source_text["Settings.cpp"]
menu = source_text["Menu.cpp"]

# Catch the exact syntax defect from the prior package and similar escaped member access.
require(not re.search(r"\b[A-Za-z_]\w*\\\.[A-Za-z_]", all_cpp_text), "invalid backslash before C++ member access (e.g. s\\.npcEnabled)")

# No legacy ESP/papyrus/scene integration in the native source.
for pat in (r"HeelsFix\.esp", r"_heelsSpell", r"_legacyAbility", r"darSpell", r"LegacyFormsAvailable", r"SexLab", r"sexlab", r"P\+"):
    require(re.search(pat, all_cpp_text, re.I) is None, f"forbidden legacy dependency/reference remains: {pat}")
require('constexpr auto kKey = "HeelsFixNG.esp";' in engine,
        "SKEE transform ownership key changed without updating the reviewed cleanup convention")
require(not re.search(r"LookupForm|TESDataHandler|AddSpell\(|RemoveSpell\(", engine),
        "native source still performs ESP/form lookup or spell operations")
require("void Engine::Clear(RE::Actor* actor, Record* record)" in engine,
        "two-argument Clear implementation is missing")
require("void Clear(RE::Actor* actor, Record* record);" in engine_h,
        "two-argument Clear declaration is missing")
require(not re.search(r"\bClear\s*\([^;{}]*,\s*(?:true|false)\s*\)", engine),
        "obsolete third boolean argument remains in Clear() call")

# RaceMenu 0.4.11 legacy NiTransform v2 ABI declarations and adapter.
require("static_assert(sizeof(FixedString) == 0x28);" in api, "legacy SKEE FixedString size assertion missing")
require("static_assert(sizeof(OverrideVariant) == 0x20);" in api, "legacy SKEE OverrideVariant size assertion missing")
require("using FixedString = RE::BSFixedString;" not in api, "legacy SKEE FixedString was replaced with BSFixedString")
require("reinterpret_cast<SKEE::Legacy::INiTransformInterface*>(base)" in skee, "legacy SKEE v2 interface path missing")
require("if (version == 2)" in skee, "RaceMenu NiTransform v2 connection branch missing")
require("v.SetFloat(transformKey, value);\n            v.index = index;" in skee, "legacy OverrideVariant float/index assignment missing")
require("constexpr std::uint16_t kPositionKey = 31;" in skee and "constexpr std::uint16_t kRotationKey = 32;" in skee,
        "legacy SKEE position/rotation keys missing")
require("retrying without disabling the plugin" in skee, "bounded SKEE initialization retry message missing")

# NPC-only settings profile.
require("bool playerEnabled{ false };" in settings_h and "bool playerAdvanced{ false };" in settings_h,
        "player correction defaults must remain disabled")
require("bool correctFirstPerson{ false };" in settings_h and "bool correctThirdPerson{ false };" in settings_h,
        "camera correction defaults must remain disabled")
for pat in (r"BOOL_KEY\(playerEnabled\)", r"BOOL_KEY\(playerAdvanced\)", r"BOOL_KEY\(correctFirstPerson\)",
            r"BOOL_KEY\(correctThirdPerson\)", r"s\.(?:playerEnabled|playerAdvanced|correctFirstPerson|correctThirdPerson|correctScaledNoHeel|darSpell|cameraIdleZ|cameraCombatZ|heelRefreshMinutes)"):
    require(re.search(pat, settings_cpp) is None, f"removed player/camera setting is still persisted: {pat}")
require('SKSEMenuFramework::SetSection("Heels Fix Native")' in menu, "SKSE Menu Framework section missing")
require('SKSEMenuFramework::AddSectionItem("NPC furniture correction", Actors);' in menu,
        "NPC furniture page registration missing")
require('SKSEMenuFramework::AddSectionItem("Info / maintenance", Info);' in menu,
        "settings/maintenance page registration missing")
for label in ("Enable NPC correction", "Advanced NPC correction", "Thigh factor", "Thigh factor (simple sit)",
              "Lean forward factor", "Lean back factor", "Scaling deviation", "Reload INI", "Reset defaults"):
    require(label in menu, f"required in-game menu control missing: {label}")
require(not re.search(r'AddSectionItem\("(?:Player|Camera|SexLab)', menu, re.I), "forbidden player/camera/SexLab menu page found")

# Runtime and build configuration.
ini = read(ROOT / "Runtime" / "HeelsFixNative.ini")
for key in ("npcEnabled", "npcAdvanced", "correctScaledActors", "scanInterval", "thighFactor", "thighFactorSS",
            "leanForwardFactor", "leanBackFactor", "scalingDeviation", "mjfFurnitureDisable", "mjfKneeFactor", "debugLog"):
    require(re.search(rf"(?im)^\s*{re.escape(key)}\s*=", ini) is not None, f"runtime INI key missing: {key}")
require(re.search(r"(?im)^\s*(?:playerEnabled|playerAdvanced|correctFirstPerson|correctThirdPerson|cameraIdleZ|cameraCombatZ|darSpell|heelRefreshMinutes|sexlab\w*)\s*=", ini) is None,
        "runtime INI exposes a forbidden player/camera/DAR/SexLab setting")
xmake = read(ROOT / "xmake.lua")
require('set_xmakever("3.0.0")' in xmake and 'set_version("1.1.0")' in xmake,
        "xmake/plugin project version is incorrect")
require('add_files("src/**.cpp")' in xmake and 'add_headerfiles("src/**.h")' in xmake,
        "xmake target must compile the checked-in C++ files")
require((ROOT / "tests" / "InteractionStateTests.cpp").is_file(), "C++ regression test file missing")
workflow_path = ROOT / ".github" / "workflows" / "build.yml"
builder_path = ROOT / "Build-HeelsFixNG-NoESP.ps1"
workflow_text = read(workflow_path)
builder_text = read(builder_path)
expected_options = "--skyrim_se=y --skyrim_ae=y --skyrim_vr=y --rex_json=n --rex_toml=n"
for label, text in (("GitHub Actions", workflow_text), ("local builder", builder_text)):
    config_lines = [line for line in text.splitlines() if re.search(r"\bxmake\s+f\s", line)]
    require(len(config_lines) == 1, f"{label} must contain exactly one xmake configure command")
    require(len(config_lines) == 1 and expected_options in config_lines[0],
            f"{label} xmake options differ from the expected CommonLib prebuilt ABI flags")
require("Stage 1" in workflow_text and "Stage 6" in workflow_text and "InteractionStateTests" in workflow_text,
        "GitHub workflow is missing staged static verification or test gates")
require("rustyoldsword/HeelsFixNG.git" not in builder_text,
        "local build entry point must use checked-in sources, not patch an external upstream clone")
require("add_files(\"src/**.cpp\")" not in builder_text,
        "build orchestration must not substitute for checked-in C++ sources")
require((ROOT / "Build-HeelsFixNG-NoESP.ps1").is_file(), "local build entry point missing")

for path in ROOT.rglob("*"):
    if not path.is_file() or any(part in {".git", "lib", ".xmake", "build", "dist"} for part in path.parts):
        continue
    if path.suffix.lower() in {".cpp", ".h", ".hpp", ".inl", ".py", ".yml", ".yaml", ".ps1", ".ini", ".txt", ".md", ".lua", ".gitignore"}:
        try:
            text = path.read_text(encoding="utf-8-sig")
        except UnicodeDecodeError:
            continue
        for line_no, line in enumerate(text.splitlines(), 1):
            require(line == line.rstrip(" \t"), f"trailing whitespace: {path.relative_to(ROOT)}:{line_no}")

for forbidden in ("papyrus", "dist_template"):
    require(not (ROOT / forbidden).exists(), f"forbidden payload directory exists: {forbidden}")
for path in ROOT.rglob("*"):
    if path.is_file() and path.suffix.lower() in {".esp", ".esm", ".esl", ".pex"}:
        ERRORS.append(f"forbidden plugin/script payload exists: {path.relative_to(ROOT)}")

if ERRORS:
    print("SOURCE PREFLIGHT FAILED:")
    for error in ERRORS:
        print(f" - {error}")
    sys.exit(1)

cpp_count = sum(1 for p in SRC.rglob("*") if p.is_file() and p.suffix.lower() in {".cpp", ".h", ".hpp", ".inl"})
print(f"SOURCE PREFLIGHT PASSED: {cpp_count} C++ source/header files; NPC-only/SKEE v2/menu/runtime settings checked.")
