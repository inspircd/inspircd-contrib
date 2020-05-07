/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
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

/* $ModAuthor: Sadie Powell */
/* $ModAuthorMail: sadie@witchery.services */
/* $ModDepends: core 2.0 */
/* $ModDesc: Allows users to be managed using services-assigned teams. */


#include "inspircd.h"

enum
{
	// InspIRCd specific.
	RPL_WHOISTEAMS = 695
};

// Represents a list of teams that a user is a member of.
typedef std::vector<std::string> TeamList;

class TeamExt : public ExtensionItem
{
 public:
	TeamExt(Module* Creator)
		: ExtensionItem("teams", Creator)
	{
	}

	void free(void* item)
	{
		delete static_cast<TeamList*>(item);
	}

	TeamList* get(const Extensible* container) const
	{
		return static_cast<TeamList*>(get_raw(container));
	}

	std::string serialize(SerializeFormat format, const Extensible* container, void* item) const
	{
		TeamList* teamlist = static_cast<TeamList*>(item);
		if (!teamlist)
			return "";

		std::string buffer;
		for (TeamList::const_iterator iter = teamlist->begin(); iter != teamlist->end(); ++iter)
		{
			if (!buffer.empty())
				buffer.push_back(' ');
			buffer.append(*iter);
		}
		return buffer;
	}

	void unserialize(SerializeFormat format, Extensible* container, const std::string& value)
	{
		// Create a new team list from the input.
		TeamList* newteamlist = new TeamList();
		irc::spacesepstream teamstream(value);
		for (std::string teamname; teamstream.GetToken(teamname); )
			newteamlist->push_back(teamname);

		if (newteamlist->empty())
		{
			// If the new team list is empty then delete both the new and old team lists.
			void* oldteamlist = unset_raw(container);
			free(oldteamlist);
			free(newteamlist);
		}
		else
		{
			// Otherwise install the new team list.
			void* oldteamlist = set_raw(container, newteamlist);
			free(oldteamlist);
		}
	}
};

class ModuleTeams : public Module
{
 private:
	TeamExt ext;

 public:
	ModuleTeams()
		: ext(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(ext);
		Implementation eventlist[] = { I_OnCheckBan, I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult OnCheckBan(User* user, Channel* channel, const std::string& mask)
	{
		if (mask.length() <= 2 || mask[0] != 't' || mask[1] != ':')
			return MOD_RES_PASSTHRU;

		TeamList* teams = ext.get(user);
		if (!teams)
			return MOD_RES_PASSTHRU;

		const std::string submask = mask.substr(2);
		for (TeamList::const_iterator iter = teams->begin(); iter != teams->end(); ++iter)
		{
			if (InspIRCd::Match(*iter, submask))
				return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void OnWhois(User* source, User* dest)
	{
		TeamList* teams = ext.get(dest);
		if (teams)
		{
			const std::string teamstr = irc::stringjoiner(" ", *teams, 0, teams->size() - 1).GetJoined();
			ServerInstance->SendWhoisLine(source, dest, RPL_WHOISTEAMS, "%s %s %s :is a member of these teams",
				source->nick.c_str(), dest->nick.c_str(), teamstr.c_str());
		}
	}

	Version GetVersion()
	{
		return Version("Allows users to be managed using services-assigned teams", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleTeams)
