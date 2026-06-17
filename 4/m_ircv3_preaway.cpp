/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 reverse <mike.chevronnet@gmail.com>
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

/// $ModAuthor: reverse <mike.chevronnet@gmail.com>
/// $ModConfig: <preaway substitute="Away">
/// $ModDepends: core 4
/// $ModDesc: Provides the DRAFT draft/pre-away IRCv3 client capability.


#include "inspircd.h"
#include "modules/away.h"
#include "modules/cap.h"

class ModuleIRCv3PreAway final
	: public Module
	, public Away::EventListener
{
private:
	Cap::Capability cap;
	std::string substitute;

public:
	ModuleIRCv3PreAway()
		: Module(VF_VENDOR, "Provides the DRAFT draft/pre-away IRCv3 client capability.")
		, Away::EventListener(this)
		, cap(this, "draft/pre-away")
	{
		// CommandAway already sets works_before_reg, so a pre-away client can send
		// AWAY mid-registration as-is; advertising the cap is most of the job. The
		// one extra bit of behaviour the spec asks for is the "*" handling below.
	}

	void ReadConfig(ConfigStatus&) override
	{
		// What to relay in place of a bare "*" away message. Leaving this empty keeps
		// the literal "*" on the wire (the spec only says servers MAY substitute).
		substitute = ServerInstance->Config->ConfValue("preaway")->getString("substitute", "Away");
	}

	ModResult OnUserPreAway(LocalUser*, std::string& message) override
	{
		// draft/pre-away gives "*" a special meaning: the user is away but isn't
		// saying why. The spec lets the server swap in a human-readable string before
		// that gets relayed to other clients, which is what we do here.
		//
		// The spec note about "*" not overriding an away message set by another of the
		// user's connections doesn't apply to us: InspIRCd tracks away state per
		// connection, so there's no shared session to clobber.
		if (!substitute.empty() && message == "*")
			message = substitute;
		return MOD_RES_PASSTHRU;
	}

	// We only care about the pre-away rewrite; these are required but unused.
	void OnUserAway(User*, const std::optional<AwayState>&) override { }
	void OnUserBack(User*, const std::optional<AwayState>&) override { }
};

MODULE_INIT(ModuleIRCv3PreAway)
