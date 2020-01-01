/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2015 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 ChrisTX <chris@rev-crew.info>
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


#include "inspircd.h"
#include "m_regex.h"

// Fix warnings about the use of `long long` on C++03 and
// shadowing on GCC.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-long-long"
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wlong-long"
# pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <re2/re2.h>

/* $ModAuthor: Sadie Powell */
/* $ModDesc: Regex Provider Module for RE2. */
/* $ModDepends: core 2.0 */
/* $LinkerFlags: -lre2 */

class RE2Exception : public ModuleException
{
 public:
	 RE2Exception(const std::string& regex, const std::string& error)
		 : ModuleException("Error in regex '" + regex + "': " + error) { }
};


class RE2Regex : public Regex
{
	RE2 regexcl;

 public:
	RE2Regex(const std::string& rx) : Regex(rx), regexcl(rx, RE2::Quiet)
	{
		if (!regexcl.ok())
		{
			throw RE2Exception(rx, regexcl.error());
		}
	}

	bool Matches(const std::string& text)
	{
		return RE2::FullMatch(text, regexcl);
	}
};

class RE2Factory : public RegexFactory
{
 public:
	RE2Factory(Module* m) : RegexFactory(m, "regex/re2") { }

	Regex* Create(const std::string& expr)
	{
		return new RE2Regex(expr);
	}
};

class ModuleRegexRE2 : public Module
{
	RE2Factory ref;

 public:
	ModuleRegexRE2() : ref(this)
	{
		ServerInstance->Modules->AddService(ref);
	}

	Version GetVersion()
	{
		return Version("Regex Provider Module for RE2");
	}
};

MODULE_INIT(ModuleRegexRE2)
