/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
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
/// $ModConfig: <eventexec event="startup|shutdown|rehash|link|unlink" command="command to execute goes here">
/// $ModDesc: Executes commands when a specified event occurs.
/// $ModDepends: core 4


#include "inspircd.h"
#include "modules/server.h"
#include "stringutils.h"
#include "utility/string.h"

enum EventType
{
	ET_STARTUP,
	ET_SHUTDOWN,
	ET_REHASH,
	ET_SERVER_LINK,
	ET_SERVER_UNLINK
};

class CommandThread final
	: public Thread
{
private:
	std::vector<std::string> commands;

public:
	CommandThread(const std::vector<std::string>& cmds)
		: commands(cmds)
	{
	}

	void OnStart() override
	{
		for (const auto& command : commands)
			system(command.c_str());

		Stop();
		delete this;
	}
};


class ModuleEventExec final
	: public Module
	, public ServerProtocol::LinkEventListener
{
private:
	// Maps event types to the associated commands.
	typedef insp::flat_multimap<EventType, std::string> EventMap;

	// Maps template vars to the associated values.
	typedef insp::flat_map<std::string, std::string> TemplateMap;

	// The events which are currently registered.
	EventMap events;

	// Executes the events 
	void ExecuteEvents(EventType type, const TemplateMap& map)
	{
		std::vector<std::string> commands;
		for (const auto& [_, command] : insp::equal_range(events, type))
		{
			ServerInstance->Logs.Debug(MODNAME, "Formatting command: {}", command);
			const std::string fcommand = Template::Replace(command, map);

			ServerInstance->Logs.Debug(MODNAME, "Scheduling command for execution: {}", command);
			commands.push_back(command);
		}

		// The thread will delete itself when done.
		auto* thread = new CommandThread(commands);
		thread->Start();
	}

public:
	ModuleEventExec()
		: Module(VF_NONE, "Executes commands when a specified event occurs")
		, ServerProtocol::LinkEventListener(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		EventMap newevents;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("eventexec"))
		{
			// Ensure that we have the <eventexec:event> field.
			const std::string eventstr = tag->getString("event");
			if (eventstr.empty())
				throw ModuleException(this, "<eventexec:event> is a required field, at " + tag->source.str());

			// Ensure that the <eventexec:event> value is well formed.
			EventType event;
			if (insp::equalsci(eventstr, "startup"))
				event = ET_STARTUP;
			else if (insp::equalsci(eventstr, "shutdown"))
				event = ET_SHUTDOWN;
			else if (insp::equalsci(eventstr, "rehash"))
				event = ET_REHASH;
			else if (insp::equalsci(eventstr, "link"))
				event = ET_SERVER_LINK;
			else if (insp::equalsci(eventstr, "unlink"))
				event = ET_SERVER_UNLINK;
			else
				throw ModuleException(this, "<eventexec:event> contains an unrecognised event '" + eventstr + "', at " + tag->source.str());

			// Ensure that we have the <eventexec:command> parameter.
			const std::string command = tag->getString("command");
			if (command.empty())
				throw ModuleException(this, "<eventexec:command> is a required field, at " + tag->source.str());

			newevents.insert(std::make_pair(event, command));
		}
		std::swap(newevents, events);

		if (status.initial)
		{
			ExecuteEvents(ET_STARTUP, TemplateMap());
		}
		else
		{
			TemplateMap map;
			map["user"] = status.srcuser ? status.srcuser->GetRealMask() : ServerInstance->Config->ServerName;
			ExecuteEvents(ET_REHASH, map);
		}
	}

	void OnShutdown(const std::string& reason) override
	{
		TemplateMap map;
		map["reason"] = reason;
		ExecuteEvents(ET_SHUTDOWN, map);
	}

	void OnServerLink(const Server* server) override
	{
		TemplateMap map;
		map["id"] = server->GetId();
		map["name"] = server->GetName();
		ExecuteEvents(ET_SERVER_LINK, map);
	}

	void OnServerSplit(const Server* server, bool error) override
	{
		TemplateMap map;
		map["error"] = error ? "yes" : "no";
		map["id"] = server->GetId();
		map["name"] = server->GetName();
		ExecuteEvents(ET_SERVER_UNLINK, map);
	}
};

MODULE_INIT(ModuleEventExec)
