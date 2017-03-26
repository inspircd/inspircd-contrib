/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 genius3000 <genius3000@g3k.solutions>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
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
/* $ModDesc: Provides the FINDXLINE and RMXLINE commands which lets opers search and remove XLines */
/* $ModDepends: core 2.0 */

/* Helpop Lines for the COPER section
 * Find: '<helpop key="coper" value="Oper Commands
 * Place 'FINDXLINE RMXLINE   ' before 'FILTER OJOIN'
 * Re-space 'FILTER OJOIN' to match the current columns
 * Find: '<helpop key="filter" ...'
 * Place just above that line:
<helpop key="findxline" value="/FINDXLINE {<xline type>|*} <ban mask> [<reason>]

This command will list any XLines of the specified type or all (*)
that match the given ban mask and reason, if specified.">

<helpop key="rmxline" value="/RMXLINE {<xline type>|*} <ban mask> [<reason>]

This command will remove any XLines of the specified type or all (*)
that match the given ban mask and reason, if specified.">

 */


#include "inspircd.h"
#include "xline.h"

class CommandFindXLine : public Command
{
	void List(User* user, const std::string& banmask, const std::string& reason, const std::string& linetype, XLineLookup* xlines, unsigned int& matched, unsigned int& total)
	{
		for (LookupIter i = xlines->begin(); i != xlines->end(); ++i)
		{
			XLine* xline = i->second;
			if (InspIRCd::Match(xline->Displayable(), banmask) && InspIRCd::Match(xline->reason, reason))
			{
				std::string settime = ServerInstance->TimeString(xline->set_time);
				std::string expires;
				if (xline->duration == 0)
					expires = "doesn't expire";
				else
					expires = "duration " + ConvToStr(xline->duration) + 's';

				user->WriteServ("NOTICE %s :XLine type %s, match string \"%s\" set by %s at %s, %s (%s)", user->nick.c_str(), linetype.c_str(),
								xline->Displayable(), xline->source.c_str(), settime.c_str(), expires.c_str(), xline->reason.c_str());
				matched++;
			}
		}
		total += xlines->size();
	}

 public:
	CommandFindXLine(Module* mod)
		: Command(mod, "FINDXLINE", 2, 3)
	{
		syntax = "{<xline type>|*} <ban mask> [<reason>]";
		flags_needed = 'o';
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		unsigned int matched = 0;
		unsigned int total = 0;
		std::string reason = "*";

		if (parameters.size() > 2)
			reason = parameters[2];

		if (parameters[0] == "*")
		{
			user->WriteServ("NOTICE %s :Listing all XLines with a ban mask that matches \"%s\" and a reason that matches \"%s\"", user->nick.c_str(), parameters[1].c_str(), reason.c_str());

			std::vector<std::string> xlinetypes = ServerInstance->XLines->GetAllTypes();
			for (std::vector<std::string>::iterator i = xlinetypes.begin(); i != xlinetypes.end(); ++i)
			{
				XLineLookup* xlines = ServerInstance->XLines->GetAll(*i);
				if (xlines)
					List(user, parameters[1], reason, *i, xlines, matched, total);
			}
		}
		else
		{
			std::string linetype = parameters[0];
			std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);

			XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
			if (!xlines)
			{
				user->WriteServ("NOTICE %s :Invalid XLine type: %s", user->nick.c_str(), linetype.c_str());
				return CMD_FAILURE;
			}

			user->WriteServ("NOTICE %s :Listing all XLines of type %s with a ban mask that matches \"%s\" and a reason that matches \"%s\"", user->nick.c_str(), parameters[0].c_str(), parameters[1].c_str(), reason.c_str());
			List(user, parameters[1], reason, linetype, xlines, matched, total);
		}

		user->WriteServ("NOTICE %s :End of list, %u/%u XLines matched", user->nick.c_str(), matched, total);
		return CMD_SUCCESS;
	}
};

class CommandRmXLine : public Command
{
	void Remove(User* user, const std::string& banmask, const std::string& reason, const std::string& linetype, XLineLookup* xlines, unsigned int& matched, unsigned int& total)
	{
		/* Add to total first and use safe iteration as lines are removed here */
		total += xlines->size();
		LookupIter safei;

		for (LookupIter i = xlines->begin(); i != xlines->end(); )
		{
			safei = i;
			safei++;

			XLine* xline = i->second;
			if (InspIRCd::Match(xline->Displayable(), banmask) && InspIRCd::Match(xline->reason, reason))
			{
				std::string settime = ServerInstance->TimeString(xline->set_time);
				std::string expires;
				if (xline->duration == 0)
					expires = "doesn't expire";
				else
					expires = "duration " + ConvToStr(xline->duration) + 's';

				matched++;

				std::string displayable = xline->Displayable();
				std::string xlineReason = xline->reason;
				if (ServerInstance->XLines->DelLine(xline->Displayable(), linetype, user))
				{
					ServerInstance->SNO->WriteToSnoMask('x', "%s removed XLine of type %s for %s :%s",
						user->nick.c_str(), linetype.c_str(), displayable.c_str(), xlineReason.c_str());
				}
			}

			i = safei;
		}
	}

 public:
	CommandRmXLine(Module* mod)
		: Command(mod, "RMXLINE", 2, 3)
	{
		syntax = "{<xline type>|*} <ban mask> [<reason>]";
		flags_needed = 'o';
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		unsigned int matched = 0;
		unsigned int total = 0;
		std::string reason = "*";

		if (parameters.size() > 2)
			reason = parameters[2];

		if (parameters[0] == "*")
		{
			user->WriteServ("NOTICE %s :Removing all XLines with a ban mask that matches \"%s\" and a reason that matches \"%s\"", user->nick.c_str(), parameters[1].c_str(), reason.c_str());

			std::vector<std::string> xlinetypes = ServerInstance->XLines->GetAllTypes();
			for (std::vector<std::string>::iterator i = xlinetypes.begin(); i != xlinetypes.end(); ++i)
			{
				XLineLookup* xlines = ServerInstance->XLines->GetAll(*i);
				if (xlines)
					Remove(user, parameters[1], reason, *i, xlines, matched, total);
			}
		}
		else
		{
			std::string linetype = parameters[0];
			std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);

			XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
			if (!xlines)
			{
				user->WriteServ("NOTICE %s :Invalid XLine type: %s", user->nick.c_str(), linetype.c_str());
				return CMD_FAILURE;
			}

			user->WriteServ("NOTICE %s :Removing all XLines of type %s with a ban mask that matches \"%s\" and a reason that matches \"%s\"", user->nick.c_str(), parameters[0].c_str(), parameters[1].c_str(), reason.c_str());
			Remove(user, parameters[1], reason, linetype, xlines, matched, total);
		}

		user->WriteServ("NOTICE %s :End of list, %u/%u XLines removed", user->nick.c_str(), matched, total);
		return CMD_SUCCESS;
	}
};

class ModuleFindXLine : public Module
{
	CommandFindXLine findxline;
	CommandRmXLine rmxline;
 public:
	ModuleFindXLine()
		: findxline(this), rmxline(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(findxline);
		ServerInstance->Modules->AddService(rmxline);
	}

	Version GetVersion()
	{
		return Version("Provides the FINDXLINE and RMXLINE commands which lets opers search and remove XLines");
	}
};

MODULE_INIT(ModuleFindXLine)
