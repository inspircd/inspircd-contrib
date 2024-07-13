/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004, 2006, 2010 Craig Edwards <brain@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: InspIRCd Developers
/// $ModDepends: core 4
/// $ModDesc: Allows the server administrator to define inappropriate phrases that are not allowed to be used in private or channel messages.


///$CompilerFlags: find_compiler_flags("icu-uc")
///$LinkerFlags: find_linker_flags("icu-uc")
///$CompilerFlags: find_compiler_flags("icu-i18n")
/// $LinkerFlags: find_linker_flags("icu-i18n")

#include "inspircd.h"
#include "modules/exemption.h"
#include "numerichelper.h"
#include "utility/string.h"
#include <codecvt>
#include <locale>
#include <unicode/regex.h>
#include <unicode/unistr.h>

typedef insp::flat_map<std::string, std::string, irc::insensitive_swo> CensorMap;

class ModuleCensor : public Module
{
 private:
	CheckExemption::EventProvider exemptionprov;
	CensorMap censors;
	SimpleUserMode cu;
	SimpleChannelMode cc;
	std::unique_ptr<icu::RegexPattern> emoji_pattern;

	bool IsMixedUTF8(const std::string& text)
	{
		if (text.empty())
			return false;

		enum ScriptType { SCRIPT_UNKNOWN, SCRIPT_LATIN, SCRIPT_NONLATIN };
		ScriptType detected = SCRIPT_UNKNOWN;

		for (const auto& c : text)
		{
			if (static_cast<unsigned char>(c) < 128)
				continue; // ASCII characters are ignored

			if (std::isalpha(static_cast<unsigned char>(c)))
			{
				ScriptType current = std::islower(static_cast<unsigned char>(c)) || std::isupper(static_cast<unsigned char>(c)) ? SCRIPT_LATIN : SCRIPT_NONLATIN;
				if (detected == SCRIPT_UNKNOWN)
				{
					detected = current;
				}
				else if (detected != current)
				{
					return true; // Mixed scripts detected
				}
			}
		}

		return false;
	}

	// Helper function to convert UTF-8 string to UTF-32
	std::u32string to_utf32(const std::string& utf8)
	{
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
		return convert.from_bytes(utf8);
	}

	// Helper function to convert UTF-32 character to UTF-8
	std::string to_utf8(char32_t utf32_char)
	{
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
		return convert.to_bytes(utf32_char);
	}

	bool IsEmojiOnly(const std::string& text)
	{
		UErrorCode status = U_ZERO_ERROR;
		icu::UnicodeString ustr(text.c_str(), "UTF-8");
		std::unique_ptr<icu::RegexMatcher> emoji_matcher(emoji_pattern->matcher(ustr, status));
		if (U_FAILURE(status))
		{
			throw ModuleException(this, "Failed to create regex matcher for emojis");
		}

		// Check if the entire text is matched by the emoji pattern
		return emoji_matcher->matches(status, status);
	}

	bool IsAllowed(const std::string& text)
	{
		static const std::string allowed_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
		std::u32string utf32_text = to_utf32(text);

		for (char32_t c : utf32_text)
		{
			std::string utf8_char = to_utf8(c);
			if (allowed_chars.find(utf8_char) != std::string::npos)
			{
				continue;
			}
			else if (IsEmojiOnly(utf8_char))
			{
				continue; // Allow emojis matching the regex
			}
			else
			{
				return false;
			}
		}

		return true;
	}

 public:
	ModuleCensor()
		: Module(VF_NONE, "Allows the server administrator to define inappropriate phrases that are not allowed to be used in private or channel messages and blocks messages with mixed UTF-8 scripts, only allowing certain Unicode smileys.")
		, exemptionprov(this)
		, cu(this, "u_censor", 'G')
		, cc(this, "censor", 'G')
	{
		UErrorCode status = U_ZERO_ERROR;
		emoji_pattern = std::unique_ptr<icu::RegexPattern>(icu::RegexPattern::compile("\\p{Emoji}", 0, status));
		if (U_FAILURE(status))
		{
			throw ModuleException(this, "Failed to compile emoji regex pattern");
		}
	}

