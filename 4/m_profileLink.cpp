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

/// $ModAuthor: Jean Chevronnet <mike.chevronnet@gmail.com>
/// $ModDepends: core 4
/// $ModDesc: Adds a profile link to the WHOIS response for registered users, ignoring services, bots.

#include "inspircd.h"
#include "modules/whois.h"
#include "modules/account.h"

enum
{
	// Define a custom WHOIS numeric reply for the profile link.
	RPL_WHOISPROFILE = 320,
};

class ModuleProfileLink final
	: public Module
	, public Whois::EventListener
{
private:
	Account::API accountapi;

public:
	ModuleProfileLink()
		: Module(VF_OPTCOMMON, "Adds a profile link to the WHOIS response for registered users, ignoring services, bots.")
		, Whois::EventListener(this)
		, accountapi(this)
	{
	}

	void OnWhois(Whois::Context& whois) override
	{
		User* target = whois.GetTarget();

		// Skip services, bots.
		if (target->server->IsService() || target->IsModeSet('B'))
			return;

		// Check if the user has an account name (is registered).
		const std::string* account = accountapi->GetAccountName(target);
		if (account)
		{
			// Construct the profile URL using the user's account name.
			const std::string profileUrl = "https://www.reseau-entrenous.fr/profil/" + *account;
			// Send the profile URL in the WHOIS response.
			whois.SendLine(RPL_WHOISPROFILE, target->nick, "Profil: " + profileUrl);
		}
		else
		{
			// Indicate that the account is not registered.
			whois.SendLine(RPL_WHOISPROFILE, target->nick, "Profil: L'utilisateur n'est pas connecté ou le compte n'est pas enregistré.");
		}
	}
};

MODULE_INIT(ModuleProfileLink)
