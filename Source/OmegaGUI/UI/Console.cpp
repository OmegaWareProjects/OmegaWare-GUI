#include "OmegaGUI/UI/Console.hpp"
#include "OmegaGUI/UI/Internal.hpp"

#include <Windows.h>

#include <cctype>

namespace OmegaGUI
{
	namespace
	{
		[[nodiscard]] std::vector<std::string_view> SplitArguments(std::string_view strLine)
		{
			std::vector<std::string_view> aArguments;
			std::size_t uIndex = 0;
			while (uIndex < strLine.size())
			{
				while (uIndex < strLine.size() && strLine[uIndex] == ' ')
					++uIndex;
				if (uIndex >= strLine.size())
					break;
				if (strLine[uIndex] == '"')
				{
					const std::size_t uEnd = strLine.find('"', uIndex + 1);
					aArguments.push_back(strLine.substr(uIndex + 1, (uEnd == std::string_view::npos ? strLine.size() : uEnd) - uIndex - 1));
					uIndex = uEnd == std::string_view::npos ? strLine.size() : uEnd + 1;
					continue;
				}
				const std::size_t uEnd = strLine.find(' ', uIndex);
				aArguments.push_back(strLine.substr(uIndex, (uEnd == std::string_view::npos ? strLine.size() : uEnd) - uIndex));
				uIndex = uEnd == std::string_view::npos ? strLine.size() : uEnd;
			}
			return aArguments;
		}

		[[nodiscard]] std::vector<std::string_view> SplitUsage(std::string_view strUsage)
		{
			std::vector<std::string_view> aTokens;
			std::size_t uStart = std::string_view::npos;
			int iDepth = 0;
			for (std::size_t i = 0; i <= strUsage.size(); ++i)
			{
				const char chCurrent = i < strUsage.size() ? strUsage[i] : ' ';
				if (chCurrent == '<' || chCurrent == '[')
					++iDepth;
				else if ((chCurrent == '>' || chCurrent == ']') && iDepth > 0)
					--iDepth;
				if (chCurrent == ' ' && iDepth == 0)
				{
					if (uStart != std::string_view::npos)
						aTokens.push_back(strUsage.substr(uStart, i - uStart));
					uStart = std::string_view::npos;
				}
				else if (uStart == std::string_view::npos)
					uStart = i;
			}
			return aTokens;
		}

		[[nodiscard]] std::vector<std::string_view> SplitAlternatives(std::string_view strGroup)
		{
			std::vector<std::string_view> aAlternatives;
			if (strGroup.size() < 2 || (strGroup.front() != '<' && strGroup.front() != '['))
				return aAlternatives;
			strGroup = strGroup.substr(1, strGroup.size() - 2);
			std::size_t uStart = 0;
			int iDepth = 0;
			for (std::size_t i = 0; i <= strGroup.size(); ++i)
			{
				const char chCurrent = i < strGroup.size() ? strGroup[i] : '|';
				if (chCurrent == '<' || chCurrent == '[')
					++iDepth;
				else if ((chCurrent == '>' || chCurrent == ']') && iDepth > 0)
					--iDepth;
				else if (chCurrent == '|' && iDepth == 0)
				{
					aAlternatives.push_back(strGroup.substr(uStart, i - uStart));
					uStart = i + 1;
				}
			}
			return aAlternatives.size() > 1 ? aAlternatives : std::vector<std::string_view>{};
		}

		[[nodiscard]] std::string_view FirstWord(std::string_view strText)
		{
			return strText.substr(0, strText.find(' '));
		}

		[[nodiscard]] bool IsPlaceholder(std::string_view strToken)
		{
			return strToken.empty() || strToken.front() == '<' || strToken.front() == '[';
		}

		[[nodiscard]] std::string Lower(std::string_view strText)
		{
			std::string strLower(strText);
			for (char& rChar : strLower)
				rChar = static_cast<char>(std::tolower(static_cast<unsigned char>(rChar)));
			return strLower;
		}

		[[nodiscard]] bool IsWordChar(char chValue)
		{
			return std::isalnum(static_cast<unsigned char>(chValue)) || chValue == '_' || static_cast<unsigned char>(chValue) >= 0x80;
		}

