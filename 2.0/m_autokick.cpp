/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Peter Powell <petpow@saberuk.com>
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


/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModAuthorMail: petpow@saberuk.com */
/* $ModDesc: Automatically kicks people who match a banned mask. */
/* $ModDepends: core 2.0 */
/* $ModConfig: <autokick message="Banned"> */

#include "inspircd.h"

class ModeWatcherBan : public ModeWatcher
{
 public:
	std::string Reason;

	ModeWatcherBan(Module* Creator) : ModeWatcher(Creator, 'b', MODETYPE_CHANNEL) { }

	void AfterMode(User* source, User*, Channel* channel, const std::string& parameter, bool adding, ModeType)
	{
		if (adding)
		{
			unsigned int rank = channel->GetPrefixValue(source);

			const UserMembList* users = channel->GetUsers();
			UserMembCIter iter = users->begin();

			while (iter != users->end())
			{
				// KickUser invalidates the iterator so copy and increment it here.
				UserMembCIter it = iter++;
				if (IS_LOCAL(it->first) && rank > channel->GetPrefixValue(it->first) && channel->CheckBan(it->first, parameter))
				{
					channel->KickUser(ServerInstance->FakeClient, it->first, Reason.c_str());
				}
			}
		}
	}
};

class ModuleAutoKick : public Module
{
 private:
	ModeWatcherBan mw;

 public:
	ModuleAutoKick() : mw(this) { }

	~ModuleAutoKick()
	{
		ServerInstance->Modes->DelModeWatcher(&mw);
	}

	void init()
	{
		ServerInstance->Modes->AddModeWatcher(&mw);
		ServerInstance->Modules->Attach(I_OnRehash, this);
		OnRehash(NULL);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("autokick");
		mw.Reason = tag->getString("message", "Banned");
	}

	Version GetVersion()
	{
		return Version("Automatically kicks people who match a banned mask.", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleAutoKick)
