#include "Demo/DemoUI.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <format>
#include <variant>
#include <vector>

#ifdef OMEGAGUI_DEMO_LOCALIZATION
#include "OmegaLocalization/GUI.hpp"
#include "Locales/Locales.hpp"
#define DEMO_TEXT(szKey, szEnglish) OmegaLocalization::Get(szKey)
#define DEMO_LABEL(szKey, szEnglish) OmegaLocalization::Label(szKey)
#else
#define DEMO_TEXT(szKey, szEnglish) std::string_view(szEnglish)
#define DEMO_LABEL(szKey, szEnglish) std::string_view(szEnglish)
#endif

namespace
{
	using namespace OmegaGUI;

	struct stEntity
	{
		std::string strName;
		vec3 vecPosition;
		float flHealth = 1.f;
	};

	struct stDemoState
	{
		std::string strBackend;
		std::string strStartTab;
		bool bShowConsole = true;
		bool bShowPerformance = true;
		bool bOpenAbout = false;

		bool bAimEnabled = true;
		float flFov = 90.f;
		float flSmoothing = .35f;
		int iBone = 0;
		stHotkey stAimKey{ .uKey = 0x02, .eMode = EHotkeyMode::Hold, .bNotify = true };
		bool bVisibleOnly = true;
		int iPriority = 0;

		bool bBoxes = true;
		bool bNames = true;
		bool bHealth = true;
		bool bXRay = true;
		bool bSnaplines = false;
		bool bScreenBoxes = false;
		int iBoxStyle = 0;
		float flMaxDistance = 60.f;
		color colVisible = color::Hex(0x85CFFF);
		color colHidden = color::Hex(0xFF7B7B, 120);
		color colText = color::Hex(0xE4E4EA);

		bool bWalls = true;
		bool bGrid = true;
		bool bAxes = true;
		bool bOrbit = true;
		float flOrbitSpeed = .25f;
		float flCameraAngle = .6f;

		std::string strName = "omega";
		std::string strPassword;
		std::string strSearch;
		int iCount = 3;
		float flValue = 1.5f;
		bool bFeatureA = true;
		bool bFeatureB = false;
		bool bFeatureC = true;
		int iSelected = 1;
		float flProgress = 0.f;
		std::array<bool, 3> aMenuChecks{ true, false, true };

		int iTheme = 0;
		int iControls = 0;
		float flScale = 1.f;

		std::deque<float> aFrameTimes;
		std::vector<stEntity> aEntities;
		clConsole clLog;
	};

	stDemoState g_stDemo;
	stDemoProbe g_stProbe;

	void Track(const std::string& strName)
	{
		g_stProbe.mapRects[strName] = { GetItemRectMin(), GetItemRectMax() };
	}

	constexpr std::string_view aBones[] = { "head", "neck", "chest", "pelvis" };
	constexpr std::string_view aBoxStyles[] = { "3d box", "2d box", "corners" };
	constexpr std::string_view aThemes[] = { "console", "amber", "phosphor" };

