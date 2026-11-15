/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Chris Novakovic <chris@chrisn.me.uk>
 *
 * This file contains a third-party module for InspIRCd. You can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: Chris Novakovic <chris@chrisn.me.uk>
/// $ModDesc: Adds the TLS ciphersuite negotiated by the user to /WHOIS responses. Visible only to server operators with the users/auspex privilege and users performing a /WHOIS on themselves.
/// $ModDepends: core 4
/// $ModConfig: <whoiscipher showoper="yes" showuser="yes">

#include "inspircd.h"
#include "modules/ssl.h"
#include "modules/whois.h"

class ModuleWhoisCipher final
	: public Module
	, public Whois::EventListener
{
private:
	bool showoper;
	bool showuser;

public:
	ModuleWhoisCipher()
		: Module(VF_OPTCOMMON, "Adds the TLS ciphersuite negotiated by the user to WHOIS responses. Visible only to server operators with the users/auspex privilege and users performing a WHOIS on themselves.")
		, Whois::EventListener(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("whoiscipher");
		showoper = tag->getBool("showoper", true);
		showuser = tag->getBool("showuser", true);
	}

	void OnWhois(Whois::Context& whois) override
	{
		User* source = whois.GetSource();
		User* target = whois.GetTarget();

		if ((showoper && source->HasPrivPermission("users/auspex")) ||
			(showuser && whois.IsSelfWhois()))
		{
			// The TLS ciphersuite is only known for users connected locally.
			LocalUser* ltarget = IS_LOCAL(target);
			if (!ltarget)
				return;

			const auto* ssliohook = SSLIOHook::IsSSL(&ltarget->eh);
			if (!ssliohook)
				return;

			std::string cipher;
			ssliohook->GetCiphersuite(cipher);
			if (!cipher.empty())
				whois.SendLine(RPL_WHOISSPECIAL, "is using TLS ciphersuite " + cipher);
		}
	}
};

MODULE_INIT(ModuleWhoisCipher)
