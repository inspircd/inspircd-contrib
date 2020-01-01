/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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
/* $ModConfig: <blockhighlight ignoreextmsg="yes" minlen="50" minusernum="10" reason="Mass highlight spam is not allowed" stripcolor="yes"> */
/* $ModDesc: Adds a channel mode which kills clients that mass highlight spam. */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleBlockHighlight : public Module
{
	SimpleChannelModeHandler mode;
	bool ignoreextmsg;
	unsigned int minlen;
	unsigned int minusers;
	std::string reason;
	bool stripcolor;

public:
	ModuleBlockHighlight()
		: mode(this, "blockhighlight", 'V')
	{
	}

	void init()
	{
		OnRehash(NULL);
		Implementation eventList[] = { I_OnRehash, I_OnUserPreMessage, I_OnUserPreNotice };
		ServerInstance->Modules->Attach(eventList, this, sizeof(eventList)/sizeof(Implementation));
		ServerInstance->Modules->AddService(mode);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("blockhighlight");
		ignoreextmsg = tag->getBool("ignoreextmsg", true);
		minlen = tag->getInt("minlen", 50);
		minusers = tag->getInt("minusernum", 10);
		reason = tag->getString("reason", "Mass highlight spam is not allowed");
		stripcolor = tag->getBool("stripcolor", true);

		if (minlen < 1)
			minlen = 1;

		if (minusers < 2)
			minusers = 2;
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list)
	{
		if ((target_type != TYPE_CHANNEL) || (!IS_LOCAL(user)))
			return MOD_RES_PASSTHRU;

		// Must be at least minlen long
		if (text.length() < minlen)
			return MOD_RES_PASSTHRU;

		Channel* const chan = static_cast<Channel*>(dest);
		if (chan->GetUsers()->size() < minusers)
			return MOD_RES_PASSTHRU;

		// We only work if the channel mode is enabled.
		if (!chan->IsModeSet(mode.GetModeChar()))
			return MOD_RES_PASSTHRU;

		// Exempt operators with the channels/mass-highlight privilege.
		if (user->HasPrivPermission("channels/mass-highlight"))
			return MOD_RES_PASSTHRU;

		// Exempt users who match a blockhighlight entry.
		if (ServerInstance->OnCheckExemption(user, chan, "blockhighlight") == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		// Prevent the enumeration of channel members if enabled.
		if (!chan->IsModeSet('n') && !chan->HasUser(user) && ignoreextmsg)
			return MOD_RES_PASSTHRU;

		std::string message = text;
		if (stripcolor)
			InspIRCd::StripColor(message);

		irc::spacesepstream ss(message);
		unsigned int count = 0;
		for (std::string token; ss.GetToken(token); )
		{
			// Chop off trailing :
			if ((token.length() > 1) && (token[token.length()-1] == ':'))
				token.erase(token.length()-1);

			User* const highlighted = ServerInstance->FindNickOnly(token);
			if (!highlighted)
				continue;

			if (!chan->HasUser(highlighted))
				continue;

			// Highlighted someone
			count++;
			if (count >= minusers)
			{
				ServerInstance->Users->QuitUser(user, reason);
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNotice(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}

	Version GetVersion()
	{
		return Version("Adds a channel mode which kills clients that mass highlight spam.");
	}
};

MODULE_INIT(ModuleBlockHighlight)