		[[nodiscard]] std::size_t NextCharacter(std::string_view strText, std::size_t uIndex)
		{
			++uIndex;
			while (uIndex < strText.size() && (static_cast<unsigned char>(strText[uIndex]) & 0xC0) == 0x80)
				++uIndex;
			return uIndex;
		}

		constexpr std::array<std::string_view, 15> kGoodWords = { "ok", "pass", "passed", "success", "succeeded", "active", "enabled", "on", "true", "done", "ready", "loaded", "installed", "attached", "found" };
		constexpr std::array<std::string_view, 15> kBadWords = { "fail", "failed", "failure", "error", "errors", "crashed", "faulted", "false", "off", "disabled", "missing", "unknown", "invalid", "denied", "unavailable" };
		constexpr std::array<std::string_view, 10> kPathExtensions = { ".cpp", ".hpp", ".dll", ".exe", ".json", ".log", ".ini", ".lua", ".bat", ".dmp" };
	}

	clConsole::clConsole()
	{
		RegisterCommand("help", "list commands", [](clConsole& rConsole, std::span<const std::string_view>) {
			for (const auto& [strName, stEntry] : rConsole.m_mapCommands)
				rConsole.Print(ELogLevel::Info, "  {:<12} {}", strName, stEntry.strHelp);
		});
		RegisterCommand("clear", "clear the console", [](clConsole& rConsole, std::span<const std::string_view>) { rConsole.Clear(); });
		RegisterCommand("history", "show command history", [](clConsole& rConsole, std::span<const std::string_view>) {
			for (std::size_t i = 0; i < rConsole.m_aHistory.size(); ++i)
				rConsole.Print(ELogLevel::Muted, "  {:>3}  {}", i, rConsole.m_aHistory[i]);
		});
		RegisterCommand("echo", "print text", [](clConsole& rConsole, std::span<const std::string_view> aArguments) {
			std::string strText;
			for (std::size_t i = 1; i < aArguments.size(); ++i)
			{
				if (i > 1)
					strText += ' ';
				strText += aArguments[i];
			}
			rConsole.Log(ELogLevel::Info, strText);
		});
	}

	void clConsole::RegisterCommand(std::string_view strName, std::string_view strHelp, FnCommand fnCommand, FnComplete fnComplete)
	{
		m_mapCommands[std::string(strName)] = { std::string(strHelp), std::move(fnCommand), std::move(fnComplete) };
	}

	void clConsole::SetCompletion(std::string_view strName, FnComplete fnComplete)
	{
		if (const auto itCommand = m_mapCommands.find(strName); itCommand != m_mapCommands.end())
			itCommand->second.fnComplete = std::move(fnComplete);
	}

	void clConsole::Log(ELogLevel eLevel, std::string_view strText)
	{
		std::size_t uStart = 0;
		while (uStart <= strText.size())
		{
			const std::size_t uEnd = strText.find('\n', uStart);
			stLine& rLine = m_aLines.emplace_back();
			rLine.eLevel = eLevel;
			rLine.strText = std::string(strText.substr(uStart, uEnd == std::string_view::npos ? std::string_view::npos : uEnd - uStart));
			if (eLevel == ELogLevel::Command)
				rLine.strText.insert(0, "$ ");
			Tokenize(rLine);
			if (uEnd == std::string_view::npos)
				break;
			uStart = uEnd + 1;
		}
		while (m_aLines.size() > uMaxLines)
		{
			m_aLines.pop_front();
			++m_uFirstLine;
		}
		if (!m_bSelecting)
			m_bScrollToBottom = true;
	}

	void clConsole::Clear()
	{
		m_uFirstLine += m_aLines.size();
		m_aLines.clear();
		m_bHasSelection = false;
		m_bSelecting = false;
	}

	void clConsole::Execute(std::string_view strLine)
	{
		while (!strLine.empty() && strLine.back() == ' ')
			strLine.remove_suffix(1);
		while (!strLine.empty() && strLine.front() == ' ')
			strLine.remove_prefix(1);
		if (strLine.empty())
			return;

		Log(ELogLevel::Command, strLine);
		m_aHistory.erase(std::remove(m_aHistory.begin(), m_aHistory.end(), strLine), m_aHistory.end());
		m_aHistory.emplace_back(strLine);
		m_iHistoryPosition = -1;

		const std::vector<std::string_view> aArguments = SplitArguments(strLine);
		if (aArguments.empty())
			return;
		const auto itCommand = m_mapCommands.find(aArguments.front());
		if (itCommand == m_mapCommands.end())
			Print(ELogLevel::Error, "unknown command '{}'. try 'help'", aArguments.front());
		else
			itCommand->second.fnCommand(*this, aArguments);
		Log(ELogLevel::Muted, {});
	}

