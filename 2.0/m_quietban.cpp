/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Shawn Smith <shawn@inspircd.org>
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
#include "u_listmode.h"

/* $ModAuthor: Shawn Smith */
/* $ModAuthorMail: shawn@inspircd.org */
/* $ModDesc: Provides channel mode +q for 'quiet' bans. */
/* $ModDepends: core 2.0-2.1 */
/* $ModConflicts: m_muteban.so */
/* $ModConflicts: m_chanprotect.so */

/* This module was based heavily off of m_banexception.cpp
	which was in turn adapted from m_exception, which was also based
	on m_chanprotect and m_silence. */

/** Handles +q channel mode
*/
class QuietBan : public ListModeBase
{
	public:
		QuietBan(Module* Creator) : ListModeBase(Creator, "quietban", 'q', "End of Channel Quiet List", 728, 729, true) { }

};

class ModuleQuietBan : public Module
{
	QuietBan qb;

	public:
		ModuleQuietBan() : qb(this)
		{
			if (ServerInstance->Modules->Find("m_muteban.so") || ServerInstance->Modules->Find("m_chanprotect.so"))
				throw ModuleException("Can not load with: m_muteban.so or m_chanprotect.so.");

			if (!ServerInstance->Modes->AddMode(&qb))
				throw ModuleException("Could not add new modes!");

			/* Pupulate Implements list with the events for a List Mode */
			qb.DoImplements(this);

			Implementation list[] = { I_OnUserPreNotice, I_OnUserPreMessage };
			ServerInstance->Modules->Attach(list, this, 2);
		}

		virtual ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
		{
			if (target_type == TYPE_CHANNEL)
			{
				Channel* chan = (Channel*)dest;

				/* If the user is +v or higher they can speak regardless. */
				if (chan->GetPrefixValue(user) >= VOICE_VALUE)
					return MOD_RES_PASSTHRU;

				/* Get the list of +q's */
				modelist *list = qb.extItem.get(chan);

				/* No list, continue. */
				if (!list)
					return MOD_RES_PASSTHRU;

				/* Copied from m_banredirect.cpp */
				std::string ipmask(user->nick);
				ipmask.append(1, '!').append(user->MakeHostIP());

				/* If this matches then they match a +q, don't allow them to speak. */
				for (modelist::iterator it = list->begin(); it != list->end(); it++)
					if (InspIRCd::Match(user->GetFullHost(), it->mask) ||
						InspIRCd::Match(user->GetFullRealHost(), it->mask) ||
						InspIRCd::MatchCIDR(ipmask, it->mask))
					{
						/* lol 404 */
						user->WriteNumeric(404, "%s %s :Cannot send to channel (you're muted (+q))", user->nick.c_str(), chan->name.c_str());
						return MOD_RES_DENY;
					}
			}

			return MOD_RES_PASSTHRU;
		}

		virtual ModResult OnUserPreNotice(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
		{
			return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
		}

		void OnCleanup(int target_type, void* item)
		{
			qb.DoCleanup(target_type, item);
		}

		void OnRehash(User* user)
		{
			qb.DoRehash();
		}

		void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
		{
			qb.DoSyncChannel(chan, proto, opaque);
		}
		Version GetVersion()
		{
			return Version("Provides cahnnel mode +q for 'quiet' bans.", VF_OPTCOMMON);
		}
};

MODULE_INIT(ModuleQuietBan)
