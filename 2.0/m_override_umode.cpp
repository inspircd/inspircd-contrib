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
/* $ModDesc: Adds usermode +O that must be set on opers to permit override */
/* $ModDepends: core 2.0 */
/* $ModConfig: <override umodeexpire="10m" expiremsg="Oper override mode (+O) has expired." snoonset="yes" snoonunset="yes" snoonexpire="no"> */

/* Configuration:

# If you already have an <override> tag then add these options to it,
# DO NOT add a second <override> tag.
<override

# Duration how long the override mode lasts, set to 0 to never expire.
# Defaults to 10 minutes.
umodeexpire="10m"

# When the override mode expires, send a NOTICE with the given text to the
# oper who had the mode set. If empty (the default), no NOTICE is sent.
expiremsg="Oper override mode (+O) has expired."

# If true, sends a snotice when the override mode is set on a user. Defaults to on.
snoonset="yes"

# If true, sends a snotice when the override mode is unset on a user. Defaults to off.
snoonunset="yes"

# If true, sends a snotice when the override mode set on a user expires. Defaults to off.
snoonexpire="no">

*/

#include "inspircd.h"

static unsigned int activetime; // seconds after the mode expires

struct ActiveOper
{
	std::string uuid;
	time_t expiretime;

	ActiveOper(LocalUser* user)
		: uuid(user->uuid)
		, expiretime(ServerInstance->Time() + activetime)
	{
	}
};

typedef std::vector<ActiveOper> ActiveOperList;
static ActiveOperList activeopers; // List of opers with the mode set who need expire checking
static bool snoonset;
static bool snoonunset;

class OverrideMode : public ModeHandler
{
 public:
	OverrideMode(Module* mod, unsigned char modechar)
		: ModeHandler(mod, "permitoverride", modechar, PARAM_NONE, MODETYPE_USER)
	{
		oper = true;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* chan, std::string& parameter, bool adding)
	{
		if (dest->IsModeSet(GetModeChar()) == adding)
			return MODEACTION_DENY;

		dest->SetMode(GetModeChar(), adding);

		LocalUser* const localuser = IS_LOCAL(dest);
		// Send snotices
		char snodest = localuser ? 'v' : 'V';
		if (snoonset && adding)
			ServerInstance->SNO->WriteToSnoMask(snodest, "Oper %s has turned on override", dest->nick.c_str());
		else if (!adding)
		{
			// IS_OPER check is needed to make sure we don't send snotices when the server unsets
			// the mode due to deopering
			if ((snoonunset) && (IS_OPER(dest)) && (!IS_SERVER(source)))
				ServerInstance->SNO->WriteToSnoMask(snodest, "Oper %s has turned off override", dest->nick.c_str());
		}

		// Ignore remote users, their own server handles them
		if (localuser)
		{
			if (adding)
			{
				if (activetime > 0)
					activeopers.push_back(ActiveOper(localuser));
			}
			else
			{
				// Remove this oper from the list
				for (ActiveOperList::iterator i = activeopers.begin(); i != activeopers.end(); ++i)
				{
					ActiveOper& item = *i;
					if (item.uuid == dest->uuid)
					{
						activeopers.erase(i);
						break;
					}
				}
			}
		}

		return MODEACTION_ALLOW;
	}
};

class ModuleOverrideUserMode : public Module
{
	Module* overridemod;
	unsigned char modechar;
	OverrideMode overridemode;
	std::string expiremsg;
	bool snoonexpire;