	std::vector<std::string> clConsole::Candidates(std::span<const std::string_view> aArguments) const
	{
		std::vector<std::string> aCandidates;
		const auto itCommand = m_mapCommands.find(aArguments.front());
		if (itCommand == m_mapCommands.end())
			return aCandidates;
		if (itCommand->second.fnComplete)
			return itCommand->second.fnComplete(aArguments);

		std::string_view strUsage = itCommand->second.strHelp;
		if (FirstWord(strUsage) != itCommand->first)
			return aCandidates;
		if (const std::size_t uDescription = strUsage.find(" - "); uDescription != std::string_view::npos)
			strUsage = strUsage.substr(0, uDescription);
		std::vector<std::string_view> aTokens = SplitUsage(strUsage);
		aTokens.erase(aTokens.begin());

		std::size_t uToken = 0;
		for (std::size_t uArgument = 1; uArgument < aArguments.size(); ++uArgument)
		{
			if (uToken >= aTokens.size())
				return aCandidates;
			const std::vector<std::string_view> aAlternatives = SplitAlternatives(aTokens[uToken]);
			const auto itAlternative = std::ranges::find_if(aAlternatives, [&](std::string_view strAlternative) { return FirstWord(strAlternative) == aArguments[uArgument]; });
			if (itAlternative != aAlternatives.end() && itAlternative->find(' ') != std::string_view::npos)
			{
				std::vector<std::string_view> aExpanded = SplitUsage(itAlternative->substr(itAlternative->find(' ') + 1));
				aExpanded.insert(aExpanded.end(), aTokens.begin() + static_cast<std::ptrdiff_t>(uToken) + 1, aTokens.end());
				aTokens = std::move(aExpanded);
				uToken = 0;
				continue;
			}
			++uToken;
		}
		if (uToken >= aTokens.size())
			return aCandidates;
		const std::vector<std::string_view> aAlternatives = SplitAlternatives(aTokens[uToken]);
		if (aAlternatives.empty())
		{
			if (!IsPlaceholder(aTokens[uToken]))
				aCandidates.emplace_back(aTokens[uToken]);
			return aCandidates;
		}
		for (const std::string_view strAlternative : aAlternatives)
			if (const std::string_view strWord = FirstWord(strAlternative); !IsPlaceholder(strWord))
				aCandidates.emplace_back(strWord);
		return aCandidates;
	}

	void clConsole::Autocomplete(ID uInputID)
	{
		const bool bCommand = m_strInput.find(' ') == std::string::npos;
		std::vector<std::string> aOptions;
		std::string_view strPrefix = m_strInput;
		if (bCommand)
		{
			for (const auto& [strName, stEntry] : m_mapCommands)
				aOptions.push_back(strName);
		}
		else
		{
			std::vector<std::string_view> aArguments = SplitArguments(m_strInput);
			if (aArguments.empty())
				return;
			if (m_strInput.back() == ' ')
				strPrefix = {};
			else
			{
				strPrefix = aArguments.back();
				aArguments.pop_back();
			}
			aOptions = Candidates(aArguments);
		}

		const std::string strLowerPrefix = Lower(strPrefix);
		std::vector<std::string> aMatches;
		for (std::string& rOption : aOptions)
			if (Lower(rOption).starts_with(strLowerPrefix))
				aMatches.push_back(std::move(rOption));
		std::ranges::sort(aMatches);
		aMatches.erase(std::unique(aMatches.begin(), aMatches.end()), aMatches.end());
		if (aMatches.empty())
			return;

		const std::string strBase = m_strInput.substr(0, m_strInput.size() - strPrefix.size());
		if (aMatches.size() == 1)
		{
			m_strInput = strBase + aMatches.front() + ' ';
			SetInputTextValue(uInputID, m_strInput);
			return;
		}
		std::size_t uCommon = strPrefix.size();
		while (std::ranges::all_of(aMatches, [&](const std::string& rMatch) { return rMatch.size() > uCommon && rMatch[uCommon] == aMatches.front()[uCommon]; }))
			++uCommon;
		std::string strList;
		for (const std::string& rMatch : aMatches)
			strList += rMatch + "  ";
		Log(ELogLevel::Muted, strList);
		m_strInput = strBase + aMatches.front().substr(0, uCommon);
		SetInputTextValue(uInputID, m_strInput);
	}