	void DrawWorld(const stDemoCamera& stCamera)
	{
		stDemoState& d = g_stDemo;
		const vec2 vecDisplay = GetDisplaySize();
		if (d.bOrbit)
			d.flCameraAngle += GetDeltaTime() * d.flOrbitSpeed;

		const vec3 vecEye{ std::sin(d.flCameraAngle) * 11.f, 4.5f, -std::cos(d.flCameraAngle) * 11.f };
		const mat4 matView = mat4::LookAtLH(vecEye, { 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f });
		const mat4 matProjection = mat4::PerspectiveFovLH(1.0472f, vecDisplay.x / std::max(vecDisplay.y, 1.f), .1f, 300.f);
		mat4 matViewProjection = matView * matProjection;
		if (stCamera.bClipSpaceYDown)
			for (int iRow = 0; iRow < 4; ++iRow)
				matViewProjection.At(iRow, 1) = -matViewProjection.At(iRow, 1);

		clWorldDrawList& rWorld = GetWorldDrawList();
		rWorld.SetCamera(matViewProjection, vecDisplay, stCamera.bClipSpaceYDown);
		const stStyle& rStyle = GetStyle();

		if (d.bGrid)
			rWorld.AddGrid({ 0.f, 0.f, 0.f }, 24.f, 24, rStyle.Get(EColor::Border), EDepthMode::Visible, { color(), 1.f });
		if (d.bAxes)
			rWorld.AddAxes({ 0.f, .01f, 0.f }, 1.5f, EDepthMode::Visible);
		if (d.bWalls)
		{
			const color colWall = color::Hex(0x2B2B2F);
			const color colEdge = rStyle.Get(EColor::TextDisabled);
			rWorld.AddBoxFilled({ -4.f, 0.f, -.2f }, { 4.f, 3.f, .2f }, colWall, EDepthMode::WriteDepth);
			rWorld.AddBox({ -4.f, 0.f, -.2f }, { 4.f, 3.f, .2f }, colEdge, EDepthMode::Visible, { color(), 1.f });
			rWorld.AddBoxFilled({ 5.f, 0.f, -3.f }, { 5.6f, 2.4f, 3.f }, colWall.Lerp(Colors::Black, .2f), EDepthMode::WriteDepth);
			rWorld.AddBox({ 5.f, 0.f, -3.f }, { 5.6f, 2.4f, 3.f }, colEdge, EDepthMode::Visible, { color(), 1.f });
		}

		const float flTime = static_cast<float>(GetTime());
		d.aEntities[0].vecPosition.x = std::sin(flTime * .7f) * 3.f;
		d.aEntities[1].flHealth = .5f + .5f * std::sin(flTime * .9f);

		clDrawList& rBackground = GetBackgroundDrawList();
		const EDepthMode eMode = d.bXRay ? EDepthMode::XRay : EDepthMode::Visible;
		const stWorldStyle stStyleWorld{ d.colHidden, 1.5f };
		for (const stEntity& rEntity : d.aEntities)
		{
			const vec3 vecMin = rEntity.vecPosition - vec3{ .4f, 0.f, .4f };
			const vec3 vecMax = rEntity.vecPosition + vec3{ .4f, 1.8f, .4f };
			const vec3 vecHead = rEntity.vecPosition + vec3{ 0.f, 1.8f, 0.f };
			if ((vecEye - rEntity.vecPosition).Length() > d.flMaxDistance)
				continue;

			if (d.bBoxes && d.iBoxStyle == 0)
				rWorld.AddBox(vecMin, vecMax, d.colVisible, eMode, stStyleWorld);
			if (d.bBoxes && d.iBoxStyle > 0)
			{
				const auto optFeet = rWorld.WorldToScreen(rEntity.vecPosition);
				const auto optTop = rWorld.WorldToScreen(vecHead);
				if (optFeet && optTop)
				{
					const float flHeight = optFeet->y - optTop->y;
					const vec2 vecPixelMin{ -flHeight * .25f, -flHeight };
					const vec2 vecPixelMax{ flHeight * .25f, 0.f };
					if (d.bScreenBoxes)
					{
						const vec2 vecMinScreen = *optFeet + vecPixelMin;
						const vec2 vecMaxScreen = *optFeet + vecPixelMax;
						if (d.iBoxStyle == 2)
							rBackground.AddCornerBox(vecMinScreen, vecMaxScreen, d.colVisible, .25f, 1.5f);
						else
							rBackground.AddRect(vecMinScreen, vecMaxScreen, d.colVisible, 0.f, 1.5f);
					}
					else
						rWorld.AddScreenRect(rEntity.vecPosition, vecPixelMin, vecPixelMax, d.colVisible, false, eMode, stStyleWorld);
				}
			}
			if (d.bNames)
				rWorld.AddText(vecHead + vec3{ 0.f, .35f, 0.f }, rEntity.strName, d.colText, true, {}, eMode, stStyleWorld);
			if (d.bHealth)
			{
				rWorld.AddScreenRect(vecHead, { -22.f, -6.f }, { 22.f, -2.f }, color(0, 0, 0, 160), true, eMode, { color(0, 0, 0, 90), 1.f });
				rWorld.AddScreenRect(vecHead, { -21.f, -5.f }, { -21.f + 42.f * rEntity.flHealth, -3.f }, color::Hex(0x7EE2A8), true, eMode, { color::Hex(0x7EE2A8, 90), 1.f });
			}
			if (d.bSnaplines)
				if (const auto optFeet = rWorld.WorldToScreen(rEntity.vecPosition))
					rBackground.AddLine({ vecDisplay.x * .5f, vecDisplay.y }, *optFeet, d.colVisible.WithAlpha(150), 1.f);
		}

		if (d.bAimEnabled)
		{
			const float flRadius = std::tan(d.flFov * .5f * 3.14159f / 180.f) / std::tan(1.0472f * .5f) * vecDisplay.y * .5f * .25f;
			rBackground.AddCircle(vecDisplay * .5f, flRadius, rStyle.Get(EColor::Accent).WithAlpha(90), 0, 1.f);
		}
	}

