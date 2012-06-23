/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2012 satmd <satmd@satmd.dyndns.org>
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

/* $ModDesc: Provides support for blocking any private messages (umode +D) */

/** User mode +D - filter out any private messages (non-channel)
 */
class User_D : public ModeHandler
{
 public:
	User_D(Module* Creator) : ModeHandler(Creator, "privdeaf", 'D', PARAM_NONE, MODETYPE_USER) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('D'))
			{
				dest->WriteServ("NOTICE %s :*** You have enabled usermode +D, private deaf mode. This mode means you WILL NOT receive any messages and notices from any nicks. If you did NOT mean to do this, use /mode %s -D.", dest->nick.c_str(), dest->nick.c_str());
				dest->SetMode('D',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('D'))
			{
				dest->SetMode('D',false);
				return MODEACTION_ALLOW;
			}
		}
		return MODEACTION_DENY;
	}
};

class ModulePrivdeaf : public Module
{
	User_D m1;
	bool operoverride;
	bool ulineoverride;

 public:
	ModulePrivdeaf()
		: m1(this)
	{
		if (!ServerInstance->Modes->AddMode(&m1))
			throw ModuleException("Could not add new modes!");

		OnRehash(NULL);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader conf;
		operoverride = conf.ReadFlag("privdeaf", "operoverride", "1", 0);
		ulineoverride = conf.ReadFlag("privdeaf", "ulineoverride", "1", 0);
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type != TYPE_USER)
			return MOD_RES_PASSTHRU;

		User* target = (User*) dest;
		if (!target->IsModeSet('D'))
			return MOD_RES_PASSTHRU;

		if ((operoverride) && (IS_OPER(user)))
			return MOD_RES_PASSTHRU;

		if ((ulineoverride) && (ServerInstance->ULine(user->server)))
			return MOD_RES_PASSTHRU;

		return MOD_RES_DENY;
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for blocking any private messages and notices (umode +D)");
	}

};

MODULE_INIT(ModulePrivdeaf)
