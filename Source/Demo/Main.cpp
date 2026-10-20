#include "Demo/DemoUI.hpp"
#include "Demo/Host.hpp"
#include "Demo/SelfTest.hpp"

#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

std::unique_ptr<clHost> CreateHostDX9();
std::unique_ptr<clHost> CreateHostDX10();
std::unique_ptr<clHost> CreateHostDX11();
std::unique_ptr<clHost> CreateHostDX12();
std::unique_ptr<clHost> CreateHostOpenGL();
std::unique_ptr<clHost> CreateHostVulkan();

std::unique_ptr<clHost> CreateHost(std::string_view strBackend)
{
#if OMEGAGUI_BACKEND_DX9
	if (strBackend == "dx9") return CreateHostDX9();
#endif
#if OMEGAGUI_BACKEND_DX10
	if (strBackend == "dx10") return CreateHostDX10();
#endif
#if OMEGAGUI_BACKEND_DX11
	if (strBackend == "dx11") return CreateHostDX11();
#endif
#if OMEGAGUI_BACKEND_DX12
	if (strBackend == "dx12") return CreateHostDX12();
#endif
#if OMEGAGUI_BACKEND_OPENGL
	if (strBackend == "opengl") return CreateHostOpenGL();
#endif
#if OMEGAGUI_BACKEND_VULKAN
	if (strBackend == "vulkan") return CreateHostVulkan();
#endif
	return nullptr;
}

namespace
{
	bool g_bResizePending = false;
	int g_iWidth = 1600;
	int g_iHeight = 900;

	LRESULT CALLBACK WindowProcedure(HWND hWindow, UINT uMessage, WPARAM uWParam, LPARAM iLParam)
	{
		if (OmegaGUI::GetCurrentContext() && OmegaGUI::ProcessWin32Message(hWindow, uMessage, uWParam, iLParam))
			return uMessage == WM_SETCURSOR ? TRUE : 0;
		switch (uMessage)
		{
		case WM_SIZE:
			if (uWParam != SIZE_MINIMIZED)
			{
				g_iWidth = LOWORD(iLParam);
				g_iHeight = HIWORD(iLParam);
				g_bResizePending = true;
			}
			return 0;
		case WM_SYSCOMMAND:
			if ((uWParam & 0xFFF0) == SC_KEYMENU)
				return 0;
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			break;
		}
		return DefWindowProcW(hWindow, uMessage, uWParam, iLParam);
	}

	bool SaveWindowBitmap(HWND hWindow, const std::filesystem::path& pathFile)
	{
		RECT stClient{};
		GetClientRect(hWindow, &stClient);
		const int iWidth = stClient.right - stClient.left;
		const int iHeight = stClient.bottom - stClient.top;
		HDC hWindowDC = GetDC(hWindow);
		HDC hMemoryDC = CreateCompatibleDC(hWindowDC);
		HBITMAP hBitmap = CreateCompatibleBitmap(hWindowDC, iWidth, iHeight);
		HGDIOBJ hPrevious = SelectObject(hMemoryDC, hBitmap);
		const bool bPrinted = PrintWindow(hWindow, hMemoryDC, PW_CLIENTONLY | PW_RENDERFULLCONTENT) != FALSE;

		BITMAPINFOHEADER stInfo{ sizeof(BITMAPINFOHEADER), iWidth, -iHeight, 1, 32, BI_RGB };
		std::vector<std::uint8_t> aPixels(static_cast<std::size_t>(iWidth) * iHeight * 4);
		GetDIBits(hMemoryDC, hBitmap, 0, static_cast<UINT>(iHeight), aPixels.data(), reinterpret_cast<BITMAPINFO*>(&stInfo), DIB_RGB_COLORS);
		SelectObject(hMemoryDC, hPrevious);
		DeleteObject(hBitmap);
		DeleteDC(hMemoryDC);
		ReleaseDC(hWindow, hWindowDC);

		std::ofstream fileOut(pathFile, std::ios::binary);
		if (!fileOut || !bPrinted)
			return false;
		BITMAPFILEHEADER stFile{};
		stFile.bfType = 0x4D42;
		stFile.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
		stFile.bfSize = stFile.bfOffBits + static_cast<DWORD>(aPixels.size());
		fileOut.write(reinterpret_cast<const char*>(&stFile), sizeof(stFile));
		fileOut.write(reinterpret_cast<const char*>(&stInfo), sizeof(stInfo));
		fileOut.write(reinterpret_cast<const char*>(aPixels.data()), static_cast<std::streamsize>(aPixels.size()));
		return true;
	}

	[[nodiscard]] std::string Narrow(const wchar_t* szWide)
	{
		std::string strResult;
		for (; *szWide; ++szWide)
			strResult.push_back(static_cast<char>(*szWide));
		return strResult;
	}
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
	std::string strBackend = "dx11";
	std::filesystem::path pathScreenshot;
	int iExitAfterFrames = -1;
	bool bMenuHidden = false;
	std::string strStartTab;
	std::string strLanguage;
	bool bSelfTest = false;
	std::filesystem::path pathOutput = ".";

