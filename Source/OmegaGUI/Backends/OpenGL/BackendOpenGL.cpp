#include "OmegaGUI/Backends/OpenGL/BackendOpenGL.hpp"
#include "OmegaGUI/Backends/Shaders.hpp"

#include <Windows.h>
#include <GL/gl.h>

#include <cstddef>

namespace OmegaGUI
{
	namespace
	{
		using GLchar = char;
		using GLsizeiptr = std::ptrdiff_t;
		using GLintptr = std::ptrdiff_t;

		constexpr GLenum GL_ARRAY_BUFFER = 0x8892;
		constexpr GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
		constexpr GLenum GL_STREAM_DRAW = 0x88E0;
		constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
		constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
		constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
		constexpr GLenum GL_LINK_STATUS = 0x8B82;
		constexpr GLenum GL_TEXTURE0 = 0x84C0;
		constexpr GLenum GL_FUNC_ADD = 0x8006;
		constexpr GLenum GL_CURRENT_PROGRAM = 0x8B8D;
		constexpr GLenum GL_ARRAY_BUFFER_BINDING = 0x8894;
		constexpr GLenum GL_VERTEX_ARRAY_BINDING = 0x85B5;
		constexpr GLenum GL_ACTIVE_TEXTURE = 0x84E0;
		constexpr GLenum GL_BLEND_DST_RGB = 0x80C8;
		constexpr GLenum GL_BLEND_SRC_RGB = 0x80C9;
		constexpr GLenum GL_BLEND_DST_ALPHA = 0x80CA;
		constexpr GLenum GL_BLEND_SRC_ALPHA = 0x80CB;
		constexpr GLenum GL_BLEND_EQUATION_RGB = 0x8009;
		constexpr GLenum GL_BLEND_EQUATION_ALPHA = 0x883D;
		constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
		constexpr GLenum GL_SAMPLER_BINDING = 0x8919;
		constexpr GLenum GL_FRAMEBUFFER_SRGB = 0x8DB9;

#define OMEGAGUI_GL_FUNCTIONS(X) \
		X(void, GenBuffers, GLsizei, GLuint*) \
		X(void, DeleteBuffers, GLsizei, const GLuint*) \
		X(void, BindBuffer, GLenum, GLuint) \
		X(void, BufferData, GLenum, GLsizeiptr, const void*, GLenum) \
		X(void, GenVertexArrays, GLsizei, GLuint*) \
		X(void, DeleteVertexArrays, GLsizei, const GLuint*) \
		X(void, BindVertexArray, GLuint) \
		X(void, EnableVertexAttribArray, GLuint) \
		X(void, VertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) \
		X(GLuint, CreateShader, GLenum) \
		X(void, ShaderSource, GLuint, GLsizei, const GLchar* const*, const GLint*) \
		X(void, CompileShader, GLuint) \
		X(void, GetShaderiv, GLuint, GLenum, GLint*) \
		X(void, DeleteShader, GLuint) \
		X(GLuint, CreateProgram, void) \
		X(void, AttachShader, GLuint, GLuint) \
		X(void, DetachShader, GLuint, GLuint) \
		X(void, LinkProgram, GLuint) \
		X(void, GetProgramiv, GLuint, GLenum, GLint*) \
		X(void, DeleteProgram, GLuint) \
		X(void, UseProgram, GLuint) \
		X(GLint, GetUniformLocation, GLuint, const GLchar*) \
		X(void, Uniform1i, GLint, GLint) \
		X(void, UniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat*) \
		X(void, ActiveTexture, GLenum) \
		X(void, BlendEquationSeparate, GLenum, GLenum) \
		X(void, BlendFuncSeparate, GLenum, GLenum, GLenum, GLenum) \
		X(void, DrawElementsBaseVertex, GLenum, GLsizei, GLenum, const void*, GLint) \
		X(void, BindSampler, GLuint, GLuint)

#define OMEGAGUI_GL_DECLARE(Return, Name, ...) using Fn##Name = Return(APIENTRY*)(__VA_ARGS__); Fn##Name gl##Name = nullptr;
		OMEGAGUI_GL_FUNCTIONS(OMEGAGUI_GL_DECLARE)
#undef OMEGAGUI_GL_DECLARE

