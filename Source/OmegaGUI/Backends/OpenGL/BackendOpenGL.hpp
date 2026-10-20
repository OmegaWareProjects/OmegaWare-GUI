#pragma once

#include "OmegaGUI/Render/Backend.hpp"

namespace OmegaGUI
{
	class clBackendOpenGL final : public clRenderBackend
	{
	public:
		~clBackendOpenGL() override;

		bool Initialize();
		void Shutdown();

		[[nodiscard]] std::string_view GetName() const override { return "OpenGL 3.3"; }
		[[nodiscard]] TextureHandle CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight) override;
		void DestroyTexture(TextureHandle hTexture) override;
		void RenderDrawData(const stDrawData& stData) override;
		void RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass) override;
		void InvalidateDeviceObjects() override;
		bool CreateDeviceObjects() override;

	private:
		struct stStateBackup;

		void BackupState(stStateBackup& rBackup) const;
		void RestoreState(const stStateBackup& rBackup) const;

		unsigned int m_uProgram = 0;
		unsigned int m_uWorldProgram = 0;
		int m_iProjectionLocation = -1;
		int m_iTextureLocation = -1;
		int m_iWorldTextureLocation = -1;
		unsigned int m_uVertexArray = 0;
		unsigned int m_uWorldVertexArray = 0;
		unsigned int m_uVertexBuffer = 0;
		unsigned int m_uIndexBuffer = 0;
		unsigned int m_uWorldVertexBuffer = 0;
		unsigned int m_uWorldIndexBuffer = 0;
		bool m_bLoaded = false;
	};
}
