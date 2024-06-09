/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2024 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell <sadie@witchery.services>
/// $ModDepends: core 4
/// $ModConfig: <delay chan="5m" nick="5m">
/// $ModDesc: Allows delaying the recreation of channels and reuse of nicknames.

#include "inspircd.h"

class ModuleDelayUse final
	: public Module
{
private:
	typedef std::unordered_map<std::string, time_t, irc::insensitive, irc::StrHashComp> DelayList;

	unsigned long chandelay;
	DelayList chanlist;

	unsigned long nickdelay;
	DelayList nicklist;

	static void GarbageCollect(DelayList& list)
	{
		for (auto it = list.begin(); it != list.end(); ) {
			if (it->second < ServerInstance->Time())
				it = list.erase(it); // Expired entry.
			else
				it++;
		}
	}

	static bool InList(DelayList& list, const std::string& item)
	{
		auto it = list.find(item);
		if (it == list.end())
			return false; // No entry.

		if (it->second < ServerInstance->Time())
		{
			list.erase(it);
			return false; // Expired entry.
		}

		return true;
	}

public:
	ModuleDelayUse()
		: Module(VF_COMMON, "Allows delaying the recreation of channels and reuse of nicknames.")
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("delay");
		chandelay = tag->getDuration("chan", 5*60);
		nickdelay = tag->getDuration("nick", 5*60);
	}

	void OnChannelDelete(Channel* chan) override
	{
		chanlist.insert_or_assign(chan->name, ServerInstance->Time() + chandelay);
	}

	void OnGarbageCollect() override
	{
		GarbageCollect(chanlist);
		GarbageCollect(nicklist);
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven, bool override) override
	{
		if (chan || override)
			return MOD_RES_PASSTHRU;

		if (!InList(chanlist, cname))
			return MOD_RES_PASSTHRU;

		user->WriteNumeric(ERR_UNAVAILRESOURCE, cname, "Channel is temporarily unavailable because of chan delay.");
		return MOD_RES_DENY;
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) override
	{
		if (!InList(nicklist, newnick))
			return MOD_RES_PASSTHRU;

		user->WriteNumeric(ERR_UNAVAILRESOURCE, newnick, "Nickname is temporarily unavailable because of nick delay.");
		return MOD_RES_DENY;
	}

	void OnUserPostNick(User* user, const std::string& oldnick) override
	{
		if (user->IsFullyConnected())
			nicklist.insert_or_assign(oldnick, ServerInstance->Time() + nickdelay);
	}
};

MODULE_INIT(ModuleDelayUse)