	void DrawOverlays()
	{
		stDemoState& d = g_stDemo;
		const stStyle& rStyle = GetStyle();
		const vec2 vecDisplay = GetDisplaySize();

		clDrawList& rBackground = GetBackgroundDrawList();
		float flAverage = 0.f;
		for (const float flTime : d.aFrameTimes)
			flAverage += flTime;
		flAverage = d.aFrameTimes.empty() ? 0.f : flAverage / static_cast<float>(d.aFrameTimes.size());
		const std::string strWatermark = std::format("omegaware // {} // {:.0f} fps", d.strBackend, flAverage > 0.f ? 1.f / flAverage : 0.f);
		const vec2 vecText = CalcTextSize(strWatermark);
		const vec2 vecPadding{ 12.f, 7.f };
		const vec2 vecMin{ 12.f, 12.f };
		const vec2 vecMax = vecMin + vecText + vecPadding * 2.f + vec2{ CalcTextSize("> ").x, 0.f };
		rBackground.AddShadowRect(vecMin, vecMax, rStyle.Get(EColor::Shadow), 16.f, rStyle.flRoundingSmall, { 0.f, 4.f });
		rBackground.AddRectFilled(vecMin, vecMax, rStyle.Get(EColor::Surface).WithAlpha(235), rStyle.flRoundingSmall);
		rBackground.AddRect(vecMin, vecMax, rStyle.Get(EColor::Border), rStyle.flRoundingSmall);
		rBackground.AddRectFilled(vecMin, { vecMax.x, vecMin.y + 2.f }, rStyle.Get(EColor::Accent), rStyle.flRoundingSmall, ECorner::Top);
		rBackground.AddText(vecMin + vecPadding, rStyle.Get(EColor::Accent), "> ");
		rBackground.AddText(vecMin + vecPadding + vec2{ CalcTextSize("> ").x, 0.f }, rStyle.Get(EColor::Text), strWatermark);

		clDrawList& rForeground = GetForegroundDrawList();
		const vec2 vecCenter = (vecDisplay * .5f).Floor();
		const color colCross = rStyle.Get(EColor::Accent);
		rForeground.AddRectFilled(vecCenter - vec2{ 7.f, 0.f }, vecCenter + vec2{ -2.f, 1.f }, colCross);
		rForeground.AddRectFilled(vecCenter + vec2{ 3.f, 0.f }, vecCenter + vec2{ 8.f, 1.f }, colCross);
		rForeground.AddRectFilled(vecCenter - vec2{ 0.f, 7.f }, vecCenter + vec2{ 1.f, -2.f }, colCross);
		rForeground.AddRectFilled(vecCenter + vec2{ 0.f, 3.f }, vecCenter + vec2{ 1.f, 8.f }, colCross);

		if (!IsVisible())
		{
			const std::string strHint = std::format("press [{}] or [View + Menu] to open the menu", KeyName(GetConfig().uToggleKey));
			const vec2 vecHint = CalcTextSize(strHint);
			rForeground.AddTextShadowed({ std::floor(vecDisplay.x * .5f - vecHint.x * .5f), vecDisplay.y - 40.f }, rStyle.Get(EColor::TextMuted), strHint);
		}
	}

	void TabAim()
	{
		stDemoState& d = g_stDemo;
		Breadcrumbs(std::array<std::string_view, 2>{ "features", "aim" });
		Title("aim assist");
		TextMuted("demo controls only, no game is attached.");
		Header("general");
		Toggle("enabled", &d.bAimEnabled);
		ItemTooltip("master switch for the feature");
		BeginDisabled(!d.bAimEnabled);
		SliderFloat("field of view", &d.flFov, 1.f, 180.f, "{:.1f} deg", 1.f);
		SliderFloat("smoothing", &d.flSmoothing, 0.f, 1.f, "{:.2f}");
		int iBone = d.iBone;
		Combo("target bone", &iBone, aBones);
		d.iBone = iBone;
		Hotkey("activation key", &d.stAimKey);
		Track("hotkey_aim");
		Checkbox("visible targets only", &d.bVisibleOnly);
		Header("priority");
		RadioButton("closest to crosshair", &d.iPriority, 0);
		SameLine(0.f, 18.f);
		RadioButton("lowest health", &d.iPriority, 1);
		SameLine(0.f, 18.f);
		RadioButton("distance", &d.iPriority, 2);
		EndDisabled();
		Spacing();
		Alert(EToastKind::Info, "the circle in the middle of the screen is drawn on the background layer, under every window.");
	}

