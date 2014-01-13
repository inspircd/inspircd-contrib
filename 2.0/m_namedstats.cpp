/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
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

/* $ModAuthor: Attila Molnar */
/* $ModAuthorMail: attilamolnar@hush.com */
/* $ModDesc: Allows /STATS queries by name */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleNamedStats : public Module
{
	typedef std::map<irc::string, char> NamedStatsMap;
	NamedStatsMap statsmap;

	void LoadDefaults()
	{
		statsmap["kline"] = 'k';
		statsmap["gline"] = 'g';
		statsmap["eline"] = 'e';
		statsmap["zline"] = 'Z';
		statsmap["qline"] = 'q';
		statsmap["filter"] = 's';
		statsmap["cban"] = 'C';
		statsmap["linkblock"] = 'c';
		statsmap["dnsbl"] = 'd';
		statsmap["conn"] = 'l';
		statsmap["cmd"] = 'm';
		statsmap["operacc"] = 'o';
		statsmap["opertype"] = 'O';
		statsmap["port"] = 'p';
		statsmap["uptime"] = 'u';
		statsmap["debug"] = 'z';
		statsmap["connectblock"] = 'I';
		statsmap["connectclass"] = 'i';
		statsmap["client"] = 'L';
		statsmap["oper"] = 'P';
		statsmap["socket"] = 'T';
		statsmap["uline"] = 'U';
		statsmap["connectclass"] = 'Y';
		statsmap["shun"] = 'H';
		statsmap["rline"] = 'R';
		statsmap["geoip"] = 'G';
		statsmap["svshold"] = 'S';
		statsmap["socketengine"] = 'E';
	}

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnPreCommand, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnPreCommand(std::string& command, std::vector<std::string>& parameters, LocalUser* user, bool validated, const std::string& original_line)
	{
		if (!validated || command != "STATS" || parameters[0].length() == 1)
			return MOD_RES_PASSTHRU;

		NamedStatsMap::const_iterator it = statsmap.find(irc::string(parameters[0].c_str()));
		if (it != statsmap.end())
			parameters[0] = it->second;

		return MOD_RES_PASSTHRU;
	}

	void OnRehash(User* user)
	{
		statsmap.clear();
		if (ServerInstance->Config->ConfValue("namedstats")->getBool("enabledefaults", true))
			LoadDefaults();

		ConfigTagList tags = ServerInstance->Config->ConfTags("statsname");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string name = tag->getString("name");
			std::string ch = tag->getString("char");

			if ((!name.empty()) && (ch.length() == 1))
			{
				if (!statsmap.insert(std::make_pair(irc::string(name.c_str()), ch[0])).second)
					ServerInstance->Logs->Log("m_namedstats", DEFAULT, "Name already exists in <statsname> entry at " + tag->getTagLocation());
			}
			else
			{
				ServerInstance->Logs->Log("m_namedstats", DEFAULT, "Invalid name or char in <statsname> entry at " + tag->getTagLocation());
			}
		}
	}

	Version GetVersion()
	{
		return Version("Allows /STATS queries by name");
	}
};

MODULE_INIT(ModuleNamedStats)