	void clConsole::Tokenize(stLine& rLine) const
	{
		const std::string_view strText = rLine.strText;
		rLine.aSpans.clear();
		const auto fnPush = [&](std::size_t uStart, std::size_t uEnd, ETokenRole eRole)
		{
			if (uEnd <= uStart)
				return;
			if (!rLine.aSpans.empty() && rLine.aSpans.back().eRole == eRole && rLine.aSpans.back().uEnd == uStart)
				rLine.aSpans.back().uEnd = static_cast<std::uint32_t>(uEnd);
			else
				rLine.aSpans.push_back({ static_cast<std::uint32_t>(uStart), static_cast<std::uint32_t>(uEnd), eRole });
		};

		std::size_t uIndex = 0;
		if (rLine.eLevel == ELogLevel::Command && strText.starts_with("$ "))
		{
			fnPush(0, 2, ETokenRole::Prompt);
			const std::size_t uEnd = std::min(strText.find(' ', 2), strText.size());
			fnPush(2, uEnd, ETokenRole::Command);
			uIndex = uEnd;
		}

		while (uIndex < strText.size())
		{
			const char chCurrent = strText[uIndex];
			const bool bWordStart = uIndex == 0 || !IsWordChar(strText[uIndex - 1]);
			if (chCurrent == '"' || chCurrent == '\'')
			{
				const std::size_t uClose = strText.find(chCurrent, uIndex + 1);
				if (uClose != std::string_view::npos && (chCurrent == '"' || bWordStart))
				{
					fnPush(uIndex, uClose + 1, ETokenRole::String);
					uIndex = uClose + 1;
					continue;
				}
			}
			if (chCurrent == '[')
			{
				const std::size_t uClose = strText.find(']', uIndex + 1);
				if (uClose != std::string_view::npos && uClose - uIndex <= 32)
				{
					const std::string strTag = Lower(strText.substr(uIndex + 1, uClose - uIndex - 1));
					const ETokenRole eRole = strTag == "error" || strTag == "fail" ? ETokenRole::Bad : strTag == "warning" || strTag == "warn" ? ETokenRole::WarningTag : strTag == "pass" || strTag == "ok" ? ETokenRole::Good : ETokenRole::Tag;
					fnPush(uIndex, uClose + 1, eRole);
					uIndex = uClose + 1;
					continue;
				}
			}
			if (bWordStart && (std::isdigit(static_cast<unsigned char>(chCurrent)) || (chCurrent == '-' && uIndex + 1 < strText.size() && std::isdigit(static_cast<unsigned char>(strText[uIndex + 1])))))
			{
				std::size_t uEnd = uIndex + 1;
				while (uEnd < strText.size() && (std::isxdigit(static_cast<unsigned char>(strText[uEnd])) || strText[uEnd] == 'x' || strText[uEnd] == 'X' || strText[uEnd] == '.' || strText[uEnd] == ':' || strText[uEnd] == '%'))
					++uEnd;
				while (uEnd > uIndex + 1 && (strText[uEnd - 1] == '.' || strText[uEnd - 1] == ':'))
					--uEnd;
				if (uEnd >= strText.size() || !IsWordChar(strText[uEnd]))
				{
					fnPush(uIndex, uEnd, ETokenRole::Number);
					uIndex = uEnd;
					continue;
				}
			}
			if (bWordStart && (IsWordChar(chCurrent) || chCurrent == '/' || chCurrent == '\\' || chCurrent == '.'))
			{
				std::size_t uEnd = uIndex;
				while (uEnd < strText.size() && (IsWordChar(strText[uEnd]) || strText[uEnd] == '.' || strText[uEnd] == '/' || strText[uEnd] == '\\' || strText[uEnd] == ':' || strText[uEnd] == '-'))
					++uEnd;
				while (uEnd > uIndex + 1 && (strText[uEnd - 1] == '.' || strText[uEnd - 1] == ':' || strText[uEnd - 1] == '-'))
					--uEnd;
				const std::string_view strWord = strText.substr(uIndex, uEnd - uIndex);
				const std::string strLower = Lower(strWord);
				ETokenRole eRole = ETokenRole::Base;
				if (strWord.find('/') != std::string_view::npos || strWord.find('\\') != std::string_view::npos || std::ranges::any_of(kPathExtensions, [&](std::string_view strExtension) { return strLower.ends_with(strExtension); }))
					eRole = ETokenRole::Path;
				else if (std::ranges::find(kGoodWords, strLower) != kGoodWords.end())
					eRole = ETokenRole::Good;
				else if (std::ranges::find(kBadWords, strLower) != kBadWords.end())
					eRole = ETokenRole::Bad;
				else if (strLower == "warning" || strLower == "warn")
					eRole = ETokenRole::WarningTag;
				fnPush(uIndex, std::max(uEnd, uIndex + 1), eRole);
				uIndex = std::max(uEnd, uIndex + 1);
				continue;
			}
			const std::size_t uNext = NextCharacter(strText, uIndex);
			fnPush(uIndex, uNext, ETokenRole::Base);
			uIndex = uNext;
		}
		rLine.flLayoutWidth = -1.f;
	}