	void TableEntities()
	{
		stDemoState& d = g_stDemo;
		if (!BeginTable("##entities", 3, ETableFlags::Borders | ETableFlags::RowBackground | ETableFlags::Sortable | ETableFlags::HighlightHoveredRow))
			return;
		TableSetupColumn("name", ETableColumnFlags::DefaultSort, 2.f);
		TableSetupColumn("health");
		TableSetupColumn("distance");
		TableHeadersRow();
		Track("table_header_2");

		std::vector<const stEntity*> aRows;
		for (const stEntity& rEntity : d.aEntities)
			aRows.push_back(&rEntity);
		stTableSortSpecs stSort;
		if (stTableSortSpecs* pSort = TableGetSortSpecs())
		{
			stSort = *pSort;
			pSort->bDirty = false;
		}
		const auto Key = [&](const stEntity* pEntity) -> std::variant<std::string_view, float> {
			if (stSort.iColumn == 1)
				return pEntity->flHealth;
			if (stSort.iColumn == 2)
				return pEntity->vecPosition.Length();
			return std::string_view(pEntity->strName);
		};
		std::ranges::stable_sort(aRows, [&](const stEntity* pLeft, const stEntity* pRight) { return stSort.bAscending ? Key(pLeft) < Key(pRight) : Key(pRight) < Key(pLeft); });

		for (const stEntity* pEntity : aRows)
		{
			TableNextRow();
			TableNextColumn();
			Selectable(pEntity->strName);
			TableNextColumn();
			ProgressBar(pEntity->flHealth, { 0.f, 0.f }, std::format("{:.0f}%", pEntity->flHealth * 100.f));
			TableNextColumn();
			AlignTextToFramePadding();
			Text("{:.1f} m", pEntity->vecPosition.Length());
		}
		EndTable();
		Track("table_entities");

		g_stProbe.iTableSortColumn = stSort.iColumn;
		g_stProbe.bTableAscending = stSort.bAscending;
		g_stProbe.strTableFirst = aRows.empty() ? std::string() : aRows.front()->strName;
	}

	void TabVisuals()
	{
		stDemoState& d = g_stDemo;
		Breadcrumbs(std::array<std::string_view, 2>{ "features", "visuals" });
		Title("visuals");
		if (BeginTabBar("##visuals"))
		{
			if (BeginTabItem("players"))
			{
				Checkbox("boxes", &d.bBoxes);
				SameLine(0.f, 24.f);
				Checkbox("names", &d.bNames);
				SameLine(0.f, 24.f);
				Checkbox("health", &d.bHealth);
				int iStyle = d.iBoxStyle;
				Combo("box style", &iStyle, aBoxStyles);
				d.iBoxStyle = iStyle;
				Toggle("x-ray through walls", &d.bXRay);
				ItemTooltip("draws the hidden part in the occluded colour instead of hiding it");
				Toggle("2d boxes on top (no occlusion)", &d.bScreenBoxes);
				Toggle("snaplines", &d.bSnaplines);
				SliderFloat("max distance", &d.flMaxDistance, 5.f, 200.f, "{:.0f} m");
				EndTabItem();
			}
			const bool bList = BeginTabItem("list");
			Track("visuals_list");
			if (bList)
			{
				TableEntities();
				EndTabItem();
			}
			const bool bColors = BeginTabItem("colors");
			Track("visuals_colors");
			if (bColors)
			{
				ColorEdit("visible", &d.colVisible);
				Track("color_visible");
				ColorEdit("occluded", &d.colHidden);
				ColorEdit("labels", &d.colText, false);
				EndTabItem();
			}
			if (BeginTabItem("about"))
			{
				TextWrapped("boxes, names and health bars here are drawn with the world draw list. every vertex is projected on the cpu and depth-tested on the gpu against the depth buffer, so they hide behind the grey walls the same way they would hide behind level geometry in a game.");
				Spacing();
				TextWrapped("in a real game you point the backend at the game's depth-stencil view (SetWorldTarget on d3d10/11, a DSV handle on d3d12, the bound depth on opengl, a depth attachment on vulkan) and hook a point after the scene draws but before the game's own hud.");
				EndTabItem();
			}
			EndTabBar();
		}
	}

