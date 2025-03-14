/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007, 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
/// $ModDesc: Adds the /CLONES command which allows server operators to view IP addresses from which there are more than a specified number of connections.
/// $ModLink: https://docs.inspircd.org/4/moved-modules/#clones


#include "inspircd.h"
#include "clientprotocolmsg.h"
#include "modules/ircv3_batch.h"

enum
{
	// InspIRCd-specific.
	RPL_CLONES = 399
};

class CommandClones : public SplitCommand
{
 private:
	IRCv3::Batch::API batchmanager;
	IRCv3::Batch::Batch batch;

 public:
	CommandClones(Module* Creator)
		: SplitCommand(Creator,"CLONES", 1)
		, batchmanager(Creator)
		, batch("inspircd.org/clones")
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<limit>" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		auto limit = ConvToNum<unsigned int>(parameters[0]);

		// Syntax of a CLONES reply:
		// :irc.example.com BATCH +<id> inspircd.org/clones :<min-count>
		// @batch=<id> :irc.example.com 399 <client> <local-count> <remote-count> <cidr-mask>
		/// :irc.example.com BATCH :-<id>

		if (batchmanager)
		{
			batchmanager->Start(batch);
			batch.GetBatchStartMessage().PushParam(limit);
		}

		for (const auto& [range, counts] : ServerInstance->Users.GetCloneMap())
		{
			if (counts.global < limit)
				continue;

			Numeric::Numeric numeric(RPL_CLONES);
			numeric.push(counts.local);
			numeric.push(counts.global);
			numeric.push(range.str());

			ClientProtocol::Messages::Numeric numericmsg(numeric, user);
			batch.AddToBatch(numericmsg);
			user->Send(ServerInstance->GetRFCEvents().numeric, numericmsg);
		}

		if (batchmanager)
			batchmanager->End(batch);

		return CmdResult::SUCCESS;
	}
};

class ModuleClones : public Module
{
 private:
	CommandClones cmd;

 public:
	ModuleClones()
		: Module(VF_NONE, "Adds the /CLONES command which allows server operators to view IP addresses from which there are more than a specified number of connections.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleClones)
