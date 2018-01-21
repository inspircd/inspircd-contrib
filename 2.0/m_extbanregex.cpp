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
/* $ModDesc: Provides extban 'x' - Regex matching to n!u@h\sr */
/* $ModDepends: core 2.0 */
/* $ModConfig: <extbanregex engine="pcre"> */

/* Helpop Lines for the EXTBANS section
 * Find: '<helpop key="extbans" value="Extended Bans'
 * Place after the 's:<server>' line:
 x:<pattern>   Matches users to a regex pattern (requires a
               regex module and extbanregex extras-module).
 */

#include "inspircd.h"
#include "m_regex.h"
#include "u_listmode.h"


enum
{
	ERR_NOENGINE = 543,
	ERR_INVALIDMASK = 544
};

bool IsExtBanRegex(const std::string& mask)
{
	return ((mask.length() > 2) && (mask[0] == 'x') && (mask[1] == ':'));
}

bool ModeCheck(User* user, std::string& param, bool adding, ModeType modetype, dynamic_reference<RegexFactory>& rxfactory)
{
		if (!adding || modetype != MODETYPE_CHANNEL)
			return true;

		if (!IS_LOCAL(user) || !IsExtBanRegex(param))
			return true;

		if (!rxfactory)
		{
			user->WriteNumeric(ERR_NOENGINE, "%s :Regex engine is missing, cannot set a regex extban.", user->nick.c_str());
			return false;
		}

		// Ensure mask is at least "!@", beyond that is up to the user
		std::string mask = param.substr(2);
		std::string::size_type plink = mask.find('!');
		if (plink == std::string::npos || mask.find('@', plink) == std::string::npos)
		{
			user->WriteNumeric(ERR_INVALIDMASK, "%s :Regex extban mask must be n!u@h\\sr", user->nick.c_str());
			return false;
		}

		Regex* regex;
		try
		{
			regex = rxfactory->Create(mask);
			delete regex;
		}
		catch (ModuleException& e)
		{
			user->WriteNumeric(ERR_INVALIDMASK, "%s :Regex extban mask is invalid (%s)", user->nick.c_str(), e.GetReason());
			return false;
		}

		return true;
}

void ModeLine(const std::string& chan, char modechar, const std::vector<std::string>& masks)
{
	std::vector<std::string> modestr;
	modestr.push_back(chan);
	modestr.push_back("-");

	// Total length of message "MODE <chan> -" so far
	unsigned int strlength = 7 + chan.length();

	for (unsigned int i = 0; i < masks.size(); ++i)
	{
		// Send the mode line and start over before it gets truncated
		if (strlength > 450 || modestr[1].length() > 50)
		{
			ServerInstance->SendMode(modestr, ServerInstance->FakeClient);
			modestr.resize(1);
			modestr.push_back("-");
			strlength = 7 + chan.length();
		}
		modestr[1].push_back(modechar);
		modestr.push_back(masks[i]);
		strlength += 2 + masks[i].length();
	}

	ServerInstance->SendMode(modestr, ServerInstance->FakeClient);
}