	void TabWorld()
	{
		stDemoState& d = g_stDemo;
		Breadcrumbs(std::array<std::string_view, 2>{ "features", "world" });
		Title("scene");
		Toggle("walls (write depth)", &d.bWalls);
		Toggle("ground grid", &d.bGrid);
		Toggle("axes", &d.bAxes);
		Toggle("orbit camera", &d.bOrbit);
		SliderFloat("orbit speed", &d.flOrbitSpeed, -1.f, 1.f, "{:.2f}");
		SliderFloat("camera angle", &d.flCameraAngle, -3.14159f, 3.14159f * 3.f, "{:.2f} rad");
		Header("entities");
		for (std::size_t i = 0; i < d.aEntities.size(); ++i)
		{
			stEntity& rEntity = d.aEntities[i];
			PushID(static_cast<int>(i));
			if (TreeNode(rEntity.strName))
			{
				SliderFloat("x", &rEntity.vecPosition.x, -8.f, 8.f);
				SliderFloat("z", &rEntity.vecPosition.z, -8.f, 8.f);
				SliderFloat("health", &rEntity.flHealth, 0.f, 1.f);
				TreePop();
			}
			PopID();
		}
	}

	void TabWidgets()
	{
		stDemoState& d = g_stDemo;
		Breadcrumbs(std::array<std::string_view, 2>{ "showcase", "widgets" });
		Title("widgets");

		Header("buttons");
		if (Button("primary"))
			PushToast(EToastKind::Success, "primary button pressed");
		SameLine();
		if (Button("secondary", {}, EButtonStyle::Secondary))
			PushToast(EToastKind::Info, "secondary button pressed");
		SameLine();
		if (Button("danger", {}, EButtonStyle::Danger))
			PushToast(EToastKind::Error, "something went wrong (on purpose)");
		SameLine();
		Button("ghost", {}, EButtonStyle::Ghost);
		SameLine();
		SmallButton("small");
		if (BeginPopupContextItem())
		{
			TextMuted("context menu");
			Separator();
			if (Selectable("copy"))
				PushToast(EToastKind::Info, "copied");
			if (Selectable("delete"))
				PushToast(EToastKind::Warning, "deleted");
			EndPopup();
		}
		ItemTooltip("right click (or X / Square on a controller) for a context menu");
		AlignTextToFramePadding();
		TextLink("open about");
		Track("link_about");
		if (IsItemClicked())
			d.bOpenAbout = true;
		SameLine(0.f, 16.f);
		Tag("beta");
		SameLine();
		Tag("co-op", GetStyle().Get(EColor::Success));
		SameLine();
		Tag("unsafe", GetStyle().Get(EColor::Danger));

		Header("input");
		InputText("name", &d.strName, "player name");
		Track("input_name");
		InputText("password", &d.strPassword, "hidden", EInputTextFlags::Password);
		InputInt("count", &d.iCount, 1, 0, 99);
		InputFloat("value", &d.flValue, .25f);
		SliderInt("slider int", &d.iCount, 0, 99);
		Track("slider_int");

		Header("selection");
		BeginColumns(2);
		Checkbox("feature a", &d.bFeatureA);
		Track("cb_feature_a");
		Checkbox("feature b", &d.bFeatureB);
		Track("cb_feature_b");
		Checkbox("feature c", &d.bFeatureC);
		NextColumn();
		if (BeginChild("##list", { 0.f, 110.f }))
		{
			for (int i = 0; i < 12; ++i)
			{
				PushID(i);
				if (Selectable(std::format("entry_{:02}.cfg", i), d.iSelected == i))
					d.iSelected = i;
				PopID();
			}
		}
		EndChild();
		EndColumns();

		Header("feedback");
		d.flProgress = std::fmod(d.flProgress + GetDeltaTime() * .15f, 1.f);
		ProgressBar(d.flProgress);
		std::vector<float> aValues(d.aFrameTimes.begin(), d.aFrameTimes.end());
		for (float& rValue : aValues)
			rValue *= 1000.f;
		PlotLines("frame time (ms)", aValues);
		std::array<float, 16> aBars{};
		for (std::size_t i = 0; i < aBars.size(); ++i)
			aBars[i] = .5f + .5f * std::sin(static_cast<float>(GetTime()) * 1.3f + static_cast<float>(i) * .5f);
		PlotHistogram("##bars", aBars, 0.f, 1.f, { 0.f, 48.f });

		Header("structure");
		if (CollapsingHeader("collapsing header", true))
		{
			BulletText("bullet text");
			if (TreeNode("tree node"))
			{
				TextMuted("nested content");
				if (TreeNode("deeper"))
				{
					TextDisabled("all the way down");
					TreePop();
				}
				TreePop();
			}
		}
		Spacing();
		Alert(EToastKind::Success, "settings saved to omegagui.ini");
		Spacing();
		Alert(EToastKind::Warning, "this build is a demo");
		Spacing();
		Alert(EToastKind::Error, "failed to attach (expected, there is no game)");
	}