		[[nodiscard]] void* LoadFunction(const char* szName)
		{
			void* pFunction = reinterpret_cast<void*>(wglGetProcAddress(szName));
			const auto uValue = reinterpret_cast<std::uintptr_t>(pFunction);
			if (uValue == 0 || uValue == 1 || uValue == 2 || uValue == 3 || uValue == static_cast<std::uintptr_t>(-1))
			{
				static HMODULE s_hOpenGL = GetModuleHandleW(L"opengl32.dll");
				pFunction = s_hOpenGL ? reinterpret_cast<void*>(GetProcAddress(s_hOpenGL, szName)) : nullptr;
			}
			return pFunction;
		}

		[[nodiscard]] bool LoadFunctions()
		{
			bool bComplete = true;
#define OMEGAGUI_GL_LOAD(Return, Name, ...) gl##Name = reinterpret_cast<Fn##Name>(LoadFunction("gl" #Name)); bComplete = bComplete && gl##Name != nullptr;
			OMEGAGUI_GL_FUNCTIONS(OMEGAGUI_GL_LOAD)
#undef OMEGAGUI_GL_LOAD
			return bComplete;
		}

		[[nodiscard]] GLuint CompileProgram(const char* szVertex, const char* szFragment)
		{
			auto Compile = [](GLenum eType, const char* szSource) -> GLuint {
				const GLuint uShader = glCreateShader(eType);
				glShaderSource(uShader, 1, &szSource, nullptr);
				glCompileShader(uShader);
				GLint iStatus = 0;
				glGetShaderiv(uShader, GL_COMPILE_STATUS, &iStatus);
				if (!iStatus)
				{
					glDeleteShader(uShader);
					return 0;
				}
				return uShader;
			};
			const GLuint uVertex = Compile(GL_VERTEX_SHADER, szVertex);
			const GLuint uFragment = Compile(GL_FRAGMENT_SHADER, szFragment);
			if (!uVertex || !uFragment)
			{
				if (uVertex) glDeleteShader(uVertex);
				if (uFragment) glDeleteShader(uFragment);
				return 0;
			}
			const GLuint uProgram = glCreateProgram();
			glAttachShader(uProgram, uVertex);
			glAttachShader(uProgram, uFragment);
			glLinkProgram(uProgram);
			glDetachShader(uProgram, uVertex);
			glDetachShader(uProgram, uFragment);
			glDeleteShader(uVertex);
			glDeleteShader(uFragment);
			GLint iStatus = 0;
			glGetProgramiv(uProgram, GL_LINK_STATUS, &iStatus);
			if (!iStatus)
			{
				glDeleteProgram(uProgram);
				return 0;
			}
			return uProgram;
		}

