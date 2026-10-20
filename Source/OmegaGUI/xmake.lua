option("dx9")
    set_default(true)
    set_showmenu(true)
    set_description("Build the Direct3D 9 backend")
option_end()

option("dx10")
    set_default(true)
    set_showmenu(true)
    set_description("Build the Direct3D 10 backend")
option_end()

option("dx11")
    set_default(true)
    set_showmenu(true)
    set_description("Build the Direct3D 11 backend")
option_end()

option("dx12")
    set_default(true)
    set_showmenu(true)
    set_description("Build the Direct3D 12 backend")
option_end()

option("opengl")
    set_default(true)
    set_showmenu(true)
    set_description("Build the OpenGL 3.3 backend")
option_end()

option("vulkan")
    set_default(os.getenv("VULKAN_SDK") ~= nil)
    set_showmenu(true)
    set_description("Build the Vulkan backend (needs the Vulkan SDK headers via VULKAN_SDK)")
option_end()

local strOmegaGUIRoot = os.scriptdir()

target("OmegaGUI")
    set_kind("static")
    set_targetdir(path.join("$(projectdir)", "Build", is_mode("debug") and "Debug" or "Release", "$(arch)"))
    set_languages("c++latest")
    set_warnings("all")
    set_encodings("utf-8")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN", "UNICODE", "_UNICODE")

    add_includedirs(path.join(strOmegaGUIRoot, ".."), { public = true })
    add_includedirs(path.join(strOmegaGUIRoot, "..", "Third-Party", "Nothings", "STB"))

    add_files(path.join(strOmegaGUIRoot, "Render", "**.cpp"))
    add_files(path.join(strOmegaGUIRoot, "Input", "**.cpp"))
    add_files(path.join(strOmegaGUIRoot, "UI", "**.cpp"))

    for _, strName in ipairs({"dx9", "dx10", "dx11", "dx12", "opengl", "vulkan"}) do
        if has_config(strName) then
            add_defines("OMEGAGUI_BACKEND_" .. strName:upper() .. "=1", { public = true })
        end
    end
    if has_config("vulkan") then
        add_includedirs(path.join(os.getenv("VULKAN_SDK"), "Include"), { public = true })
    end

    if has_config("dx9") then add_files(path.join(strOmegaGUIRoot, "Backends", "DX9", "*.cpp")) end
    if has_config("dx10") then add_files(path.join(strOmegaGUIRoot, "Backends", "DX10", "*.cpp")) end
    if has_config("dx11") then add_files(path.join(strOmegaGUIRoot, "Backends", "DX11", "*.cpp")) end
    if has_config("dx12") then add_files(path.join(strOmegaGUIRoot, "Backends", "DX12", "*.cpp")) end
    if has_config("opengl") then add_files(path.join(strOmegaGUIRoot, "Backends", "OpenGL", "*.cpp")) end
    if has_config("vulkan") then add_files(path.join(strOmegaGUIRoot, "Backends", "Vulkan", "*.cpp")) end

    add_syslinks("user32", "gdi32", "imm32", "setupapi", "hid", "shell32", { public = true })
    if has_config("dx9") then add_syslinks("d3d9", { public = true }) end
    if has_config("dx10") then add_syslinks("d3d10", { public = true }) end
    if has_config("dx11") then add_syslinks("d3d11", { public = true }) end
    if has_config("dx12") then add_syslinks("d3d12", "dxgi", { public = true }) end
    if has_config("dx9") or has_config("dx10") or has_config("dx11") or has_config("dx12") then add_syslinks("d3dcompiler", "dxgi", { public = true }) end
    if has_config("opengl") then add_syslinks("opengl32", { public = true }) end
target_end()
