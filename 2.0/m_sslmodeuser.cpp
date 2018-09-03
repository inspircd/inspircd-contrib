/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013 Shawn Smith <shawn@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDesc: Provides user mode +z to allow for Secure/SSL only queries and user notices */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
#include "ssl.h"


namespace
{
	bool IsSSLUser(Module* mod, User* user)
	{
		UserCertificateRequest req(user, mod);
		req.Send();
		return req.cert;
	}
}

class SSLModeUser : public ModeHandler
{
 public:
	SSLModeUser(Module* Creator)
		: ModeHandler(Creator, "sslqueries", 'z', PARAM_NONE, MODETYPE_USER)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet(this->GetModeChar()))
			{
				if(!IsSSLUser(creator, dest))
					return MODEACTION_DENY;

				dest->SetMode(this->GetModeChar(), true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet(this->GetModeChar()))
			{
				dest->SetMode(this->GetModeChar(), false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleSSLModeUser : public Module
{
	SSLModeUser sslquery;

 public:
	ModuleSSLModeUser()
		: sslquery(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(sslquery);
		Implementation eventlist[] = { I_OnUserPreNotice, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	ModResult CheckMessage(User* user, void* dest, int target_type)
	{
		if (target_type != TYPE_USER)
			return MOD_RES_PASSTHRU;

		User* target = (User*)dest;

		// If one or both users are on a U-Line, let it through
		if (ServerInstance->ULine(user->server) || ServerInstance->ULine(target->server))
			return MOD_RES_PASSTHRU;

		// Target is +z, check that the source is too
		if(target->IsModeSet(sslquery.GetModeChar()))
		{
			if (!IsSSLUser(this, user))
			{
				user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user (+%c set).",
					user->nick.c_str(), target->nick.c_str(), sslquery.GetModeChar());
				return MOD_RES_DENY;
			}
		}
		// Source is +z, check that the target is too
		else if (user->IsModeSet(sslquery.GetModeChar()))
		{
			if (!IsSSLUser(this, target))
			{
				user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You must remove usermode '%c' before you are able to send private messages to a non-ssl user.",
					user->nick.c_str(), target->nick.c_str(), sslquery.GetModeChar());
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string&, char, CUList&)
	{
		return CheckMessage(user, dest, target_type);
	}

	ModResult OnUserPreNotice(User* user, void* dest, int target_type, std::string&, char, CUList&)
	{
		return CheckMessage(user, dest, target_type);
	}

	Version GetVersion()
	{
		return Version("Provides user mode +z to allow for Secure/SSL only queries and user notices");
	}
};

MODULE_INIT(ModuleSSLModeUser)
