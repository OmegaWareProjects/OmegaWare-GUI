# OmegaWare-GUI

A console-styled, immediate-mode GUI and overlay renderer written in C++26 for the
[OmegaWare Framework](https://github.com/Omega172/OmegaWare-Framework).

It is built for in-process overlays in single-player and co-op games: every backend saves and
restores the pipeline state it touches, and the world layer can depth-test against the game's
own depth buffer so drawings hide behind walls.

## Features

- **Six render backends**: Direct3D 9, 10, 11 and 12, OpenGL 3.3 and Vulkan (dynamically loaded,
  no `vulkan-1.lib` link needed).
- **Three drawing layers**
  - *Background*: drawn under every window (2D ESP, FOV circles, watermarks).
  - *Foreground*: drawn over everything (crosshairs, warnings).
  - *World*: CPU-projected 3D lines, boxes, spheres, grids, billboarded text and health bars,
    depth-tested on the GPU. Modes per primitive: `Visible` (hidden behind walls), `XRay`
    (occluded part drawn in a second colour), `Always` and `WriteDepth` (your own opaque geometry).
- **Windows**: multiple windows, z-ordering and focus, title-bar drag, edge and corner resizing,
  snapping to screen edges and other windows, collapse/maximise/close controls (terminal dots
  or `[_] [+] [x]` brackets), fade-in, auto-resize, scrollable child regions, menu bars,
  popups, modals, context menus and tooltips.
- **Tabs**: horizontal tab bars with an animated underline, vertical sidebar tab bars, closable tabs.
- **Widgets**: buttons (primary, secondary, danger, ghost), text links, checkboxes, toggles,
  radio buttons, float/int sliders, steppers, combos, selectables, text input (password,
  decimal/hex filters, clipboard, selection), hotkey capture (keyboard, mouse and controller;
  `stHotkey` adds an always on, hold or toggle mode shown as a badge and picked from a
  right-click menu, plus a notify option, with `UpdateHotkey` to drive it), ctrl+click on a
  slider to type any value, HSV colour picker with a hex field and a per-picker rainbow mode with adjustable speed (saved with the layout), progress bars, line and histogram plots, collapsing headers, tree nodes,
  columns, sections, groups, alerts, tags, breadcrumbs and key hints.
- **Tables**: `BeginTable` / `EndTable` with stretch or fixed columns, a header row, sortable
  columns (`TableGetSortSpecs`), borders, striped rows and hovered row highlight.
- **Full controller support**: Xbox via XInput, DualShock 4 and DualSense via raw HID (USB and
  Bluetooth). Spatial d-pad/stick navigation, LB/RB tab switching, LT/RT window cycling,
  View + sticks to move and resize windows, right stick scrolling and an on-screen keyboard
  for text entry.
- **Keyboard navigation**: arrows/Tab, Enter/Space, Escape, Ctrl+Tab.
- **Extras**: toast notifications, a command console with history and tab completion,
  window layout persistence (`.ini`), UI scaling, three themes, a software cursor for games that
  hide the OS cursor and input-capture flags so the game ignores input while the menu is open.

## Layout

```
Source/
  OmegaGUI/            the library (static)
    Core/              math, colour, hashing, flag enums
    Render/            draw lists, world draw list, font atlas, backend interface
    Input/             input state, Win32 message handling, XInput + PlayStation HID
    UI/                context, windows, layout, widgets, tabs, menus, console, style
    Backends/          DX9, DX10, DX11, DX12, OpenGL, Vulkan (+ SPIR-V shaders)
  Demo/                standalone demo app, one host per backend, scripted self-test
  Third-Party/Nothings/STB/   stb_truetype and stb_rect_pack (public domain)
```

Build output goes to `Build/<Debug|Release>/<x64|x86>/`.

## Building

Requirements: Visual Studio 2022 or newer with the C++ workload, [xmake](https://xmake.io), and
the Vulkan SDK if you want the Vulkan backend (it is picked up from `VULKAN_SDK`).

```bat
Build.bat debug x64
Build.bat release x86
Run.bat dx12
SelfTest.bat
```

Each backend can be switched off: `xmake f --dx9=n --vulkan=n`.

| Script | Purpose |
|---|---|
| `Build.bat` | configure and build (`mode`, `arch`) |
| `Run.bat` | build if needed and launch the demo (`backend`, `mode`, `arch`) |
| `SelfTest.bat` | run scripted mouse, keyboard and controller tests on every backend |
| `UpdateCompileCommands.bat` | write `.vscode/compile_commands.json` |
| `Source/OmegaGUI/Backends/Vulkan/Shaders/CompileShaders.bat` | rebuild the SPIR-V headers |

Demo flags: `--backend=dx9|dx10|dx11|dx12|opengl|vulkan`, `--tab=<name>`, `--hidden`,
`--frames=N --screenshot=file.bmp`, `--selftest --output=<folder>`, `--lang=<code>` (with localization), `--warp` (D3D10/11/12 only:
use the WARP software rasterizer, as CI does).

## Using it in a hook

```cpp
#include "OmegaGUI/OmegaGUI.hpp"
#include "OmegaGUI/Backends/DX11/BackendDX11.hpp"

OmegaGUI::clBackendDX11 g_clBackend;

void OnFirstPresent(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HWND hWindow)
{
	g_clBackend.Initialize(pDevice, pContext);
	OmegaGUI::stConfig stConfig;
	stConfig.pathSettings = "OmegaWare/layout.ini";
	OmegaGUI::Initialize(&g_clBackend, hWindow, stConfig);
}

LRESULT HookedWndProc(HWND hWindow, UINT uMessage, WPARAM uWParam, LPARAM iLParam)
{
	if (OmegaGUI::ProcessWin32Message(hWindow, uMessage, uWParam, iLParam))
		return uMessage == WM_SETCURSOR ? TRUE : 0;
	return CallWindowProcW(g_pfnOriginalWndProc, hWindow, uMessage, uWParam, iLParam);
}

void OnPresent()
{
	OmegaGUI::NewFrame();

	auto& rWorld = OmegaGUI::GetWorldDrawList();
	rWorld.SetCamera(matViewProjection, OmegaGUI::GetDisplaySize());
	rWorld.AddBox(vecMin, vecMax, OmegaGUI::Colors::Blue, OmegaGUI::EDepthMode::XRay);

	if (OmegaGUI::Begin("omegaware", nullptr, OmegaGUI::EWindowFlags::MenuBar | OmegaGUI::EWindowFlags::CloseHidesMenu))
	{
		OmegaGUI::Toggle("enabled", &bEnabled);
		OmegaGUI::SliderFloat("fov", &flFov, 1.f, 180.f);
	}
	OmegaGUI::End();

	OmegaGUI::EndFrame();
	OmegaGUI::Render();
}
```

Give your main menu window `EWindowFlags::CloseHidesMenu`: its close button then hides the whole
menu instead of closing the window, so the toggle key always brings it back.

### Drawing behind walls

The world list is projected on the CPU with the view-projection matrix you pass to `SetCamera`
(row-vector convention, `v * M`), then depth-tested on the GPU. Call `OmegaGUI::RenderWorld()`
at a point where the game's depth buffer is still valid, normally after the scene and before
the game's own HUD. Point the backend at that depth buffer first:

| Backend | How to bind the game's depth |
|---|---|
| D3D9 | `SetWorldDepthSurface(IDirect3DSurface9*)` or leave the game's bound surface |
| D3D10 / D3D11 | `SetWorldTarget(renderTargetView, depthStencilView)` |
| D3D12 | `SetWorldTarget(rtvHandle, dsvHandle, dsvFormat, width, height)` |
| OpenGL | uses the currently bound framebuffer's depth |
| Vulkan | pass a render pass with a depth attachment as `hWorldRenderPass` |

Set `stWorldPass::bReversedZ` for engines that use reversed depth (most Unreal Engine titles).
If the game's depth buffer is multisampled or a different size from the back buffer, it has to
match the render target you draw into, which may mean hooking earlier in the frame.

## Localization

Translations come from [OmegaWare-Localization](https://github.com/Omega172/OmegaWare-Localization).
When that repository is cloned next to this one the build picks it up automatically
(`xmake f --localization=<path>` to point elsewhere, `--localization=` to turn it off) and the
demo gets a language selector on the settings tab.

```cpp
#include "OmegaLocalization/GUI.hpp"
#include "Locales/Locales.hpp"
using namespace OmegaLocalization::Literals;

OmegaLocalization::GUI::Attach({ g_stLocaleEnglish, g_stLocalePolish });

OmegaGUI::Toggle("aim.enabled"_l, &bEnabled);
OmegaLocalization::GUI::LanguageSelector("settings.language"_l);
```

`"key"_l` produces `text###key`, so widget IDs and saved window state do not change with the
language. Fonts are rebuilt on the next `NewFrame` with whatever glyph ranges the active language
declares; without the localization package the same hook is `OmegaGUI::SetGlyphRanges(aRanges)`.

## Controller mapping

| Input | Action |
|---|---|
| D-pad / left stick | move focus |
| A / Cross | activate |
| B / Circle | back, close popup, cancel edit |
| X / Square | context menu (on-screen keyboard: backspace) |
| Y / Triangle | on-screen keyboard: space |
| LB / RB | previous / next tab |
| LT / RT | previous / next window |
| Right stick | scroll |
| View + left / right stick | move / resize the focused window |
| View + Menu | toggle the menu |
| Left / right on sliders and steppers | adjust the value (hold RB for ×10) |

## Known limitations

- Windows only (Win32 input, XInput and HID).
- Snapping, not docking. Windows cannot be docked into each other yet.
- The D3D12 backend binds its own shader-visible descriptor heap; the game normally rebinds
  its own on the next draw.
- `WantsGamepad()` tells you when to hide controller input from the game, but blocking it
  needs hooks on the game's input APIs. OmegaWare-RenderHook's `InputBlock` does this.
- Tables have no column resizing or reordering and no frozen header when scrolled.
- PlayStation controllers are read as raw HID reports. Rumble and light bar are not driven.
- The controller code paths were verified with injected input; they have not been exercised
  against physical hardware yet.

## Licence

GNU GPL v3, matching the OmegaWare Framework. See `LICENCE`. stb_truetype and stb_rect_pack are
public domain.