void RemoveAll(const std::string& engine, bool runexceptions, bool runinvex)
{
	ListModeBase* banexception = NULL;
	ListModeBase* invex = NULL;

	// We know these modes are proper if their ModeWatcher is active
	if (runexceptions)
		banexception = (ListModeBase*)ServerInstance->Modes->FindMode('e', MODETYPE_CHANNEL);
	if (runinvex)
		invex = (ListModeBase*)ServerInstance->Modes->FindMode('I', MODETYPE_CHANNEL);

	/* Loop each channel checking for any regex extbans
	 * Create a list to remove and batch the mode removal
	 * Send a notice to hop/op if anything was removed
	 */
	const chan_hash& chans = *ServerInstance->chanlist;
	for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i)
	{
		bool removed = false;
		Channel* chan = i->second;

		std::vector<std::string> banmasks;
		for (BanList::iterator ban = chan->bans.begin(); ban != chan->bans.end(); ++ban)
		{
			if (IsExtBanRegex(ban->data))
				banmasks.push_back(ban->data);
		}
		if (!banmasks.empty())
		{
			ModeLine(chan->name, 'b', banmasks);
			removed = true;
		}

		modelist* banexceptions = banexception ? banexception->extItem.get(chan) : NULL;
		if (banexceptions)
		{
			std::vector<std::string> excmasks;
			for (modelist::iterator exc = banexceptions->begin(); exc != banexceptions->end(); ++exc)
			{
				if (IsExtBanRegex(exc->mask))
					excmasks.push_back(exc->mask);
			}
			if (!excmasks.empty())
			{
				ModeLine(chan->name, 'e', excmasks);
				removed = true;
			}
		}

		modelist* inviteexceptions = invex ? invex->extItem.get(chan) : NULL;
		if (inviteexceptions)
		{
			std::vector<std::string> invmasks;
			for (modelist::iterator inv = inviteexceptions->begin(); inv != inviteexceptions->end(); ++inv)
			{
				if (IsExtBanRegex(inv->mask))
					invmasks.push_back(inv->mask);
			}
			if (!invmasks.empty())
			{
				ModeLine(chan->name, 'I', invmasks);
				removed = true;
			}
		}

		if (!removed)
			continue;

		CUList empty;
		std::string notice = "Regex engine has changed to '" + engine + "'. All regex extbans have been removed";
		ModeHandler* hop = ServerInstance->Modes->FindMode('h', MODETYPE_CHANNEL);
		char pfxchar = ((hop) && (hop->name == "halfop")) ? hop->GetPrefix() : '@';

		chan->WriteAllExcept(ServerInstance->FakeClient, true, pfxchar, empty, "NOTICE %s :%s", chan->name.c_str(), notice.c_str());
	}
}

class BanWatcher : public ModeWatcher
{
	dynamic_reference<RegexFactory>& rxfactory;

 public:
	BanWatcher(Module* mod, dynamic_reference<RegexFactory>& rf)
		: ModeWatcher(mod, 'b', MODETYPE_CHANNEL)
		, rxfactory(rf)
	{
	}

	bool BeforeMode(User* user, User*, Channel* chan, std::string& param, bool adding, ModeType modetype)
	{
		return ModeCheck(user, param, adding, modetype, rxfactory);
	}
};

class ExceptionWatcher : public ModeWatcher
{
	dynamic_reference<RegexFactory>& rxfactory;

 public:
	ExceptionWatcher(Module* mod, dynamic_reference<RegexFactory>& rf)
		: ModeWatcher(mod, 'e', MODETYPE_CHANNEL)
		, rxfactory(rf)
	{
	}

	bool BeforeMode(User* user, User*, Channel* chan, std::string& param, bool adding, ModeType modetype)
	{
		return ModeCheck(user, param, adding, modetype, rxfactory);
	}
};

class InviteExceptionWatcher : public ModeWatcher
{
	dynamic_reference<RegexFactory>& rxfactory;

 public:
	InviteExceptionWatcher(Module* mod, dynamic_reference<RegexFactory>& rf)
		: ModeWatcher(mod, 'I', MODETYPE_CHANNEL)
		, rxfactory(rf)
	{
	}

	bool BeforeMode(User* user, User*, Channel* chan, std::string& param, bool adding, ModeType modetype)
	{
		return ModeCheck(user, param, adding, modetype, rxfactory);
	}
};

class ModuleExtBanRegex : public Module
{
	BanWatcher banwatcher;
	ExceptionWatcher exceptionwatcher;
	InviteExceptionWatcher inviteexceptionwatcher;
	bool ewactive;
	bool iewactive;
	bool initing;

	dynamic_reference<RegexFactory> rxfactory;
	RegexFactory* factory;