	void ReadConfig(ConfigStatus& status) override
	{
		CensorMap newcensors;
		for (const auto& [_, badword_tag] : ServerInstance->Config->ConfTags("badword"))
		{
			const std::string text = badword_tag->getString("text");
			if (text.empty())
				throw ModuleException(this, "<badword:text> is empty! at " + badword_tag->source.str());

			const std::string replace = badword_tag->getString("replace");
			newcensors[text] = replace;
		}
		censors.swap(newcensors);
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		// Allow IRC operators to bypass the restrictions
		if (user->IsOper())
			return MOD_RES_PASSTHRU;

		switch (target.type)
		{
			case MessageTarget::TYPE_USER:
			{
				User* targuser = target.Get<User>();
				if (!targuser->IsModeSet(cu))
					return MOD_RES_PASSTHRU;
				break;
			}

			case MessageTarget::TYPE_CHANNEL:
			{
				auto* targchan = target.Get<Channel>();
				if (!targchan->IsModeSet(cc))
					return MOD_RES_PASSTHRU;

				ModResult result = exemptionprov.Check(user, targchan, "censor");
				if (result == MOD_RES_ALLOW)
					return MOD_RES_PASSTHRU;
				break;
			}

			default:
				return MOD_RES_PASSTHRU;
		}

		if (IsMixedUTF8(details.text) || !IsAllowed(details.text))
		{
			const std::string msg = "Your message contained disallowed characters and was blocked. IRC operators have been notified (Spamfilter purpose).";

			// Announce to opers
			std::string oper_announcement;
			if (target.type == MessageTarget::TYPE_CHANNEL)
			{
				auto* targchan = target.Get<Channel>();
				oper_announcement = INSP_FORMAT("MixedCharacterUTF8 notice: User {} in channel {} sent a message containing disallowed characters: '{}', which was blocked.", user->nick, targchan->name, details.text);
			}
			else
			{
				auto* targuser = target.Get<User>();
				oper_announcement = INSP_FORMAT("MixedCharacterUTF8 notice: User {} sent a private message to {} containing disallowed characters: '{}', which was blocked.", user->nick, targuser->nick, details.text);
			}
			ServerInstance->SNO.WriteGlobalSno('a', oper_announcement);

			if (target.type == MessageTarget::TYPE_CHANNEL)
				user->WriteNumeric(Numerics::CannotSendTo(target.Get<Channel>(), msg));
			else
				user->WriteNumeric(Numerics::CannotSendTo(target.Get<User>(), msg));
			return MOD_RES_DENY;
		}

		for (const auto& [find, replace] : censors)
		{
			size_t censorpos;
			while ((censorpos = irc::find(details.text, find)) != std::string::npos)
			{
				if (replace.empty())
				{
					const std::string msg = INSP_FORMAT("Your message to this channel contained a banned phrase ({}) and was blocked. IRC operators have been notified (Spamfilter purpose).", find);

					// Announce to opers
					std::string oper_announcement;
					if (target.type == MessageTarget::TYPE_CHANNEL)
					{
						auto* targchan = target.Get<Channel>();
						oper_announcement = INSP_FORMAT("BannedPhrase notice: User {} in channel {} sent a message containing banned phrase ({}): '{}', which was blocked.", user->nick, targchan->name, find, details.text);
					}
					else
					{
						auto* targuser = target.Get<User>();
						oper_announcement = INSP_FORMAT("BannedPhrase notice: User {} sent a private message to {} containing banned phrase ({}): '{}', which was blocked.", user->nick, targuser->nick, find, details.text);
					}
					ServerInstance->SNO.WriteGlobalSno('a', oper_announcement);

					if (target.type == MessageTarget::TYPE_CHANNEL)
						user->WriteNumeric(Numerics::CannotSendTo(target.Get<Channel>(), msg));
					else
						user->WriteNumeric(Numerics::CannotSendTo(target.Get<User>(), msg));
					return MOD_RES_DENY;
				}

				details.text.replace(censorpos, find.size(), replace);
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleCensor)