	void clConsole::Layout(stLine& rLine, float flWidth, float flFontSize) const
	{
		if (rLine.flLayoutWidth == flWidth && rLine.flLayoutFont == flFontSize)
			return;
		rLine.flLayoutWidth = flWidth;
		rLine.flLayoutFont = flFontSize;
		rLine.aRows.clear();
		const clFont* pFont = Ctx().stShared.pFont;
		const std::string_view strText = rLine.strText;
		if (!pFont || strText.empty() || flWidth <= 0.f)
		{
			rLine.aRows.emplace_back(0u, static_cast<std::uint32_t>(strText.size()));
			return;
		}
		std::size_t uIndex = 0;
		while (uIndex < strText.size())
		{
			std::size_t uLength = pFont->FindWrapPosition(strText.substr(uIndex), flFontSize, flWidth);
			if (uLength == 0)
				uLength = NextCharacter(strText, uIndex) - uIndex;
			rLine.aRows.emplace_back(static_cast<std::uint32_t>(uIndex), static_cast<std::uint32_t>(uIndex + uLength));
			uIndex += uLength;
			while (uIndex < strText.size() && strText[uIndex] == ' ')
				++uIndex;
		}
		if (rLine.aRows.empty())
			rLine.aRows.emplace_back(0u, 0u);
	}

	color clConsole::RoleColor(ETokenRole eRole, ELogLevel eLevel) const
	{
		const stStyle& rStyle = Ctx().stCurrentStyle;
		if (eLevel == ELogLevel::Muted)
			return rStyle.Get(EColor::TextDisabled);
		switch (eRole)
		{
		case ETokenRole::Prompt: return rStyle.Get(EColor::Success);
		case ETokenRole::Command: return rStyle.Get(EColor::Accent);
		case ETokenRole::Number: return rStyle.Get(EColor::Accent);
		case ETokenRole::String: return rStyle.Get(EColor::Warning);
		case ETokenRole::Path: return rStyle.Get(EColor::TextHeading);
		case ETokenRole::Tag: return rStyle.Get(EColor::TextMuted);
		case ETokenRole::Good: return rStyle.Get(EColor::Success);
		case ETokenRole::Bad: return rStyle.Get(EColor::Danger);
		case ETokenRole::WarningTag: return rStyle.Get(EColor::Warning);
		case ETokenRole::Muted: return rStyle.Get(EColor::TextMuted);
		default: break;
		}
		switch (eLevel)
		{
		case ELogLevel::Success: return rStyle.Get(EColor::Success);
		case ELogLevel::Warning: return rStyle.Get(EColor::Warning);
		case ELogLevel::Error: return rStyle.Get(EColor::Danger);
		default: return rStyle.Get(EColor::Text);
		}
	}

	clConsole::stLine* clConsole::LineAt(std::uint64_t uLine)
	{
		return uLine >= m_uFirstLine && uLine - m_uFirstLine < m_aLines.size() ? &m_aLines[static_cast<std::size_t>(uLine - m_uFirstLine)] : nullptr;
	}

	const clConsole::stLine* clConsole::LineAt(std::uint64_t uLine) const
	{
		return uLine >= m_uFirstLine && uLine - m_uFirstLine < m_aLines.size() ? &m_aLines[static_cast<std::size_t>(uLine - m_uFirstLine)] : nullptr;
	}