	void TabSettings()
	{
		stDemoState& d = g_stDemo;
		stStyle& rStyle = GetStyle();
		Breadcrumbs(std::array<std::string_view, 2>{ "menu", "settings" });
		Title(DEMO_TEXT("settings.title", "settings"));
		Header(DEMO_TEXT("settings.appearance", "appearance"));
		int iTheme = d.iTheme;
		const bool bThemeChanged = Combo(DEMO_LABEL("settings.theme", "theme"), &iTheme, aThemes);
		Track("combo_theme");
#ifdef OMEGAGUI_DEMO_LOCALIZATION
		OmegaLocalization::GUI::LanguageSelector(DEMO_LABEL("settings.language", "language"));
#endif
		if (bThemeChanged)
		{
			d.iTheme = iTheme;
			const stStyle stTheme = iTheme == 1 ? stStyle::ConsoleAmber() : iTheme == 2 ? stStyle::ConsoleGreen() : stStyle::Console();
			rStyle.aColors = stTheme.aColors;
		}
		AlignTextToFramePadding();
		TextUnformatted("window controls");
		SameLine(0.f, 24.f);
		if (RadioButton("dots", &d.iControls, 0))
			rStyle.eWindowControls = EWindowControls::Dots;
		SameLine(0.f, 18.f);
		if (RadioButton("brackets", &d.iControls, 1))
			rStyle.eWindowControls = EWindowControls::Brackets;
		Toggle("dot grid background", &rStyle.bDotGrid);
		Toggle("console prefixes", &rStyle.bConsolePrefixes);
		Toggle("animations", &rStyle.bAnimations);
		SliderFloat("ui scale", &d.flScale, .75f, 2.f, "{:.2f}x", .05f);
		if (SmallButton("apply scale"))
			SetScale(d.flScale);

		Header(DEMO_TEXT("settings.input", "input"));
		Toggle("keyboard navigation", &GetConfig().bKeyboardNavigation);
		Toggle("controller navigation", &GetConfig().bGamepadNavigation);
		Toggle("software cursor", &GetConfig().bDrawSoftwareCursor);
		Toggle("block game input while open", &GetConfig().bBlockInputWhenVisible);
		Hotkey("menu key", &GetConfig().uToggleKey);
		const stGamepadState& rPad = GetInput().GetGamepad();
		LabelText("controller", rPad.bConnected ? (rPad.eType == EGamepadType::Xbox ? "xbox (xinput)" : rPad.eType == EGamepadType::DualSense ? "dualsense (hid)" : "dualshock 4 (hid)") : "none connected");

		Header(DEMO_TEXT("settings.notifications", "notifications"));
		if (Button("info", {}, EButtonStyle::Secondary))
			PushToast(EToastKind::Info, "this is an info toast");
		SameLine();
		if (Button("success", {}, EButtonStyle::Secondary))
			PushToast(EToastKind::Success, "config loaded");
		SameLine();
		if (Button("warning", {}, EButtonStyle::Secondary))
			PushToast(EToastKind::Warning, "high ping detected");
		SameLine();
		if (Button("error", {}, EButtonStyle::Secondary))
			PushToast(EToastKind::Error, "connection lost");

		Header("layout");
		if (Button("save layout"))
			PushToast(SaveSettings() ? EToastKind::Success : EToastKind::Error, "window layout saved");
		SameLine();
		if (Button("reload layout", {}, EButtonStyle::Secondary))
			PushToast(LoadSettings() ? EToastKind::Success : EToastKind::Error, "window layout loaded");

		Spacing();
		Separator();
		ControllerHints();
	}

