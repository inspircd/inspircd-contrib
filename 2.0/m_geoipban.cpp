/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Filippo Cortigiani <simos@simosnap.org>
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


#include "inspircd.h"
#include <GeoIP.h>

#ifdef _WIN32
# pragma comment(lib, "GeoIP.lib")
#endif

/* $ModAuthor: Filippo Cortigiani */
/* $ModAuthorMail: simos@simosnap.org */
/* $ModDesc: Implements extban +b G: - GeoIP Country Code bans and add county name and country code in whois */
/* $ModDepends: core 2.0 */
/* $LinkerFlags: -lGeoIP */

enum
{
	// InspIRCd-specific.
	RPL_WHOISCOUNTRY = 344
};

class ModuleGeoIPBan : public Module
{
	LocalStringExt ext;
	GeoIP* gi;

	std::string* SetExt(User* user)
	{
		const char* c = GeoIP_country_code_by_addr(gi, user->GetIPString());
		if (!c)
			c = "UNK";

		std::string* cc = new std::string(c);
		ext.set(user, cc);
		return cc;
	}

 public:
	ModuleGeoIPBan() : ext("geoipban_cc", this), gi(NULL)
	{
	}

	void init()
	{
		gi = GeoIP_new(GEOIP_STANDARD);
		if (gi == NULL)
			throw ModuleException("Unable to initialize geoip, are you missing GeoIP.dat?");
		ServerInstance->Modules->AddService(ext);
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric, I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Extban 'G' - GeoIP Country Code ban", VF_OPTCOMMON);
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask)
	{
		if ((mask.length() > 2) && (mask[0] == 'G') && (mask[1] == ':'))
		{
			std::string* cc = ext.get(user);
			if (!cc)
				cc = SetExt(user);

			if (InspIRCd::Match(*cc, mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::string& output)
	{
		ServerInstance->AddExtBanChar('G');
	}

	void OnWhois(User* src, User* dst)
	{
		std::string* cc = ext.get(dst);
		if (!cc)
			cc = SetExt(dst);

		const char* d = GeoIP_country_name_by_addr(gi, dst->GetIPString());
		if (!d)
			d = "UNKNOWN";

		ServerInstance->SendWhoisLine(src, dst, RPL_WHOISCOUNTRY, src->nick+" "+dst->nick+" :is connected from "+d +" ("+*cc+")");
	}
};

MODULE_INIT(ModuleGeoIPBan)
