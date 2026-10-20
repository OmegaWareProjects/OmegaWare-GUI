#if OMEGAGUI_BACKEND_OPENGL

#include "Demo/Host.hpp"
#include "OmegaGUI/Backends/OpenGL/BackendOpenGL.hpp"

#include <GL/gl.h>

namespace
{
	class clHostOpenGL final : public clHost
	{
	public:
		~clHostOpenGL() override
		{
			m_clBackend.Shutdown();
			wglMakeCurrent(nullptr, nullptr);
			if (m_hContext)
				wglDeleteContext(m_hContext);
			if (m_hDC)
				ReleaseDC(m_hWindow, m_hDC);
		}

		bool Create(HWND hWindow, int iWidth, int iHeight) override
		{
			m_hWindow = hWindow;
			m_hDC = GetDC(hWindow);
			PIXELFORMATDESCRIPTOR stFormat{};
			stFormat.nSize = sizeof(stFormat);
			stFormat.nVersion = 1;
			stFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
			stFormat.iPixelType = PFD_TYPE_RGBA;
			stFormat.cColorBits = 32;
			stFormat.cDepthBits = 24;
			stFormat.cStencilBits = 8;
			const int iFormat = ChoosePixelFormat(m_hDC, &stFormat);
			if (!iFormat || !SetPixelFormat(m_hDC, iFormat, &stFormat))
				return false;
			m_hContext = wglCreateContext(m_hDC);
			if (!m_hContext || !wglMakeCurrent(m_hDC, m_hContext))
				return false;
			using FnSwapInterval = BOOL(WINAPI*)(int);
			if (const auto pfnSwapInterval = reinterpret_cast<FnSwapInterval>(wglGetProcAddress("wglSwapIntervalEXT")))
				pfnSwapInterval(1);
			Resize(iWidth, iHeight);
			return m_clBackend.Initialize();
		}

		void Resize(int iWidth, int iHeight) override
		{
			m_iWidth = std::max(iWidth, 1);
			m_iHeight = std::max(iHeight, 1);
		}

		void BeginFrame(const float aClearColor[4]) override
		{
			glViewport(0, 0, m_iWidth, m_iHeight);
			glClearColor(aClearColor[0], aClearColor[1], aClearColor[2], aClearColor[3]);
			glDepthMask(GL_TRUE);
			glClearDepth(1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		}

		void EndFrame() override
		{
			SwapBuffers(m_hDC);
		}

		OmegaGUI::clRenderBackend* Backend() override { return &m_clBackend; }

	private:
		HWND m_hWindow = nullptr;
		HDC m_hDC = nullptr;
		HGLRC m_hContext = nullptr;
		OmegaGUI::clBackendOpenGL m_clBackend;
		int m_iWidth = 1;
		int m_iHeight = 1;
	};
}

std::unique_ptr<clHost> CreateHostOpenGL()
{
	return std::make_unique<clHostOpenGL>();
}

#endif