	void MainWindow()
	{
		stDemoState& d = g_stDemo;
		SetNextWindowPos({ 80.f, 80.f });
		SetNextWindowSize({ 760.f, 560.f });
		const bool bMainOpen = Begin("omegaware", nullptr, EWindowFlags::MenuBar | EWindowFlags::CloseHidesMenu);
		if (IsVisible())
			g_stProbe.mapRects["main_window"] = { GetWindowPos(), GetWindowPos() + GetWindowSize() };
		if (bMainOpen)
		{
			if (BeginMenuBar())
			{
				if (BeginMenu("file"))
				{
					if (MenuItem("save layout", "ctrl+s"))
						SaveSettings();
					if (MenuItem("load layout"))
						LoadSettings();
					if (BeginMenu("recent"))
					{
						MenuItem("legit.cfg");
						MenuItem("rage.cfg");
						MenuItem("streamer.cfg");
						OmegaGUI::EndMenu();
					}
					MenuItem("exit", "alt+f4");
					OmegaGUI::EndMenu();
				}
				if (BeginMenu("view"))
				{
					MenuItem("console", {}, &d.bShowConsole);
					MenuItem("performance", {}, &d.bShowPerformance);
					OmegaGUI::EndMenu();
				}
				if (BeginMenu("help"))
				{
					if (MenuItem("about"))
						d.bOpenAbout = true;
					OmegaGUI::EndMenu();
				}
				EndMenuBar();
			}

			if (!d.strStartTab.empty())
			{
				SetTabItemSelected("##sidebar", d.strStartTab);
				d.strStartTab.clear();
			}
			if (BeginTabBar("##sidebar", ETabBarFlags::Vertical))
			{
				using FnTab = void (*)();
				constexpr std::pair<const char*, FnTab> aTabs[] = { { "aim", TabAim }, { "visuals", TabVisuals }, { "world", TabWorld }, { "widgets", TabWidgets }, { "settings", TabSettings } };
				for (const auto& [szName, fnTab] : aTabs)
				{
					const bool bOpen = BeginTabItem(szName);
					Track(std::string("tab_") + szName);
					if (bOpen)
					{
						g_stProbe.strSidebarTab = szName;
						fnTab();
						EndTabItem();
					}
				}
				EndTabBar();
			}

			if (d.bOpenAbout)
			{
				OpenPopup("about omegagui");
				d.bOpenAbout = false;
			}
			if (BeginPopupModal("about omegagui"))
			{
				g_stProbe.mapRects["about_modal"] = { GetWindowPos(), GetWindowPos() + GetWindowSize() };
				Title("omegagui");
				TextMuted(std::format("backend: {}", d.strBackend));
				TextWrapped("a console-styled immediate mode ui for the omegaware framework.");
				Spacing();
				const bool bCloseAbout = Button("close", { 120.f, 0.f });
				Track("about_close");
				if (bCloseAbout)
					CloseCurrentPopup();
				EndPopup();
			}
		}
		End();
	}

	void PerformanceWindow()
	{
		stDemoState& d = g_stDemo;
		if (!d.bShowPerformance)
			return;
		SetNextWindowPos({ GetDisplaySize().x - 340.f, 80.f });
		SetNextWindowSize({ 300.f, 210.f });
		const bool bPerfOpen = Begin("perf", &d.bShowPerformance);
		g_stProbe.vecPerfPosition = GetWindowPos();
		g_stProbe.vecPerfSize = GetWindowSize();
		g_stProbe.bPerfCollapsed = IsWindowCollapsed();
		g_stProbe.mapRects["perf_window"] = { GetWindowPos(), GetWindowPos() + GetWindowSize() };
		if (bPerfOpen)
		{
			std::vector<float> aValues(d.aFrameTimes.begin(), d.aFrameTimes.end());
			for (float& rValue : aValues)
				rValue *= 1000.f;
			const float flLast = aValues.empty() ? 0.f : aValues.back();
			LabelText("frame", std::format("{:.2f} ms", flLast));
			LabelText("backend", d.strBackend);
			PlotLines("##frametimes", aValues, 0.f, 0.f, { 0.f, -1.f });
		}
		End();
	}
}

