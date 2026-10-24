set_xmakever("3.0.0")

includes("lib/CommonLibSSE-NG")

set_project("HeelsFixNative")
set_version("1.1.0")
set_license("MIT")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")
add_rules("mode.debug", "mode.releasedbg")
set_runtimes("MD")
set_allowedplats("windows")
set_allowedarchs("x64")
set_defaultplat("windows")
set_defaultarchs("x64")

target("InteractionStateTests", function()
    set_kind("binary")
    set_default(false)
    add_files("tests/InteractionStateTests.cpp")
    add_includedirs("src")
end)

if is_plat("windows") then
    add_defines("_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS", "WIN32_LEAN_AND_MEAN", "NOMINMAX", "NOGDI", "UNICODE", "_UNICODE")
    add_cxxflags("cl::/permissive-", "cl::/Zc:preprocessor", "cl::/EHsc", "cl::/utf-8")
    add_cxxflags("cl::/wd4068", "cl::/wd4201", "cl::/wd4251", "cl::/wd4275", "cl::/wd4267", "cl::/wd4244", "cl::/wd4996", "cl::/wd4100")
end

rule("hfn.deploy", function()
    after_build(function(target)
        local dir = os.getenv("HFN_DEPLOY_DIR")
        if not dir then return end
        local plugindir = path.join(dir, "SKSE", "Plugins")
        os.mkdir(plugindir)
        os.cp(target:targetfile(), plugindir)
        if os.isfile(target:symbolfile()) then os.cp(target:symbolfile(), plugindir) end
    end)
end)

target("HeelsFixNative", function()
    add_rules("commonlibsse-ng.plugin", {
        name = "HeelsFixNative",
        author = "RustyOldSword",
        description = "Native SKSE replacement runtime for Heels Fix 1.9"
    })
    add_rules("hfn.deploy")
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src", "lib/SKSE-Menu-Framework-3-API")
    set_pcxxheader("src/PCH.h")
end)
