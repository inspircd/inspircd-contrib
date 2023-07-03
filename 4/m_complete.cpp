/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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
/// $ModConfig: <complete maxsuggestions="10" minlength="3">
/// $ModDepends: core 4
/// $ModDesc: Allows clients to automatically complete commands.


#include "inspircd.h"
#include "modules/ircv3_replies.h"

class CommandComplete final
	: public SplitCommand
{
private:
	Cap::Reference cap;
	ClientProtocol::EventProvider evprov;
	IRCv3::Replies::Fail failrpl;

public:
	size_t maxsuggestions;
	size_t minlength;

	CommandComplete(Module* Creator)
		: SplitCommand(Creator, "COMPLETE", 1)
		, cap(Creator, "labeled-response")
		, evprov(Creator, "COMPLETE")
		, failrpl(Creator)
	{
		penalty = 3;
		syntax = { "<partial-command> [<max>]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (!cap.IsEnabled(user))
			return CmdResult::FAILURE;

		if (parameters[0].length() < minlength)
		{
			failrpl.Send(user, this, "NEED_MORE_CHARS", parameters[0], minlength, "You must specify more characters to complete.");
			return CmdResult::FAILURE;
		}

		size_t max = SIZE_MAX;
		if (parameters.size() > 1)
		{
			max = ConvToNum<size_t>(parameters[1]);
			if (!max || max > maxsuggestions)
			{
				failrpl.Send(user, this, "INVALID_MAX_SUGGESTIONS", parameters[1], maxsuggestions, "The number of suggestions you requested was invalid.");
				return CmdResult::FAILURE;
			}
		}

		size_t maxsent = 0;
		for (const auto& [_, command] : ServerInstance->Parser.GetCommands())
		{
			if (!irc::find(command->name, parameters[0]))
			{
				for (const auto& syntaxline : command->syntax)
				{
					ClientProtocol::Message msg("COMPLETE");
					msg.PushParamRef(command->name);
					msg.PushParamRef(syntaxline);
					ClientProtocol::Event ev(evprov, msg);
					user->Send(ev);
					maxsent++;
				}
			}

			if (maxsent > maxsuggestions)
				break;
		}
		return CmdResult::SUCCESS;
	}
};

class ModuleComplete final
	: public Module
{
private:
	CommandComplete cmd;

public:
	ModuleComplete()
		: Module(VF_NONE, "Allows clients to automatically complete commands.")
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("complete");
		cmd.maxsuggestions = tag->getNum<size_t>("maxsuggestions", 10, 1);
		cmd.minlength = tag->getNum<size_t>("minlength", 3, 1);
	}
};

MODULE_INIT(ModuleComplete)
