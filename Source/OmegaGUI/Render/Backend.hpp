#pragma once

#include "OmegaGUI/Render/DrawList.hpp"
#include "OmegaGUI/Render/WorldDrawList.hpp"

#include <string_view>

namespace OmegaGUI
{
	template <typename T>
	[[nodiscard]] T* HandleToPointer(TextureHandle hTexture)
	{
		return reinterpret_cast<T*>(static_cast<std::uintptr_t>(hTexture));
	}

	[[nodiscard]] inline TextureHandle PointerToHandle(const void* pObject)
	{
		return static_cast<TextureHandle>(reinterpret_cast<std::uintptr_t>(pObject));
	}

	class clRenderBackend
	{
	public:
		virtual ~clRenderBackend() = default;

		[[nodiscard]] virtual std::string_view GetName() const = 0;

		[[nodiscard]] virtual TextureHandle CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight) = 0;
		virtual void DestroyTexture(TextureHandle hTexture) = 0;

		virtual void RenderDrawData(const stDrawData& stData) = 0;
		virtual void RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass) = 0;

		virtual void InvalidateDeviceObjects() {}
		virtual bool CreateDeviceObjects() { return true; }
	};
}
