/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: InspIRCd Developers
/// $ModDepends: core 4
/// $ModDesc: Provides compatibility with the legacy PROTOCTL system.


#include "inspircd.h"
#include "modules/cap.h"
#include "modules/isupport.h"

class CommandProtoctl final
	: public SplitCommand
{
 public:
	Cap::Reference namesx;
	Cap::Reference uhnames;

	CommandProtoctl(Module* Creator)
		: SplitCommand(Creator, "PROTOCTL", 1)
		, namesx(Creator, "multi-prefix")
		, uhnames(Creator, "userhost-in-names")
	{
		allow_empty_last_param = false;
		works_before_reg = true;
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		for (const auto& parameter : parameters)
		{
			if (irc::equals(parameter, "NAMESX"))
				namesx.Set(user, true);

			if (irc::equals(parameter, "UHNAMES"))
				uhnames.Set(user, true);
		}
		return CmdResult::SUCCESS;
	}
};

class ModuleProtoctl
	: public Module
	, public ISupport::EventListener
{
 private:
	CommandProtoctl cmd;

 public:
	ModuleProtoctl()
		: Module(VF_NONE, "Provides compatibility with the legacy PROTOCTL system.")
		, ISupport::EventListener(this)
		, cmd(this)
	{
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		// The legacy PROTOCTL system is a wrapper around the equivalent caps.

		if (cmd.namesx)
			tokens["NAMESX"];

		if (cmd.uhnames)
			tokens["UHNAMES"];
	}
};

MODULE_INIT(ModuleProtoctl)
