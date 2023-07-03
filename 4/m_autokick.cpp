/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014-2016 Sadie Powell <sadie@witchery.services>
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
/// $ModConfig: <autokick message="Banned">
/// $ModDepends: core 4
/// $ModDesc: Automatically kicks people who match a banned mask.


#include "inspircd.h"

class ModeWatcherBan final
	: public ModeWatcher
{
public:
	std::string reason;

	ModeWatcherBan(Module* Creator)
		: ModeWatcher(Creator, "ban", MODETYPE_CHANNEL)
	{
	}

	void AfterMode(User* source, User* dest, Channel* channel, const Modes::Change& change) override
	{
		if (change.adding)
		{
			ModeHandler::Rank rank = channel->GetPrefixValue(source);

			const Channel::MemberMap& users = channel->GetUsers();
			auto iter = users.begin();

			while (iter != users.end())
			{
				// KickUser invalidates the iterator so copy and increment it here.
				auto it = iter++;
				if (IS_LOCAL(it->first) && rank > channel->GetPrefixValue(it->first) && channel->CheckBan(it->first, change.param))
				{
					channel->KickUser(ServerInstance->FakeClient, it->first, reason.c_str());
				}
			}
		}
	}
};

class ModuleAutoKick final
	: public Module
{
private:
	ModeWatcherBan mw;

public:
	ModuleAutoKick()
		: Module(VF_OPTCOMMON, "Automatically kicks people who match a banned mask.")
		, mw(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("autokick");
		mw.reason = tag->getString("message", "Banned");
	}
};

MODULE_INIT(ModuleAutoKick)
