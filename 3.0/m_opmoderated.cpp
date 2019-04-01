/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

/// $ModDepends: core 3.0
/// $ModDesc: Implements channel mode +U and extban 'u' - moderator mute


#include "inspircd.h"
#include "modules/exemption.h"

class ModuleOpModerated : public Module
{
	SimpleChannelModeHandler opmod;
	CheckExemption::EventProvider exemptionprov;

 public:
	ModuleOpModerated()
		: opmod(this, "opmoderated", 'U')
		, exemptionprov(this)
	{
	}

	void Prioritize() CXX11_OVERRIDE
	{
		// since we steal the message, we should be last (let everyone else eat it first)
		ServerInstance->Modules->SetPriority(this, I_OnUserPreMessage, PRIORITY_LAST);
	}

	ModResult OnUserPreMessage(User *user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if (target.type != MessageTarget::TYPE_CHANNEL)
			return MOD_RES_PASSTHRU;

		if (!IS_LOCAL(user) || target.status)
			return MOD_RES_PASSTHRU;

		if (user->HasPrivPermission("channels/ignore-opmoderated"))
			return MOD_RES_PASSTHRU;

		Channel* const chan = target.Get<Channel>();
		if (CheckExemption::Call(exemptionprov, user, chan, "opmoderated") == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		if (!chan->GetExtBanStatus(user, 'u').check(!chan->IsModeSet(&opmod)) && chan->GetPrefixValue(user) < VOICE_VALUE)
		{
			MessageTarget newtarget = target;
			newtarget.status = '@';

			FOREACH_MOD(OnUserMessage, (user, newtarget, details));
			ClientProtocol::Messages::Privmsg msg(user, chan, details.text, details.type);
			chan->Write(ServerInstance->GetRFCEvents().privmsg, msg, newtarget.status, details.exemptions);
			FOREACH_MOD(OnUserPostMessage, (user, newtarget, details));

			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('u');
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements opmoderated channel mode +U (non-voiced messages sent to ops) and extban 'u'", VF_OPTCOMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleOpModerated)
