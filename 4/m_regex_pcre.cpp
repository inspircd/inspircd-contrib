/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2016, 2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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
/// $ModDesc: Provides the pcre regular expression engine which uses the PCRE library.

/// $CompilerFlags: execute("pcre-config --cflags" "PCRE_CXXFLAGS")
/// $LinkerFlags: execute("pcre-config --libs" "PCRE_LDFLAGS" "-lpcre")

/// $PackageInfo: require_system("arch") pcre
/// $PackageInfo: require_system("darwin") pcre
/// $PackageInfo: require_system("debian~") libpcre3-dev
/// $PackageInfo: require_system("rhel~") pcre-devel


#include "inspircd.h"
#include "modules/regex.h"

#include <pcre.h>

#ifdef _WIN32
# pragma comment(lib, "pcre.lib")
#endif

class PCREPattern final
	: public Regex::Pattern
{
 private:
	pcre* regex;

 public:
	PCREPattern(const Module* mod, const std::string& pattern, uint8_t options)
		: Regex::Pattern(pattern, options)
	{
		int flags = 0;
		if (options & Regex::OPT_CASE_INSENSITIVE)
			flags &= PCRE_CASELESS;

		const char* error;
		int erroroffset;
		regex = pcre_compile(pattern.c_str(), flags, &error, &erroroffset, nullptr);
		if (!regex)
			throw Regex::Exception(mod, pattern, error, erroroffset);
	}

	~PCREPattern() override
	{
		pcre_free(regex);
	}

	bool IsMatch(const std::string& text) override
	{
		// This cast is potentially unsafe but it's what pcre_exec expects.
		return pcre_exec(regex, nullptr, text.c_str(), int(text.length()), 0, 0, nullptr, 0) >= 0;
	}

	std::optional<Regex::MatchCollection> Matches(const std::string& text) override
	{
		if (!IsMatch(text))
			return std::nullopt;

		// The old pcre engine does not support any kind of capture.
		static const Regex::Captures unusedc;
		static const Regex::NamedCaptures unusednc;

		return Regex::MatchCollection(unusedc, unusednc);
	}
};

class ModuleRegexPCRE final
	: public Module
{
 private:
	Regex::SimpleEngine<PCREPattern> regex;

 public:
	ModuleRegexPCRE()
		: Module(VF_NONE, "Provides the pcre regular expression engine which uses the PCRE library.")
		, regex(this, "pcre")
	{
	}
};

MODULE_INIT(ModuleRegexPCRE)
