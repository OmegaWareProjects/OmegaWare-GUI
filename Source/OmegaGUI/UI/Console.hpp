#pragma once

#include "OmegaGUI/OmegaGUI.hpp"

#include <deque>
#include <map>

namespace OmegaGUI
{
	enum class ELogLevel : std::uint8_t
	{
		Info,
		Success,
		Warning,
		Error,
		Command,
		Muted,
	};

	class clConsole
	{
	public:
		using FnCommand = std::function<void(clConsole& rConsole, std::span<const std::string_view> aArguments)>;
		using FnComplete = std::function<std::vector<std::string>(std::span<const std::string_view> aArguments)>;

		clConsole();

		void RegisterCommand(std::string_view strName, std::string_view strHelp, FnCommand fnCommand, FnComplete fnComplete = {});
		void SetCompletion(std::string_view strName, FnComplete fnComplete);
		void Execute(std::string_view strLine);
		void Log(ELogLevel eLevel, std::string_view strText);
		void Clear();
		void Draw(std::string_view strTitle = "console", bool* pOpen = nullptr);
		[[nodiscard]] std::string GetText(bool bSelectionOnly) const;
		[[nodiscard]] rect GetInputRect() const { return m_rcInput; }
		[[nodiscard]] rect GetWindowRect() const { return m_rcWindow; }
		[[nodiscard]] rect GetLogRect() const { return m_rcLog; }
		[[nodiscard]] const std::string& GetInput() const { return m_strInput; }

		template <typename... Args>
		void Print(ELogLevel eLevel, std::format_string<Args...> strFormat, Args&&... args)
		{
			Log(eLevel, std::format(strFormat, std::forward<Args>(args)...));
		}

		std::size_t uMaxLines = 2000;
		bool* pAlwaysVisible = nullptr;

	private:
		enum class ETokenRole : std::uint8_t
		{
			Base,
			Prompt,
			Command,
			Number,
			String,
			Path,
			Tag,
			Good,
			Bad,
			WarningTag,
			Muted,
		};

		struct stSpan
		{
			std::uint32_t uStart = 0;
			std::uint32_t uEnd = 0;
			ETokenRole eRole = ETokenRole::Base;
		};

		struct stLine
		{
			ELogLevel eLevel = ELogLevel::Info;
			std::string strText;
			std::vector<stSpan> aSpans;
			std::vector<std::pair<std::uint32_t, std::uint32_t>> aRows;
			float flLayoutWidth = -1.f;
			float flLayoutFont = 0.f;
		};

		struct stCommand
		{
			std::string strHelp;
			FnCommand fnCommand;
			FnComplete fnComplete;
		};

		struct stTextPosition
		{
			std::uint64_t uLine = 0;
			std::uint32_t uOffset = 0;

			[[nodiscard]] auto operator<=>(const stTextPosition&) const = default;
		};

		struct stVisibleLine
		{
			std::uint64_t uLine = 0;
			float flTop = 0.f;
			float flLeft = 0.f;
		};

		void Autocomplete(ID uInputID);
		[[nodiscard]] std::vector<std::string> Candidates(std::span<const std::string_view> aArguments) const;
		void Tokenize(stLine& rLine) const;
		void Layout(stLine& rLine, float flWidth, float flFontSize) const;
		[[nodiscard]] color RoleColor(ETokenRole eRole, ELogLevel eLevel) const;
		[[nodiscard]] stLine* LineAt(std::uint64_t uLine);
		[[nodiscard]] const stLine* LineAt(std::uint64_t uLine) const;
		[[nodiscard]] stTextPosition HitTest(const vec2& vecMouse) const;
		void DrawToolbar();
		void DrawLog(float flHeight);

		std::deque<stLine> m_aLines;
		std::uint64_t m_uFirstLine = 0;
		std::vector<std::string> m_aHistory;
		int m_iHistoryPosition = -1;
		std::string m_strInput;
		std::map<std::string, stCommand, std::less<>> m_mapCommands;
		bool m_bScrollToBottom = true;
		bool m_bAlwaysVisible = false;
		bool m_bSelecting = false;
		bool m_bHasSelection = false;
		std::string m_strPendingPaste;
		stTextPosition m_stAnchor;
		stTextPosition m_stCaret;
		std::vector<stVisibleLine> m_aVisible;
		float m_flLineHeight = 0.f;
		rect m_rcLog;
		rect m_rcInput;
		rect m_rcWindow;
	};
}