	clConsole::stTextPosition clConsole::HitTest(const vec2& vecMouse) const
	{
		if (m_aVisible.empty() || m_flLineHeight <= 0.f)
			return m_stCaret;
		const stVisibleLine* pHit = &m_aVisible.front();
		if (vecMouse.y < pHit->flTop)
			return { pHit->uLine, 0 };
		for (const stVisibleLine& rVisible : m_aVisible)
			if (vecMouse.y >= rVisible.flTop)
				pHit = &rVisible;
		const stLine* pLine = LineAt(pHit->uLine);
		if (!pLine || pLine->aRows.empty())
			return { pHit->uLine, 0 };
		const std::size_t uRow = static_cast<std::size_t>(std::max(0.f, (vecMouse.y - pHit->flTop) / m_flLineHeight));
		if (uRow >= pLine->aRows.size())
			return { pHit->uLine, static_cast<std::uint32_t>(pLine->strText.size()) };
		const auto [uRowStart, uRowEnd] = pLine->aRows[uRow];
		const std::string_view strText = pLine->strText;
		float flX = pHit->flLeft;
		std::size_t uIndex = uRowStart;
		while (uIndex < uRowEnd)
		{
			const std::size_t uNext = NextCharacter(strText, uIndex);
			const float flAdvance = MeasureText(strText.substr(uIndex, uNext - uIndex)).x;
			if (vecMouse.x < flX + flAdvance * .5f)
				break;
			flX += flAdvance;
			uIndex = uNext;
		}
		return { pHit->uLine, static_cast<std::uint32_t>(uIndex) };
	}

	std::string clConsole::GetText(bool bSelectionOnly) const
	{
		std::string strResult;
		stTextPosition stStart{ m_uFirstLine, 0 };
		stTextPosition stEnd{ m_uFirstLine + m_aLines.size(), 0 };
		if (bSelectionOnly)
		{
			if (!m_bHasSelection)
				return strResult;
			stStart = std::min(m_stAnchor, m_stCaret);
			stEnd = std::max(m_stAnchor, m_stCaret);
		}
		for (std::uint64_t uLine = stStart.uLine; uLine <= stEnd.uLine; ++uLine)
		{
			const stLine* pLine = LineAt(uLine);
			if (!pLine)
				continue;
			const std::size_t uFrom = uLine == stStart.uLine ? std::min<std::size_t>(stStart.uOffset, pLine->strText.size()) : 0;
			const std::size_t uTo = uLine == stEnd.uLine && bSelectionOnly ? std::min<std::size_t>(stEnd.uOffset, pLine->strText.size()) : pLine->strText.size();
			if (!strResult.empty() || uLine != stStart.uLine)
				strResult += "\r\n";
			if (uTo > uFrom)
				strResult += pLine->strText.substr(uFrom, uTo - uFrom);
		}
		return strResult;
	}

	void clConsole::DrawToolbar()
	{
		if (SmallButton("clear"))
			Clear();
		SameLine();
		if (SmallButton(m_bHasSelection ? "copy selection" : "copy all"))
			SetClipboardText(GetText(m_bHasSelection));
		SameLine();
		Checkbox("always visible", pAlwaysVisible ? pAlwaysVisible : &m_bAlwaysVisible);
	}

