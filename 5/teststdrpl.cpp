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

/// $ModAuthor: Sadie Powell <sadie@witchery.services>
/// $ModDepends: core 5
/// $ModDesc: Adds the STDRPL command for testing client standard reply implementations.


#include "inspircd.h"
#include "modules/ircv3.h"
#include "numerichelper.h"

class CommandStdRpl final
	: public SplitCommand
{
private:
	IRCv3::ReplyCapReference stdrplcap;

public:
	CommandStdRpl(const WeakModulePtr& Creator)
		: SplitCommand(Creator, "STDRPL")
		, stdrplcap(Creator)
	{
		syntax = { "[<target>]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		User* target = user;
		if (!parameters.empty())
		{
			target = ServerInstance->Users.Find(parameters[0], true);
			if (!target)
			{
				user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
				return CmdResult::FAILURE;
			}
		}

		target->WriteRemoteReply(Reply::Type::FAIL, nullptr, "EXAMPLE", "FAIL with no command name.");
		target->WriteRemoteReply(Reply::Type::WARN, nullptr, "EXAMPLE", "WARN with no command name.");
		target->WriteRemoteReply(Reply::Type::NOTE, nullptr, "EXAMPLE", "NOTE with a command name.");

		target->WriteRemoteReply(Reply::Type::FAIL, this, "EXAMPLE", "FAIL with a command name.");
		target->WriteRemoteReply(Reply::Type::WARN, this, "EXAMPLE", "FAIL with a command name.");
		target->WriteRemoteReply(Reply::Type::NOTE, this, "EXAMPLE", "NOTE with a command name.");

		target->WriteRemoteReply(Reply::Type::FAIL,this, "EXAMPLE", 123, "FAIL with variable parameters.");
		target->WriteRemoteReply(Reply::Type::WARN, this, "EXAMPLE", 123, "WARN with variable parameters.");
		target->WriteRemoteReply(Reply::Type::NOTE, this, "EXAMPLE", 123, "NOTE with variable parameters.");

		IRCv3::WriteReply(Reply::Type::FAIL, target, stdrplcap, nullptr, "EXAMPLE", 123, "FAIL with standard-replies cap.");
		IRCv3::WriteReply(Reply::Type::WARN, target, stdrplcap, nullptr, "EXAMPLE", 123, "WARN with standard-replies cap.");
		IRCv3::WriteReply(Reply::Type::NOTE, target, stdrplcap, nullptr, "EXAMPLE", 123, "NOTE with standard-replies cap.");

		IRCv3::WriteReply(Reply::Type::FAIL, target, stdrplcap, this, "EXAMPLE", 123, "FAIL with standard-replies cap and a command name.");
		IRCv3::WriteReply(Reply::Type::WARN, target, stdrplcap, this, "EXAMPLE", 123, "WARN with standard-replies cap and a command name.");
		IRCv3::WriteReply(Reply::Type::NOTE, target, stdrplcap, this, "EXAMPLE", 123, "NOTE with standard-replies cap and a command name.");

		return CmdResult::SUCCESS;
	}
};

class ModuleStdRpl final
	: public Module
{
private:
	CommandStdRpl cmd;

public:
	ModuleStdRpl()
		: Module(VF_NONE, "Adds the STDRPL command for testing client standard reply implementations.")
		, cmd(weak_from_this())
	{
	}
};

MODULE_INIT(ModuleStdRpl)
