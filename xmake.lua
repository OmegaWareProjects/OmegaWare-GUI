set_project("OmegaWare-GUI")
set_version("0.1.0")

add_rules("mode.debug", "mode.release")

local function LlvmTool(strName)
    local strTool = path.join(os.getenv("ProgramFiles") or "", "LLVM", "bin", strName .. ".exe")
    return os.isfile(strTool) and strTool or strName
end
set_toolchains("clang-cl")
set_toolset("ld", LlvmTool("lld-link"))
set_toolset("sh", LlvmTool("lld-link"))
set_toolset("ar", LlvmTool("llvm-ar"))

if is_mode("debug") then
    set_symbols("debug")
end

local strLocalizationDefault = path.join(os.scriptdir(), "..", "OmegaWare-Localization")
option("localization")
    set_default(os.isdir(strLocalizationDefault) and strLocalizationDefault or "")
    set_showmenu(true)
    set_description("Path to OmegaWare-Localization; the demo gets a language selector when set (empty to disable)")
option_end()

set_languages("c++latest")
set_warnings("all")
set_encodings("utf-8")
set_runtimes(is_mode("debug") and "MTd" or "MT")
add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN", "UNICODE", "_UNICODE")

local strOutputDir = path.join("Build", is_mode("debug") and "Debug" or "Release", "$(arch)")

includes("Source/OmegaGUI/xmake.lua")

local strLocalizationDir = get_config("localization")
local bLocalization = strLocalizationDir ~= nil and strLocalizationDir ~= "" and os.isdir(strLocalizationDir)
if bLocalization then
    includes(path.join(strLocalizationDir, "Source", "OmegaLocalization", "xmake.lua"))
end

target("Demo")
    set_kind("binary")
    set_targetdir(strOutputDir)
    set_filename("OmegaGUI-Demo.exe")
    add_deps("OmegaGUI")
    add_files("Source/Demo/**.cpp")
    if bLocalization then
        add_deps("OmegaLocalization")
        add_defines("OMEGAGUI_DEMO_LOCALIZATION=1")
        add_includedirs(strLocalizationDir)
    end
    add_ldflags("/SUBSYSTEM:WINDOWS", "/ENTRY:wWinMainCRTStartup", { force = true })
target_end()