 public:
	ModuleOverrideUserMode()
		: overridemod(NULL)
		, modechar('O')
		, overridemode(this, modechar)
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnLoadModule, I_OnUnloadModule, I_OnBackgroundTimer, I_OnRehash, I_OnPreMode, I_OnUserPreJoin, I_OnUserPreKick, I_OnPreTopicChange };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ServerInstance->Modules->AddService(overridemode);
		OnRehash(NULL);
		overridemod = ServerInstance->Modules->Find("m_override.so");
	}

	~ModuleOverrideUserMode()
	{
		// If we're unloaded and m_override is loaded reattach it to all events
		Implementation eventlist[] = { I_OnPreMode, I_OnUserPreJoin, I_OnUserPreKick, I_OnPreTopicChange };
		if (overridemod)
			ServerInstance->Modules->Attach(eventlist, overridemod, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("override");
		activetime = ServerInstance->Duration(tag->getString("umodeexpire", "10m"));
		expiremsg = tag->getString("expiremsg");

		snoonexpire = tag->getBool("snoonexpire");
		snoonset = tag->getBool("snoonset", true);
		snoonunset = tag->getBool("snoonunset");
	}

	void OnLoadModule(Module* mod)
	{
		if (overridemod)
			return;

		if (mod->ModuleSourceFile != "m_override.so")
			return;

		overridemod = mod;
	}

	void OnUnloadModule(Module* mod)
	{
		if (mod == overridemod)
			overridemod = NULL;
	}

	ModResult OnPreTopicChange(User* source, Channel* chan, const std::string& topic)
	{
		if (!source->IsModeSet(modechar))
			return MOD_RES_PASSTHRU;

		if (!overridemod)
			return MOD_RES_PASSTHRU;

		return overridemod->OnPreTopicChange(source, chan, topic);
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string& reason)
	{
		if (!source->IsModeSet(modechar))
			return MOD_RES_PASSTHRU;

		if (!overridemod)
			return MOD_RES_PASSTHRU;

		return overridemod->OnUserPreKick(source, memb, reason);
	}

	ModResult OnPreMode(User* source, User* dest, Channel* chan, const std::vector<std::string>& parameters)
	{
		if (!source->IsModeSet(modechar))
			return MOD_RES_PASSTHRU;

		if (!overridemod)
			return MOD_RES_PASSTHRU;

		return overridemod->OnPreMode(source, dest, chan, parameters);
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string& privs, const std::string& keygiven)
	{
		if (!user->IsModeSet(modechar))
			return MOD_RES_PASSTHRU;

		if (!overridemod)
			return MOD_RES_PASSTHRU;

		return overridemod->OnUserPreJoin(user, chan, cname, privs, keygiven);
	}

	void OnBackgroundTimer(time_t curtime)
	{
		for (ActiveOperList::iterator i = activeopers.begin(); i != activeopers.end(); )
		{
			ActiveOper& item = *i;
			if (item.expiretime < curtime)
			{
				User* const user = ServerInstance->FindUUID(item.uuid);
				i = activeopers.erase(i);
				if ((!user) || (user->quitting))
					continue; // User has quit

				if (!expiremsg.empty())
					user->WriteServ("NOTICE %s :%s", user->nick.c_str(), expiremsg.c_str());

				if (snoonexpire)
					ServerInstance->SNO->WriteGlobalSno('v', "Override has expired on oper %s", user->nick.c_str());

				// Remove the mode
				std::vector<std::string> modeparams;
				modeparams.push_back(user->nick);
				modeparams.push_back(std::string("-") + overridemode.GetModeChar());
				ServerInstance->SendGlobalMode(modeparams, ServerInstance->FakeClient);
			}
			else
				++i;
		}
	}

	void Prioritize()
	{
		Module* mod = ServerInstance->Modules->Find("m_override.so");
		if (!mod)
			return;

		// Put ourselves before m_override in all events it uses to permit overrides
		// and detach it from those events, allowing us to filter events it handles.
		Implementation eventlist[] = { I_OnPreMode, I_OnUserPreJoin, I_OnUserPreKick, I_OnPreTopicChange };
		for (size_t i = 0; i < sizeof(eventlist)/sizeof(Implementation); i++)
		{
			ServerInstance->Modules->SetPriority(mod, eventlist[i], PRIORITY_BEFORE, mod);
			ServerInstance->Modules->Detach(eventlist[i], mod);
		}
	}

	Version GetVersion()
	{
		return Version("Adds usermode +O that must be set on opers to permit override");
	}
};

MODULE_INIT(ModuleOverrideUserMode)
