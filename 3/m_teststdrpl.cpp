/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015-2016 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Adds the STDRPL command for testing client standard reply implementations.


#include "inspircd.h"
#include "modules/ircv3_replies.h"

class CommandStdRpl CXX11_FINAL
	: public SplitCommand
{
 private:
	IRCv3::Replies::Fail failrpl;
	IRCv3::Replies::Warn warnrpl;
	IRCv3::Replies::Note noterpl;
	IRCv3::Replies::CapReference stdrplcap;

 public:
	CommandStdRpl(Module* Creator)
		: SplitCommand(Creator, "STDRPL")
		, failrpl(Creator)
		, warnrpl(Creator)
		, noterpl(Creator)
		, stdrplcap(Creator)
	{
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		failrpl.Send(user, NULL, "EXAMPLE", "FAIL with no command name.");
		warnrpl.Send(user, NULL, "EXAMPLE", "WARN with no command name.");
		noterpl.Send(user, NULL, "EXAMPLE", "NOTE with a command name.");

		failrpl.Send(user, this, "EXAMPLE", "FAIL with a command name.");
		warnrpl.Send(user, this, "EXAMPLE", "FAIL with a command name.");
		noterpl.Send(user, this, "EXAMPLE", "NOTE with a command name.");

		failrpl.Send(user, this, "EXAMPLE", 123, "FAIL with variable parameters.");
		warnrpl.Send(user, this, "EXAMPLE", 123, "WARN with variable parameters.");
		noterpl.Send(user, this, "EXAMPLE", 123, "NOTE with variable parameters.");

		failrpl.SendIfCap(user, stdrplcap, this, "EXAMPLE", 123, "FAIL with standard-replies cap.");
		warnrpl.SendIfCap(user, stdrplcap, this, "EXAMPLE", 123, "WARN with standard-replies cap.");
		noterpl.SendIfCap(user, stdrplcap, this, "EXAMPLE", 123, "NOTE with standard-replies cap.");

		return CMD_SUCCESS;
	}
};

class ModuleStdRpl CXX11_FINAL
	: public Module
{
 private:
	CommandStdRpl cmd;

 public:
	ModuleStdRpl()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds the STDRPL command for testing client standard reply implementations.");
	}
};

MODULE_INIT(ModuleStdRpl)