 public:
	ModuleExtBanRegex()
		: banwatcher(this, rxfactory)
		, exceptionwatcher(this, rxfactory)
		, inviteexceptionwatcher(this, rxfactory)
		, ewactive(false)
		, iewactive(false)
		, initing(true)
		, rxfactory(this, "regex")
	{
	}

	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_On005Numeric, I_OnCheckBan, I_OnLoadModule, I_OnRehash, I_OnUnloadModule };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		ServerInstance->Modes->AddModeWatcher(&banwatcher);
		if (ServerInstance->Modules->Find("m_banexception.so"))
			ewactive = ServerInstance->Modes->AddModeWatcher(&exceptionwatcher);
		if (ServerInstance->Modules->Find("m_inviteexception.so"))
			iewactive = ServerInstance->Modes->AddModeWatcher(&inviteexceptionwatcher);

	}

	~ModuleExtBanRegex()
	{
		ServerInstance->Modes->DelModeWatcher(&banwatcher);
		if (ewactive)
			ServerInstance->Modes->DelModeWatcher(&exceptionwatcher);
		if (iewactive)
			ServerInstance->Modes->DelModeWatcher(&inviteexceptionwatcher);
	}

	void OnLoadModule(Module* mod)
	{
		if (!ewactive && mod->ModuleSourceFile == "m_banexception.so")
			ewactive = ServerInstance->Modes->AddModeWatcher(&exceptionwatcher);
		if (!iewactive && mod->ModuleSourceFile == "m_inviteexception.so")
			iewactive = ServerInstance->Modes->AddModeWatcher(&inviteexceptionwatcher);
	}

	void OnUnloadModule(Module* mod)
	{
		if (ewactive && mod->ModuleSourceFile == "m_banexception.so" && ServerInstance->Modes->DelModeWatcher(&exceptionwatcher))
			ewactive = false;
		if (iewactive && mod->ModuleSourceFile == "m_inviteexception.so" && ServerInstance->Modes->DelModeWatcher(&inviteexceptionwatcher))
			iewactive = false;
	}

	void OnRehash(User* user)
	{
		std::string newrxengine = ServerInstance->Config->ConfValue("extbanregex")->getString("engine");
		factory = rxfactory ? rxfactory.operator->() : NULL;

		if (newrxengine.empty())
			rxfactory.SetProvider("regex");
		else
			rxfactory.SetProvider("regex/" + newrxengine);

		if (!rxfactory)
		{
			if (newrxengine.empty())
				ServerInstance->SNO->WriteToSnoMask('a', "WARNING: No regex engine loaded - regex extban functionality disabled until this is corrected.");
			else
				ServerInstance->SNO->WriteToSnoMask('a', "WARNING: Regex engine '%s' is not loaded - regex extban functionality disabled until this is corrected.", newrxengine.c_str());

			RemoveAll("none", ewactive, iewactive);
		}
		else if (!initing && rxfactory.operator->() != factory)
		{
			ServerInstance->SNO->WriteToSnoMask('a', "Regex engine has changed to '%s', removing all regex extbans.", rxfactory->name.c_str());
			RemoveAll(rxfactory->name, ewactive, iewactive);
		}

		initing = false;
	}

	ModResult OnCheckBan(User* user, Channel* c, const std::string& mask)
	{
		if (!factory)
			return MOD_RES_PASSTHRU;

		if (!IsExtBanRegex(mask))
			return MOD_RES_PASSTHRU;

		std::string dhost = user->nick + "!" + user->ident + "@" + user->dhost + " " + user->fullname;
		std::string host = user->nick + "!" + user->ident + "@" + user->host + " " + user->fullname;
		std::string ip = user->nick + "!" + user->ident + "@" + user->GetIPString() + " " + user->fullname;

		Regex* regex = factory->Create(mask.substr(2));
		bool matched = (regex->Matches(dhost) || regex->Matches(host) || regex->Matches(ip));
		delete regex;

		return (matched ? MOD_RES_DENY : MOD_RES_PASSTHRU);
	}

	void On005Numeric(std::string& output)
	{
		ServerInstance->AddExtBanChar('x');
	}

	Version GetVersion()
	{
		return Version("Extban 'x' - regex matching to n!u@h\\sr", VF_OPTCOMMON, rxfactory ? rxfactory->name : "");
	}
};

MODULE_INIT(ModuleExtBanRegex)