	void clConsole::DrawLog(float flHeight)
	{
		stContext& g = Ctx();
		m_aVisible.clear();
		if (!BeginChild("##log", { 0.f, flHeight }, true))
		{
			EndChild();
			return;
		}
		stWindow* pWindow = CurrentWindow();
		const float flWidth = std::max(1.f, GetContentRegionAvail().x);
		const float flFontSize = g.stShared.flFontSize;
		m_flLineHeight = GetTextLineHeight();
		const color colSelection = WithGlobalAlpha(g.stCurrentStyle.Get(EColor::Selection));
		const stTextPosition stSelStart = std::min(m_stAnchor, m_stCaret);
		const stTextPosition stSelEnd = std::max(m_stAnchor, m_stCaret);

		for (std::size_t i = 0; i < m_aLines.size(); ++i)
		{
			stLine& rLine = m_aLines[i];
			const std::uint64_t uLine = m_uFirstLine + i;
			Layout(rLine, flWidth, flFontSize);
			const vec2 vecPosition = pWindow->Region().vecCursor;
			const vec2 vecSize{ flWidth, m_flLineHeight * static_cast<float>(rLine.aRows.size()) };
			ItemSize(vecSize);
			if (!ItemAdd({ vecPosition, vecPosition + vecSize }, 0))
				continue;
			m_aVisible.push_back({ uLine, vecPosition.y, vecPosition.x });

			const std::string_view strText = rLine.strText;
			const bool bSelected = m_bHasSelection && uLine >= stSelStart.uLine && uLine <= stSelEnd.uLine;
			const std::size_t uSelFrom = uLine == stSelStart.uLine ? stSelStart.uOffset : 0;
			const std::size_t uSelTo = uLine == stSelEnd.uLine ? stSelEnd.uOffset : strText.size() + 1;
			for (std::size_t uRow = 0; uRow < rLine.aRows.size(); ++uRow)
			{
				const auto [uRowStart, uRowEnd] = rLine.aRows[uRow];
				const float flTop = vecPosition.y + m_flLineHeight * static_cast<float>(uRow);
				if (bSelected)
				{
					const std::size_t uFrom = std::clamp<std::size_t>(uSelFrom, uRowStart, uRowEnd);
					const std::size_t uTo = std::clamp<std::size_t>(uSelTo, uRowStart, uRowEnd);
					const float flFrom = vecPosition.x + MeasureText(strText.substr(uRowStart, uFrom - uRowStart)).x;
					float flTo = vecPosition.x + MeasureText(strText.substr(uRowStart, uTo - uRowStart)).x;
					if (uSelTo > uRowEnd && uRow + 1 == rLine.aRows.size())
						flTo += MeasureText(" ").x;
					if (flTo > flFrom)
						pWindow->drawList.AddRectFilled({ flFrom, flTop }, { flTo, flTop + m_flLineHeight }, colSelection);
				}
				float flX = vecPosition.x;
				for (const stSpan& rSpan : rLine.aSpans)
				{
					const std::size_t uFrom = std::max<std::size_t>(rSpan.uStart, uRowStart);
					const std::size_t uTo = std::min<std::size_t>(rSpan.uEnd, uRowEnd);
					if (uFrom >= uTo)
						continue;
					const std::string_view strPiece = strText.substr(uFrom, uTo - uFrom);
					pWindow->drawList.AddText({ flX, flTop }, WithGlobalAlpha(RoleColor(rSpan.eRole, rLine.eLevel)), strPiece);
					flX += MeasureText(strPiece).x;
				}
			}
		}

		m_rcLog = pWindow->drawList.GetClipRect();
		const ID uSelectID = GetID("##logselect");
		ItemAdd(m_rcLog, uSelectID);
		clInput& rInput = g.clInputState;
		const bool bHovered = ItemHoverable(m_rcLog, uSelectID, EItemFlags::AllowWhenBlocked) && (g.uActiveID == 0 || g.uActiveID == uSelectID || g.uActiveID == g.stText.uID);
		if (bHovered && rInput.IsMouseClicked(EMouseButton::Left))
		{
			if (g.stText.uID)
			{
				if (g.uActiveID == g.stText.uID)
					ClearActiveID();
				g.stText = {};
			}
			SetActiveID(uSelectID, pWindow);
		}
		const bool bHeld = g.uActiveID == uSelectID && rInput.IsMouseDown(EMouseButton::Left);
		if (g.uActiveID == uSelectID)
		{
			KeepAliveID(uSelectID);
			if (!bHeld)
				ClearActiveID();
		}
		if (bHovered)
			g.eCursor = EMouseCursor::TextInput;
		if (bHovered && rInput.IsMouseDoubleClicked(EMouseButton::Left))
		{
			const stTextPosition stHit = HitTest(rInput.GetMousePosition());
			if (const stLine* pLine = LineAt(stHit.uLine))
			{
				m_stAnchor = { stHit.uLine, 0 };
				m_stCaret = { stHit.uLine, static_cast<std::uint32_t>(pLine->strText.size()) };
				m_bHasSelection = true;
				m_bSelecting = false;
			}
		}
		else if (bHovered && rInput.IsMouseClicked(EMouseButton::Left))
		{
			m_stAnchor = m_stCaret = HitTest(rInput.GetMousePosition());
			m_bSelecting = true;
			m_bHasSelection = false;
		}
		else if (m_bSelecting && bHeld)
		{
			m_stCaret = HitTest(rInput.GetMousePosition());
			m_bHasSelection = m_stCaret != m_stAnchor;
			const float flMouseY = rInput.GetMousePosition().y;
			if (flMouseY < m_rcLog.min.y)
				SetScrollY(GetScrollY() - m_flLineHeight);
			else if (flMouseY > m_rcLog.max.y)
				SetScrollY(GetScrollY() + m_flLineHeight);
		}
		else if (m_bSelecting && !bHeld)
			m_bSelecting = false;

		if (m_bScrollToBottom)
		{
			SetScrollHereY(1.f);
			m_bScrollToBottom = false;
		}
		EndChild();
	}

