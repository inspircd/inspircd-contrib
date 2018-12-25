/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
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
/* $ModDesc: Restrict messages until a user has been connected for a specified duration. */
/* $ModDepends: core 2.0 */
/* $ModConfig: <restrictmsg_duration duration="1m" target="both" notify="no" exemptoper="yes" exemptuline="yes" exemptregistered="yes">  */

/* Config descriptions:
 * duration:         time string for how long after sign on to restrict messages. Default: 1m
 * target:           which targets to block: user, chan, or both. Default: both
 * notify:           whether to let the user know their message was blocked. Default: no
 * exemptoper:       whether to exempt messages to opers. Default: yes
 * exemptuline:      whether to exempt messages to U-Lined clients (services). Default: yes
 * exemptregistered: whether to exempt messages from registered (and identified) users. Default: yes
 * Connect Class exemption (add to any connect class block you wish to exempt from this):
 * exemptrestrictmsg="yes"
 */

#include "inspircd.h"
#include "account.h"


class ModuleRestrictMsgDuration : public Module
{
	bool blockuser;
	bool blockchan;
	bool exemptoper;
	bool exemptuline;
	bool exemptregistered;
	bool notify;
	time_t duration;

 public:
	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void Prioritize()
	{
		// Go last to let filter, +R, bans, etc. act first
		ServerInstance->Modules->SetPriority(this, PRIORITY_LAST);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("restrictmsg_duration");

		const std::string target = tag->getString("target", "both");
		if (target != "user" && target != "chan" && target != "both")
		{
			throw ModuleException("Invalid \"target\" of '" + target + "' in <restrictmsg_duration>. Default of both will be used.");
		}

		blockuser = (target != "chan");
		blockchan = (target != "user");
		exemptoper = tag->getBool("exemptoper", true);
		exemptuline = tag->getBool("exemptuline", true);
		exemptregistered = tag->getBool("exemptregistered", true);
		notify = tag->getBool("notify");
		duration = ServerInstance->Duration(tag->getString("duration", "1m"));
	}

	ModResult CheckMessage(User* user, void* dest, int target_type)
	{
		LocalUser* src = IS_LOCAL(user);
		// Only check against non-oper local users
		if (!src || IS_OPER(src))
			return MOD_RES_PASSTHRU;

		// Check their connected duration
		if (src->signon + duration <= ServerInstance->Time())
			return MOD_RES_PASSTHRU;

		// Check for connect class exemption
		if (src->MyClass->config->getBool("exemptrestrictmsg"))
			return MOD_RES_PASSTHRU;

		if (target_type == TYPE_USER)
		{
			if (!blockuser)
				return MOD_RES_PASSTHRU;

			User* dst = (User*)dest;

			// Target is Oper exemption
			if (exemptoper && IS_OPER(dst))
				return MOD_RES_PASSTHRU;
			// Target is on a U-Lined server exemption
			if (exemptuline && ServerInstance->ULine(dst->server))
				return MOD_RES_PASSTHRU;
		}
		else if (target_type == TYPE_CHANNEL && !blockchan)
			return MOD_RES_PASSTHRU;

		// Source is registered (and identified) exemption
		if (exemptregistered)
		{
			const AccountExtItem* accountname = GetAccountExtItem();
			if (accountname)
			{
				const std::string* account = accountname->get(src);
				if (account && !account->empty())
					return MOD_RES_PASSTHRU;
			}
		}

		if (notify)
		{
			if (target_type == TYPE_USER)
				src->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You cannot send messages within the first %lu seconds of connecting.", src->nick.c_str(), ((User*)dest)->nick.c_str(), duration);
			else
				src->WriteNumeric(ERR_CANNOTSENDTOCHAN, "%s %s :You cannot send messages within the first %lu seconds of connecting.", src->nick.c_str(), ((Channel*)dest)->name.c_str(), duration);
		}

		// Finally, deny them the message sending ability :D
		return MOD_RES_DENY;
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
		return Version("Restrict messages until a user has been connected for a specified duration.");
	}
};

MODULE_INIT(ModuleRestrictMsgDuration)