	int iArgumentCount = 0;
	LPWSTR* aArguments = CommandLineToArgvW(GetCommandLineW(), &iArgumentCount);
	for (int i = 1; i < iArgumentCount; ++i)
	{
		const std::string strArgument = Narrow(aArguments[i]);
		if (strArgument.starts_with("--backend="))
			strBackend = strArgument.substr(10);
		else if (strArgument.starts_with("--screenshot="))
			pathScreenshot = strArgument.substr(13);
		else if (strArgument.starts_with("--frames="))
			iExitAfterFrames = std::stoi(strArgument.substr(9));
		else if (strArgument.starts_with("--tab="))
			strStartTab = strArgument.substr(6);
		else if (strArgument.starts_with("--lang="))
			strLanguage = strArgument.substr(7);
		else if (strArgument == "--selftest")
			bSelfTest = true;
		else if (strArgument.starts_with("--output="))
			pathOutput = strArgument.substr(9);
		else if (strArgument == "--hidden")
			bMenuHidden = true;
		else if (strArgument == "--warp")
			g_bForceWarp = true;
	}
	LocalFree(aArguments);

	WNDCLASSEXW stClass{ sizeof(WNDCLASSEXW), CS_CLASSDC, WindowProcedure, 0, 0, hInstance, nullptr, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"OmegaGUIDemo", nullptr };
	RegisterClassExW(&stClass);
	const std::wstring strTitle = L"OmegaGUI Demo - " + std::wstring(strBackend.begin(), strBackend.end());
	RECT stRect{ 0, 0, g_iWidth, g_iHeight };
	AdjustWindowRect(&stRect, WS_OVERLAPPEDWINDOW, FALSE);
	HWND hWindow = CreateWindowExW(0, stClass.lpszClassName, strTitle.c_str(), WS_OVERLAPPEDWINDOW, 60, 60, stRect.right - stRect.left, stRect.bottom - stRect.top, nullptr, nullptr, hInstance, nullptr);

	std::unique_ptr<clHost> pHost = CreateHost(strBackend);
	if (!pHost || !pHost->Create(hWindow, g_iWidth, g_iHeight))
	{
		MessageBoxW(hWindow, L"Failed to create the requested rendering backend.", strTitle.c_str(), MB_ICONERROR);
		DestroyWindow(hWindow);
		return 1;
	}
	ShowWindow(hWindow, SW_SHOWDEFAULT);
	UpdateWindow(hWindow);

	OmegaGUI::stConfig stConfig;
	stConfig.pathSettings = bSelfTest ? std::filesystem::path() : std::filesystem::path("omegagui.ini");
	stConfig.bManualGamepad = bSelfTest;
	stConfig.bStartVisible = !bMenuHidden;
	stConfig.bPollCursorPosition = false;
	stConfig.bBlockInputWhenVisible = false;
	if (!OmegaGUI::Initialize(pHost->Backend(), hWindow, stConfig))
	{
		const std::string_view strError = OmegaGUI::GetInitError();
		const std::wstring strMessage = L"Failed to initialise OmegaGUI: " + std::wstring(strError.begin(), strError.end());
		MessageBoxW(hWindow, strMessage.c_str(), strTitle.c_str(), MB_ICONERROR);
		return 1;
	}
	DemoInitialize(pHost->Backend()->GetName(), strStartTab, strLanguage);

	std::unique_ptr<clSelfTest> pSelfTest;
	if (bSelfTest)
		pSelfTest = std::make_unique<clSelfTest>([&](const std::string& strName) { SaveWindowBitmap(hWindow, pathOutput / (strName + ".bmp")); });

	const stDemoCamera stCamera{ pHost->ClipSpaceYDown(), pHost->DepthMinusOneToOne() };
	const float aClearColor[4] = { .055f, .055f, .062f, 1.f };
	int iFrame = 0;
	bool bRunning = true;
	while (bRunning)
	{
		MSG stMessage{};
		while (PeekMessageW(&stMessage, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&stMessage);
			DispatchMessageW(&stMessage);
			if (stMessage.message == WM_QUIT)
				bRunning = false;
		}
		if (!bRunning)
			break;
		if (g_bResizePending)
		{
			pHost->Resize(g_iWidth, g_iHeight);
			g_bResizePending = false;
		}

		if (pSelfTest)
		{
			pSelfTest->BeforeFrame();
			if (pSelfTest->IsDone())
				break;
		}
		pHost->BeginFrame(aClearColor);
		OmegaGUI::NewFrame();
		DemoFrame(stCamera);
		OmegaGUI::EndFrame();
		OmegaGUI::RenderWorld({});
		OmegaGUI::Render();
		pHost->EndFrame();

		++iFrame;
		if (iExitAfterFrames > 0 && iFrame >= iExitAfterFrames)
		{
			if (!pathScreenshot.empty())
				SaveWindowBitmap(hWindow, pathScreenshot);
			break;
		}
	}

	int iExitCode = 0;
	if (pSelfTest)
	{
		pSelfTest->WriteReport(pathOutput / "selftest.txt");
		iExitCode = pSelfTest->Failures() > 0 ? 2 : 0;
	}

	OmegaGUI::Shutdown();
	pHost.reset();
	DestroyWindow(hWindow);
	UnregisterClassW(stClass.lpszClassName, hInstance);
	return iExitCode;
}