	void clConsole::Draw(std::string_view strTitle, bool* pOpen)
	{
		const bool bAlwaysVisible = pAlwaysVisible ? *pAlwaysVisible : m_bAlwaysVisible;
		SetNextWindowSize({ 620.f, 380.f });
		const bool bVisible = Begin(strTitle, pOpen, EWindowFlags::NoScrollbar | EWindowFlags::NoScrollWithMouse | (bAlwaysVisible ? EWindowFlags::AlwaysVisible : EWindowFlags::None));
		m_rcWindow = { GetWindowPos(), GetWindowPos() + GetWindowSize() };
		if (!bVisible)
		{
			End();
			return;
		}
		stContext& g = Ctx();
		const stStyle& rStyle = g.stCurrentStyle;
		stWindow* pConsoleWindow = CurrentWindow();
		DrawToolbar();
		const float flInputHeight = FrameHeight() + rStyle.vecItemSpacing.y;
		DrawLog(-flInputHeight);

		const float flPrompt = MeasureText("> ").x;
		stWindow* pWindow = CurrentWindow();
		const vec2 vecPrompt = pWindow->Region().vecCursor;
		pWindow->drawList.AddText({ vecPrompt.x, std::floor(vecPrompt.y + (FrameHeight() - GetTextLineHeight()) * .5f) }, WithGlobalAlpha(rStyle.Get(EColor::Accent)), ">");
		Dummy({ flPrompt, FrameHeight() });
		SameLine(0.f, 0.f);
		SetNextItemWidth(-1.f);
		const ID uInputID = GetID("##input");
		const bool bSubmitted = InputText("##input", &m_strInput, "type 'help', tab completes", EInputTextFlags::EnterReturnsTrue | EInputTextFlags::KeepFocusOnEnter);
		m_rcInput = { GetItemRectMin(), GetItemRectMax() };
		if (bSubmitted)
		{
			Execute(m_strInput);
			m_strInput.clear();
			SetInputTextValue(uInputID, {});
		}
		clInput& rInput = g.clInputState;
		if (g.stText.uID == uInputID)
		{
			if (rInput.IsKeyPressed(VK_TAB))
				Autocomplete(uInputID);
			if (!m_aHistory.empty() && (rInput.IsKeyPressed(VK_UP, true) || rInput.IsKeyPressed(VK_DOWN, true)))
			{
				if (rInput.IsKeyPressed(VK_UP, true))
					m_iHistoryPosition = m_iHistoryPosition < 0 ? static_cast<int>(m_aHistory.size()) - 1 : std::max(0, m_iHistoryPosition - 1);
				else if (m_iHistoryPosition >= 0 && ++m_iHistoryPosition >= static_cast<int>(m_aHistory.size()))
					m_iHistoryPosition = -1;
				m_strInput = m_iHistoryPosition >= 0 ? m_aHistory[m_iHistoryPosition] : std::string();
				SetInputTextValue(uInputID, m_strInput);
			}
		}
		else if (g.stText.uID == 0 && g.pFocusedWindow == pConsoleWindow && rInput.IsCtrlDown())
		{
			if (rInput.IsKeyPressed('C') && m_bHasSelection)
				SetClipboardText(GetText(true));
			if (rInput.IsKeyPressed('A') && !m_aLines.empty())
			{
				m_stAnchor = { m_uFirstLine, 0 };
				m_stCaret = { m_uFirstLine + m_aLines.size() - 1, static_cast<std::uint32_t>(m_aLines.back().strText.size()) };
				m_bHasSelection = true;
			}
		}
		End();
	}
}