void DemoInitialize(std::string_view strBackendName, std::string_view strStartTab, std::string_view strLanguage)
{
	stDemoState& d = g_stDemo;
	d.strBackend = std::string(strBackendName);
	d.strStartTab = std::string(strStartTab);
	d.aEntities = {
		{ "bot_alpha", { 0.f, 0.f, 3.5f }, .8f },
		{ "bot_bravo", { 2.5f, 0.f, -3.f }, .5f },
		{ "bot_charlie", { -6.f, 0.f, 1.f }, 1.f },
		{ "bot_delta", { 7.f, 0.f, .5f }, .25f },
	};

	d.clLog.Print(ELogLevel::Success, "omegagui initialised on {}", d.strBackend);
#ifdef OMEGAGUI_DEMO_LOCALIZATION
	std::string strLocaleErrors;
	OmegaLocalization::GUI::Attach({ g_stLocaleEnglish, g_stLocaleGerman, g_stLocalePolish, g_stLocaleRussian }, "en", "Locales", &strLocaleErrors);
	if (!strLanguage.empty())
		OmegaLocalization::SetLocale(strLanguage);
	d.clLog.Print(strLocaleErrors.empty() ? ELogLevel::Success : ELogLevel::Warning, "localization: {} language(s), using '{}' {}", OmegaLocalization::GetLocales().size(), OmegaLocalization::GetLocale(), strLocaleErrors);
#endif
	d.clLog.Log(ELogLevel::Muted, "type 'help' for commands, tab completes, up/down walks history");
	d.clLog.RegisterCommand("toast", "toast <text> - show a notification", [](clConsole&, std::span<const std::string_view> aArguments) {
		std::string strText;
		for (std::size_t i = 1; i < aArguments.size(); ++i)
			strText += std::string(aArguments[i]) + (i + 1 < aArguments.size() ? " " : "");
		PushToast(EToastKind::Info, strText.empty() ? "hello from the console" : strText);
	});
	d.clLog.RegisterCommand("fov", "fov <degrees> - set the aim fov", [](clConsole& rConsole, std::span<const std::string_view> aArguments) {
		if (aArguments.size() < 2)
		{
			rConsole.Print(ELogLevel::Info, "fov = {:.1f}", g_stDemo.flFov);
			return;
		}
		g_stDemo.flFov = std::clamp(static_cast<float>(std::atof(std::string(aArguments[1]).c_str())), 1.f, 180.f);
		rConsole.Print(ELogLevel::Success, "fov set to {:.1f}", g_stDemo.flFov);
	});
	d.clLog.RegisterCommand("theme", "theme <console|amber|phosphor>", [](clConsole& rConsole, std::span<const std::string_view> aArguments) {
		if (aArguments.size() < 2)
			return rConsole.Log(ELogLevel::Warning, "usage: theme <console|amber|phosphor>");
		const int iTheme = aArguments[1] == "amber" ? 1 : aArguments[1] == "phosphor" ? 2 : 0;
		g_stDemo.iTheme = iTheme;
		GetStyle().aColors = (iTheme == 1 ? stStyle::ConsoleAmber() : iTheme == 2 ? stStyle::ConsoleGreen() : stStyle::Console()).aColors;
		rConsole.Print(ELogLevel::Success, "theme set to {}", aThemes[iTheme]);
	});
}

const stDemoProbe& DemoProbe()
{
	return g_stProbe;
}

void DemoFrame(const stDemoCamera& stCamera)
{
	stDemoState& d = g_stDemo;
	g_stProbe.mapRects.clear();
	d.aFrameTimes.push_back(GetDeltaTime());
	while (d.aFrameTimes.size() > 120)
		d.aFrameTimes.pop_front();

	if (UpdateHotkey(d.stAimKey) && d.stAimKey.bNotify)
		PushToast(d.stAimKey.bActive ? EToastKind::Success : EToastKind::Info, d.stAimKey.bActive ? "aim assist on" : "aim assist off", 1.5f);

	DrawWorld(stCamera);
	DrawOverlays();
	MainWindow();
	PerformanceWindow();
	if (d.bShowConsole)
	{
		SetNextWindowPos({ GetDisplaySize().x - 660.f, GetDisplaySize().y - 420.f });
		d.clLog.Draw("console", &d.bShowConsole);
	}

	g_stProbe.bFeatureA = d.bFeatureA;
	g_stProbe.bFeatureB = d.bFeatureB;
	g_stProbe.strName = d.strName;
	g_stProbe.iCount = d.iCount;
	g_stProbe.flFov = d.flFov;
	g_stProbe.iTheme = d.iTheme;
	g_stProbe.bShowConsole = d.bShowConsole;
	g_stProbe.uAimKey = d.stAimKey.uKey;
	g_stProbe.eAimMode = d.stAimKey.eMode;
	g_stProbe.bMenuVisible = IsVisible();
	g_stProbe.mapRects["console_input"] = d.clLog.GetInputRect();
	g_stProbe.mapRects["console_window"] = d.clLog.GetWindowRect();
	g_stProbe.mapRects["console_log"] = d.clLog.GetLogRect();
	g_stProbe.strConsoleInput = d.clLog.GetInput();
	g_stProbe.strConsoleSelection = d.clLog.GetText(true);
	g_stProbe.strConsoleText = d.clLog.GetText(false);
}