		void SetupVertexArray(GLuint uVertexArray, GLuint uVertexBuffer, GLuint uIndexBuffer, bool bWorld)
		{
			glBindVertexArray(uVertexArray);
			glBindBuffer(GL_ARRAY_BUFFER, uVertexBuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, uIndexBuffer);
			glEnableVertexAttribArray(0);
			glEnableVertexAttribArray(1);
			glEnableVertexAttribArray(2);
			if (bWorld)
			{
				glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(stWorldVertex), reinterpret_cast<const void*>(offsetof(stWorldVertex, vecClip)));
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(stWorldVertex), reinterpret_cast<const void*>(offsetof(stWorldVertex, vecUV)));
				glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(stWorldVertex), reinterpret_cast<const void*>(offsetof(stWorldVertex, uColor)));
			}
			else
			{
				glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(stVertex), reinterpret_cast<const void*>(offsetof(stVertex, vecPosition)));
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(stVertex), reinterpret_cast<const void*>(offsetof(stVertex, vecUV)));
				glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(stVertex), reinterpret_cast<const void*>(offsetof(stVertex, uColor)));
			}
			glBindVertexArray(0);
		}

		[[nodiscard]] GLenum DepthFunction(EDepthBatch eBatch, bool bReversed)
		{
			switch (eBatch)
			{
			case EDepthBatch::Behind: return bReversed ? GL_LESS : GL_GREATER;
			case EDepthBatch::Always: return GL_ALWAYS;
			default: return bReversed ? GL_GEQUAL : GL_LEQUAL;
			}
		}
	}

	struct clBackendOpenGL::stStateBackup
	{
		GLint iActiveTexture = 0;
		GLint iProgram = 0;
		GLint iTexture = 0;
		GLint iSampler = 0;
		GLint iArrayBuffer = 0;
		GLint iVertexArray = 0;
		GLint aViewport[4]{};
		GLint aScissor[4]{};
		GLint iBlendSrcRGB = 0, iBlendDstRGB = 0, iBlendSrcAlpha = 0, iBlendDstAlpha = 0;
		GLint iBlendEquationRGB = 0, iBlendEquationAlpha = 0;
		GLint iDepthFunction = 0;
		GLboolean bDepthMask = GL_TRUE;
		GLboolean bBlend = GL_FALSE, bCull = GL_FALSE, bDepth = GL_FALSE, bStencil = GL_FALSE, bScissor = GL_FALSE, bSRGB = GL_FALSE;
	};

	clBackendOpenGL::~clBackendOpenGL()
	{
		Shutdown();
	}

	bool clBackendOpenGL::Initialize()
	{
		m_bLoaded = LoadFunctions();
		return m_bLoaded && CreateDeviceObjects();
	}

	void clBackendOpenGL::Shutdown()
	{
		if (m_bLoaded)
			InvalidateDeviceObjects();
		m_bLoaded = false;
	}

	bool clBackendOpenGL::CreateDeviceObjects()
	{
		if (!m_bLoaded)
			return false;
		InvalidateDeviceObjects();
		m_uProgram = CompileProgram(Shaders::szVertexGLSL, Shaders::szFragmentGLSL);
		m_uWorldProgram = CompileProgram(Shaders::szWorldVertexGLSL, Shaders::szFragmentGLSL);
		if (!m_uProgram || !m_uWorldProgram)
			return false;
		m_iProjectionLocation = glGetUniformLocation(m_uProgram, "ProjectionMatrix");
		m_iTextureLocation = glGetUniformLocation(m_uProgram, "Texture");
		m_iWorldTextureLocation = glGetUniformLocation(m_uWorldProgram, "Texture");

		GLint iPreviousArray = 0, iPreviousBuffer = 0;
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &iPreviousArray);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &iPreviousBuffer);
		glGenBuffers(1, &m_uVertexBuffer);
		glGenBuffers(1, &m_uIndexBuffer);
		glGenBuffers(1, &m_uWorldVertexBuffer);
		glGenBuffers(1, &m_uWorldIndexBuffer);
		glGenVertexArrays(1, &m_uVertexArray);
		glGenVertexArrays(1, &m_uWorldVertexArray);
		SetupVertexArray(m_uVertexArray, m_uVertexBuffer, m_uIndexBuffer, false);
		SetupVertexArray(m_uWorldVertexArray, m_uWorldVertexBuffer, m_uWorldIndexBuffer, true);
		glBindVertexArray(static_cast<GLuint>(iPreviousArray));
		glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(iPreviousBuffer));
		return true;
	}

	void clBackendOpenGL::InvalidateDeviceObjects()
	{
		if (!m_bLoaded)
			return;
		if (m_uVertexArray) glDeleteVertexArrays(1, &m_uVertexArray);
		if (m_uWorldVertexArray) glDeleteVertexArrays(1, &m_uWorldVertexArray);
		const GLuint aBuffers[] = { m_uVertexBuffer, m_uIndexBuffer, m_uWorldVertexBuffer, m_uWorldIndexBuffer };
		for (const GLuint uBuffer : aBuffers)
			if (uBuffer)
				glDeleteBuffers(1, &uBuffer);
		if (m_uProgram) glDeleteProgram(m_uProgram);
		if (m_uWorldProgram) glDeleteProgram(m_uWorldProgram);
		m_uVertexArray = m_uWorldVertexArray = m_uVertexBuffer = m_uIndexBuffer = m_uWorldVertexBuffer = m_uWorldIndexBuffer = m_uProgram = m_uWorldProgram = 0;
	}

	TextureHandle clBackendOpenGL::CreateTexture(const std::uint8_t* pPixelsRGBA, int iWidth, int iHeight)
	{
		GLint iPrevious = 0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &iPrevious);
		GLuint uTexture = 0;
		glGenTextures(1, &uTexture);
		glBindTexture(GL_TEXTURE_2D, uTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, iWidth, iHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pPixelsRGBA);
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(iPrevious));
		return uTexture;
	}

	void clBackendOpenGL::DestroyTexture(TextureHandle hTexture)
	{
		const auto uTexture = static_cast<GLuint>(hTexture);
		if (uTexture)
			glDeleteTextures(1, &uTexture);
	}

	void clBackendOpenGL::BackupState(stStateBackup& rBackup) const
	{
		glGetIntegerv(GL_ACTIVE_TEXTURE, &rBackup.iActiveTexture);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_CURRENT_PROGRAM, &rBackup.iProgram);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &rBackup.iTexture);
		glGetIntegerv(GL_SAMPLER_BINDING, &rBackup.iSampler);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &rBackup.iArrayBuffer);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &rBackup.iVertexArray);
		glGetIntegerv(GL_VIEWPORT, rBackup.aViewport);
		glGetIntegerv(GL_SCISSOR_BOX, rBackup.aScissor);
		glGetIntegerv(GL_BLEND_SRC_RGB, &rBackup.iBlendSrcRGB);
		glGetIntegerv(GL_BLEND_DST_RGB, &rBackup.iBlendDstRGB);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &rBackup.iBlendSrcAlpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &rBackup.iBlendDstAlpha);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &rBackup.iBlendEquationRGB);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &rBackup.iBlendEquationAlpha);
		glGetIntegerv(GL_DEPTH_FUNC, &rBackup.iDepthFunction);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &rBackup.bDepthMask);
		rBackup.bBlend = glIsEnabled(GL_BLEND);
		rBackup.bCull = glIsEnabled(GL_CULL_FACE);
		rBackup.bDepth = glIsEnabled(GL_DEPTH_TEST);
		rBackup.bStencil = glIsEnabled(GL_STENCIL_TEST);
		rBackup.bScissor = glIsEnabled(GL_SCISSOR_TEST);
		rBackup.bSRGB = glIsEnabled(GL_FRAMEBUFFER_SRGB);
	}

	void clBackendOpenGL::RestoreState(const stStateBackup& rBackup) const
	{
		auto Toggle = [](GLenum eCapability, GLboolean bEnabled) {
			if (bEnabled)
				glEnable(eCapability);
			else
				glDisable(eCapability);
		};
		glUseProgram(static_cast<GLuint>(rBackup.iProgram));
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(rBackup.iTexture));
		glBindSampler(0, static_cast<GLuint>(rBackup.iSampler));
		glActiveTexture(static_cast<GLenum>(rBackup.iActiveTexture));
		glBindVertexArray(static_cast<GLuint>(rBackup.iVertexArray));
		glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(rBackup.iArrayBuffer));
		glBlendEquationSeparate(static_cast<GLenum>(rBackup.iBlendEquationRGB), static_cast<GLenum>(rBackup.iBlendEquationAlpha));
		glBlendFuncSeparate(static_cast<GLenum>(rBackup.iBlendSrcRGB), static_cast<GLenum>(rBackup.iBlendDstRGB), static_cast<GLenum>(rBackup.iBlendSrcAlpha), static_cast<GLenum>(rBackup.iBlendDstAlpha));
		glDepthFunc(static_cast<GLenum>(rBackup.iDepthFunction));
		glDepthMask(rBackup.bDepthMask);
		Toggle(GL_BLEND, rBackup.bBlend);
		Toggle(GL_CULL_FACE, rBackup.bCull);
		Toggle(GL_DEPTH_TEST, rBackup.bDepth);
		Toggle(GL_STENCIL_TEST, rBackup.bStencil);
		Toggle(GL_SCISSOR_TEST, rBackup.bScissor);
		Toggle(GL_FRAMEBUFFER_SRGB, rBackup.bSRGB);
		glViewport(rBackup.aViewport[0], rBackup.aViewport[1], rBackup.aViewport[2], rBackup.aViewport[3]);
		glScissor(rBackup.aScissor[0], rBackup.aScissor[1], rBackup.aScissor[2], rBackup.aScissor[3]);
	}

	void clBackendOpenGL::RenderDrawData(const stDrawData& stData)
	{
		if (!m_uProgram || stData.uTotalIndices == 0 || stData.vecDisplaySize.x <= 0.f || stData.vecDisplaySize.y <= 0.f)
			return;
		stStateBackup stBackup;
		BackupState(stBackup);

		const auto iWidth = static_cast<GLsizei>(stData.vecDisplaySize.x * stData.vecFramebufferScale.x);
		const auto iHeight = static_cast<GLsizei>(stData.vecDisplaySize.y * stData.vecFramebufferScale.y);
		glEnable(GL_BLEND);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_FRAMEBUFFER_SRGB);
		glEnable(GL_SCISSOR_TEST);
		glViewport(0, 0, iWidth, iHeight);
		glUseProgram(m_uProgram);
		glUniform1i(m_iTextureLocation, 0);
		const auto aProjection = stData.OrthoProjection();
		glUniformMatrix4fv(m_iProjectionLocation, 1, GL_FALSE, aProjection.data());
		glBindSampler(0, 0);
		glBindVertexArray(m_uVertexArray);

		for (const clDrawList* pList : stData.aLists)
		{
			glBindBuffer(GL_ARRAY_BUFFER, m_uVertexBuffer);
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(pList->GetVertices().size() * sizeof(stVertex)), pList->GetVertices().data(), GL_STREAM_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(pList->GetIndices().size() * sizeof(std::uint32_t)), pList->GetIndices().data(), GL_STREAM_DRAW);
			for (const stDrawCommand& rCommand : pList->GetCommands())
			{
				if (rCommand.uIndexCount == 0)
					continue;
				const rect rcClip = rCommand.rcClip.Clipped({ stData.vecDisplayPosition, stData.vecDisplayPosition + stData.vecDisplaySize });
				if (rcClip.IsEmpty())
					continue;
				const vec2 vecMin = (rcClip.min - stData.vecDisplayPosition) * stData.vecFramebufferScale;
				const vec2 vecMax = (rcClip.max - stData.vecDisplayPosition) * stData.vecFramebufferScale;
				glScissor(static_cast<GLint>(vecMin.x), static_cast<GLint>(static_cast<float>(iHeight) - vecMax.y), static_cast<GLsizei>(vecMax.x - vecMin.x), static_cast<GLsizei>(vecMax.y - vecMin.y));
				glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(rCommand.hTexture));
				glDrawElementsBaseVertex(GL_TRIANGLES, static_cast<GLsizei>(rCommand.uIndexCount), GL_UNSIGNED_INT, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(rCommand.uIndexOffset) * sizeof(std::uint32_t)), 0);
			}
		}

		RestoreState(stBackup);
	}

	void clBackendOpenGL::RenderWorld(const clWorldDrawList& rList, const stWorldPass& stPass)
	{
		if (!m_uWorldProgram || rList.IsEmpty())
			return;
		stStateBackup stBackup;
		BackupState(stBackup);

		glEnable(GL_BLEND);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_SCISSOR_TEST);
		glUseProgram(m_uWorldProgram);
		glUniform1i(m_iWorldTextureLocation, 0);
		glBindSampler(0, 0);
		glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(rList.GetTexture()));
		glBindVertexArray(m_uWorldVertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, m_uWorldVertexBuffer);

		for (std::size_t i = 0; i < static_cast<std::size_t>(EDepthBatch::Count); ++i)
		{
			const auto eBatch = static_cast<EDepthBatch>(i);
			const stWorldBatch& rBatch = rList.GetBatch(eBatch);
			if (rBatch.aIndices.empty())
				continue;
			if (eBatch == EDepthBatch::Always)
				glDisable(GL_DEPTH_TEST);
			else
				glEnable(GL_DEPTH_TEST);
			glDepthFunc(DepthFunction(eBatch, stPass.bReversedZ));
			glDepthMask(eBatch == EDepthBatch::WriteDepth ? GL_TRUE : GL_FALSE);
			glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(rBatch.aVertices.size() * sizeof(stWorldVertex)), rBatch.aVertices.data(), GL_STREAM_DRAW);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(rBatch.aIndices.size() * sizeof(std::uint32_t)), rBatch.aIndices.data(), GL_STREAM_DRAW);
			glDrawElementsBaseVertex(GL_TRIANGLES, static_cast<GLsizei>(rBatch.aIndices.size()), GL_UNSIGNED_INT, nullptr, 0);
		}

		RestoreState(stBackup);
	}
}
