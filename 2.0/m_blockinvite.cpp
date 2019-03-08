/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 genius3000 <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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
/* $ModDesc: Provides usermode 'V' - block all INVITEs */
/* $ModDepends: core 2.0 */
/* $ModConfig: <blockinvite reply="no"> */
/* Set whether to reply with a blocked message to the source (inviter)
 * with the 'reply' config option. Defaults to no.
 * To allow oper overriding, add the privilege 'users/override-blockinvite'
 * to your preferred oper class.
 */

/* Helpop Lines for the UMODES section
 * Find: '<helpop key="umodes" value="User Modes'
 * Place just before the 'W    Receives notif...' line
 V            Blocks all INVITEs from other users (requires
              blockinvite extras-module).
 */

#include "inspircd.h"


enum
{
	// From UnrealIRCd (channel mode, but same concept)
	ERR_NOINVITE = 518
};

class ModuleBlockInvite : public Module
{
 private:
	SimpleUserModeHandler bi;
	bool reply;

 public:
	ModuleBlockInvite()
		: bi(this, "blockinvite", 'V')
		, reply(false)
	{
	}

	void init()
	{
		OnRehash(NULL);
		Implementation eventList[] = { I_OnRehash, I_OnUserPreInvite };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
		ServerInstance->Modules->AddService(bi);
	}

	void OnRehash(User*)
	{
		reply = ServerInstance->Config->ConfValue("blockinvite")->getBool("reply");
	}

	ModResult OnUserPreInvite(User* source, User* dest, Channel*, time_t)
	{
		if (!IS_LOCAL(source) || !dest->IsModeSet(bi.GetModeChar()))
			return MOD_RES_PASSTHRU;

		if (source->HasPrivPermission("users/override-blockinvite"))
			return MOD_RES_PASSTHRU;

		if (reply)
			source->WriteNumeric(ERR_NOINVITE, "%s :Can't INVITE %s, they have +%c set", source->nick.c_str(), dest->nick.c_str(), bi.GetModeChar());

		return MOD_RES_DENY;
	}

	void Prioritize()
	{
		// Go before m_allowinvite as it returns ALLOW.
		Module* allowinvite = ServerInstance->Modules->Find("m_allowinvite.so");
		ServerInstance->Modules->SetPriority(this, I_OnUserPreInvite, PRIORITY_BEFORE, allowinvite);
	}

	Version GetVersion()
	{
		return Version("Provides usermode +" + ConvToStr(bi.GetModeChar()) + " to block all INVITEs", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleBlockInvite)
