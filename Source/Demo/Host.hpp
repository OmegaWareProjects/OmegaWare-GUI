#pragma once

#include "OmegaGUI/OmegaGUI.hpp"

#include <Windows.h>

#include <memory>
#include <string_view>

class clHost
{
public:
	virtual ~clHost() = default;

	virtual bool Create(HWND hWindow, int iWidth, int iHeight) = 0;
	virtual void Resize(int iWidth, int iHeight) = 0;
	virtual void BeginFrame(const float aClearColor[4]) = 0;
	virtual void EndFrame() = 0;
	[[nodiscard]] virtual OmegaGUI::clRenderBackend* Backend() = 0;
	[[nodiscard]] virtual bool ClipSpaceYDown() const { return false; }
	[[nodiscard]] virtual bool DepthMinusOneToOne() const { return false; }
};

inline bool g_bForceWarp = false;

std::unique_ptr<clHost> CreateHost(std::string_view strBackend);
