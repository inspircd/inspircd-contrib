/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
/* $ModDesc: Provides the FINDXLINE command which lets opers search XLines */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
#include "xline.h"

class CommandFindXLine : public Command
{
	void List(User* user, const std::string& wildcardstr, const std::string& linetype, XLineLookup* xlines, unsigned int& matched, unsigned int& total)
	{
		for (LookupIter i = xlines->begin(); i != xlines->end(); ++i)
		{
			XLine* xline = i->second;
			if (InspIRCd::Match(xline->Displayable(), wildcardstr))
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
		: Command(mod, "FINDXLINE", 2)
	{
		syntax = "{<xline type>|*} <wildcard string>";
		flags_needed = 'o';
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		unsigned int matched = 0;
		unsigned int total = 0;
		if (parameters[0] == "*")
		{
			user->WriteServ("NOTICE %s :Listing all XLines with a match string that matches the wildcard string \"%s\"", user->nick.c_str(), parameters[1].c_str());

			std::vector<std::string> xlinetypes = ServerInstance->XLines->GetAllTypes();
			for (std::vector<std::string>::iterator i = xlinetypes.begin(); i != xlinetypes.end(); ++i)
			{
				XLineLookup* xlines = ServerInstance->XLines->GetAll(*i);
				if (xlines)
					List(user, parameters[1], *i, xlines, matched, total);
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

			user->WriteServ("NOTICE %s :Listing all XLines of type %s with a match string that matches the wildcard string \"%s\"", user->nick.c_str(), parameters[0].c_str(), parameters[1].c_str());
			List(user, parameters[1], linetype, xlines, matched, total);
		}

		user->WriteServ("NOTICE %s :End of list, %u/%u matches", user->nick.c_str(), matched, total);
		return CMD_SUCCESS;
	}
};

class ModuleFindXLine : public Module
{
	CommandFindXLine cmd;
 public:
	ModuleFindXLine()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion()
	{
		return Version("Provides the FINDXLINE command which lets opers search XLines", VF_VENDOR);
	}
};

MODULE_INIT(ModuleFindXLine)
