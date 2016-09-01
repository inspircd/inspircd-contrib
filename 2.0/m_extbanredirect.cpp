/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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


/* $ModAuthor: Attila Molnar */
/* $ModAuthorMail: attilamolnar@hush.com */
/* $ModDesc: Provide extended ban <extbanchar>:<chan>:<mask> to redirect users to another channel */
/* $ModDepends: core 2.0 */
/* $ModConfig: <extbanredirect char="d"> */

#include "inspircd.h"

class BanWatcher : public ModeWatcher
{
 public:
	char extbanchar;

	BanWatcher(Module* mod)
		: ModeWatcher(mod, 'b', MODETYPE_CHANNEL)
	{
	}

	bool IsExtBanRedirect(const std::string& mask)
	{
		// d:#targetchan:*!*@Attila.inspircd.org
		return ((mask.length() > 2) && (mask[0] == extbanchar) && ((mask[1] == ':')));
	}

	bool BeforeMode(User* user, User*, Channel* chan, std::string& param, bool adding, ModeType modetype)
	{
		if ((!adding) || (modetype != MODETYPE_CHANNEL))
			return true;

		if ((!IS_LOCAL(user)) || !IsExtBanRedirect(param))
			return true;

		std::string::size_type p = param.find(':', 2);
		if (p == std::string::npos)
		{
			user->WriteNumeric(690, "%s :Extban redirect \"%s\" is invalid. Format: %c:<chan>:<mask>", user->nick.c_str(), param.c_str(), extbanchar);
			return false;
		}

		std::string targetname(param, 2, p - 2);
		if (!ServerInstance->IsChannel(targetname.c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :Invalid channel name in redirection (%s)", user->nick.c_str(), chan->name.c_str(), targetname.c_str());
			return false;
		}

		Channel* const targetchan = ServerInstance->FindChan(targetname);
		if (!targetchan)
		{
			user->WriteNumeric(690, "%s :Target channel %s must exist to be set as a redirect.", user->nick.c_str(), targetname.c_str());
			return false;
		}

		if (targetchan == chan)
		{
			user->WriteNumeric(690, "%s %s :You cannot set a ban redirection to the channel the ban is on", user->nick.c_str(), targetname.c_str());
			return false;
		}

		if (adding && targetchan->GetPrefixValue(user) < OP_VALUE)
		{
			user->WriteNumeric(690, "%s :You must be opped on %s to set it as a redirect.", user->nick.c_str(), targetname.c_str());
			return false;
		}

		return true;
	}
};

class ModuleExtBanRedirect : public Module
{
	BanWatcher banwatcher;
	bool active;

 public:
	ModuleExtBanRedirect()
		: banwatcher(this)
		, active(false)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modes->AddModeWatcher(&banwatcher);
		Implementation list[] = { I_OnRehash, I_On005Numeric, I_OnCheckBan };
		ServerInstance->Modules->Attach(list, this, sizeof(list)/sizeof(Implementation));
	}

	~ModuleExtBanRedirect()
	{
		ServerInstance->Modes->DelModeWatcher(&banwatcher);
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("extbanredirect");
		banwatcher.extbanchar = tag->getString("char", "d").c_str()[0];
	}

	void On005Numeric(std::string& output)
	{
		ServerInstance->AddExtBanChar(banwatcher.extbanchar);
	}

	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask)
	{
		if ((active) || (!IS_LOCAL(user)))
			return MOD_RES_PASSTHRU;

		if (!banwatcher.IsExtBanRedirect(mask))
			return MOD_RES_PASSTHRU;

		std::string::size_type p = mask.find(':', 2);
		if (p == std::string::npos)
			return MOD_RES_PASSTHRU;

		if (!chan->CheckBan(user, mask.substr(p + 1)))
			return MOD_RES_PASSTHRU;

		const std::string targetname = mask.substr(2, p - 2);
		Channel* const target = ServerInstance->FindChan(targetname);
		if ((target) && (target->IsModeSet('l')))
		{
			std::string destlimit = target->GetModeParameter('l');
			if ((target->IsModeSet('L')) && (ServerInstance->Modules->Find("m_redirect.so")) && (target->GetUserCounter() >= atoi(destlimit.c_str())))
			{
				// The core will send "You're banned"
				return MOD_RES_DENY;
			}
		}

		// Ok to redirect
		// The core will send "You're banned"
		user->WriteNumeric(470, "%s %s %s :You are banned from this channel, so you are automatically transferred to the redirected channel.", user->nick.c_str(), chan->name.c_str(), targetname.c_str());
		active = true;
		Channel::JoinUser(user, targetname.c_str(), false, "", false, ServerInstance->Time());
		active = false;

		return MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Provide extended ban <extbanchar>:<chan>:<mask> to redirect users to another channel", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleExtBanRedirect)
