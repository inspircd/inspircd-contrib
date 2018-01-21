/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018 genius3000 <genius3000@g3k.solutions>
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
/* $ModDesc: Adds commands /checkbans, /testban, and /whyban */
/* $ModDepends: core 2.0 */

/* Helpop Lines for the CUSER section
 * Find: '<helpop key="cuser" value="User Commands
 * Place 'TESTBAN  WHYBAN    CHECKBANS' after 'SSLINFO'
 * Re-space as needed to match the current columns
 * Find: '<helpop key="sslinfo" ...'
 * Place just above that line:
<helpop key="testban" value="/TESTBAN <channel> <mask>

Test a channel ban mask against the users currently in the channel.">

<helpop key="whyban" value="/WHYBAN <channel> [user]

Get a list of bans and exceptions that match you (or the given user)
on the specified channel.">

<helpop key="checkbans" value="/CHECKBANS <channel>

Get a list of bans and exceptions that match current users on the channel.">

 */

#include "inspircd.h"
#include "u_listmode.h"


enum
{
	RPL_BANMATCH = 540,
	RPL_EXCEPTIONMATCH = 541,
	RPL_ENDLIST = 542
};

bool CanCheck(Channel* c, User* u)
{
	if (u->HasPrivPermission("channels/auspex"))
		return true;

	ModeHandler* mh = ServerInstance->Modes->FindMode('b', MODETYPE_CHANNEL);
	if (mh->GetLevelRequired() > c->GetPrefixValue(u))
	{
		u->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You do not have access to modify the ban list.",
			u->nick.c_str(), c->name.c_str());
		return false;
	}

	return true;
}

ListModeBase* CheckExceptionMode()
{
	ModeHandler* mh = ServerInstance->Modes->FindMode('e', MODETYPE_CHANNEL);
	if (!mh || mh->name != "banexception")
		return NULL;

	return (ListModeBase*)mh;
}

void CheckLists(User* source, Channel* c, User* u, ListModeBase* ex)
{
	for (BanList::iterator ban = c->bans.begin(); ban != c->bans.end(); ++ban)
	{
		if (c->CheckBan(u, ban->data))
		{
			source->WriteNumeric(RPL_BANMATCH, "%s %s :Ban %s matches %s (set by %s on %s)",
				source->nick.c_str(), c->name.c_str(), ban->data.c_str(), u->nick.c_str(),
				ban->set_by.c_str(), ServerInstance->TimeString(ban->set_time).c_str());
		}
	}

	modelist* exceptions;
	if (!ex || !(exceptions = ex->extItem.get(c)))
		return;

	for (modelist::iterator exc = exceptions->begin(); exc != exceptions->end(); ++exc)
	{
		if (c->CheckBan(u, exc->mask))
		{
			source->WriteNumeric(RPL_EXCEPTIONMATCH, "%s %s :Exception %s matches %s (set by %s on %s)",
				source->nick.c_str(), c->name.c_str(), exc->mask.c_str(), u->nick.c_str(),
				exc->nick.c_str(), ServerInstance->TimeString(strtoul(exc->time.c_str(), NULL, 0)).c_str());
		}
	}
}

class CommandCheckBans : public Command
{
 public:
	CommandCheckBans(Module* Creator) : Command(Creator, "CHECKBANS", 1, 1)
	{
		this->syntax = "<channel>";
		this->Penalty = 6;
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
		if (!c)
		{
			user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		// Only allow checking for matching users if you have access to the ban list
		if (!CanCheck(c, user))
			return CMD_FAILURE;

		// Check for ban exceptions as mode 'e'
		ListModeBase* ex = CheckExceptionMode();

		// Loop through all users of the channel, checking for matches to bans and exceptions (if available)
		const UserMembList* users = c->GetUsers();
		for (UserMembCIter u = users->begin(); u != users->end(); u++)
			CheckLists(user, c, u->first, ex);

		user->WriteNumeric(RPL_ENDLIST, "%s %s :End of check bans list", user->nick.c_str(), c->name.c_str());
		return CMD_SUCCESS;
	}
};

class CommandTestBan : public Command
{
 public:
	CommandTestBan(Module* Creator) : Command(Creator, "TESTBAN", 2, 2)
	{
		this->syntax = "<channel> <mask>";
		this->Penalty = 6;
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
		if (!c)
		{
			user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		// Only allow testing bans if the user has access to set a ban on the channel
		if (!CanCheck(c, user))
			return CMD_FAILURE;

		unsigned int matched = 0;
		const UserMembList* users = c->GetUsers();
		for (UserMembCIter u = users->begin(); u != users->end(); u++)
		{
			if (c->CheckBan(u->first, parameters[1]))
			{
				user->WriteNumeric(RPL_BANMATCH, "%s %s :Mask %s matches %s", user->nick.c_str(), c->name.c_str(),
					parameters[1].c_str(), u->first->nick.c_str());
				matched++;
			}
		}

		if (matched > 0)
		{
			float percent = ((float)matched / (float)users->size()) * 100;
			user->WriteNumeric(RPL_BANMATCH, "%s %s :Mask %s matched %d of %lu users (%.2f%%).", user->nick.c_str(),
						c->name.c_str(), parameters[1].c_str(), matched, users->size(), percent);
		}

		user->WriteNumeric(RPL_ENDLIST, "%s %s %s :End of test ban list", user->nick.c_str(), c->name.c_str(), parameters[1].c_str());
		return CMD_SUCCESS;
	}
};

class CommandWhyBan : public Command
{
 public:
	CommandWhyBan(Module* Creator) : Command(Creator, "WHYBAN", 1, 2)
	{
		this->syntax = "<channel> [user]";
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		Channel* c = ServerInstance->FindChan(parameters[0]);
		if (!c)
		{
			user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :No such channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		/* Allow checking yourself against channel bans with no access, but only
		 * allow checking others if you have access to the channel ban list.
		 */
		User* u = parameters.size() == 1 ? user : NULL;
		if (!u)
		{
			// Use a penalty of 10 when checking other users
			LocalUser* lu = IS_LOCAL(user);
			if (lu)
				lu->CommandFloodPenalty += 10000;

			if (!CanCheck(c, user))
				return CMD_FAILURE;

			u = ServerInstance->FindNick(parameters[1]);
			if (!u)
			{
				user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick", user->nick.c_str(), parameters[1].c_str());
				return CMD_FAILURE;
			}
		}

		// Check for ban exceptions as mode 'e'
		ListModeBase* ex = CheckExceptionMode();

		// Check for matching bans and exceptions (if available)
		CheckLists(user, c, u, ex);

		user->WriteNumeric(RPL_ENDLIST, "%s %s %s :End of why ban list", user->nick.c_str(), c->name.c_str(), u->nick.c_str());
		return CMD_SUCCESS;
	}
};

class ModuleCheckBans : public Module
{
	CommandCheckBans ccb;
	CommandTestBan ctb;
	CommandWhyBan cwb;

 public:
	ModuleCheckBans()
		: ccb(this), ctb(this), cwb(this)
	{
	}

	void init()
	{
		ServiceProvider* providerlist[] = { &ccb, &ctb, &cwb };
		ServerInstance->Modules->AddServices(providerlist, sizeof(providerlist)/sizeof(ServiceProvider*));
	}

	virtual Version GetVersion()
	{
		return Version("Gives /checkbans, /testban, and /whyban - channel ban helper commands.");
	}
};

MODULE_INIT(ModuleCheckBans)
