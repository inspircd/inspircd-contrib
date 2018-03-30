/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Colgate Minuette <colgate.inspircd@canternet.org>
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

/* $ModDesc: Implements extban W: - CGI:IRC bans */
/* $ModAuthor: Colgate Minuette */
/* $ModAuthorMail: colgate.inspircd@canternet.org */
/* $ModDepends: core 2.0 */

class ModuleCGIircBan : public Module
{
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric, I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion()
	{
		return Version("Extban 'W' - CGI:IRC extban", VF_OPTCOMMON);
	}

	ModResult OnCheckBan(User* user, Channel* c, const std::string& mask)
	{
		if ((mask.length() > 2) && (mask[0] == 'W') && (mask[1] == ':'))
		{
			std::string webirc_host;

			if (!ReadCGIIRCExt("cgiirc_realhost", user, webirc_host))
				ReadCGIIRCExt("cgiirc_realip", user, webirc_host);

			if(webirc_host.empty())
				return MOD_RES_PASSTHRU;

			if (InspIRCd::MatchCIDR(webirc_host, mask.substr(2)))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::string& output)
	{
		ServerInstance->AddExtBanChar('W');
	}

	void OnWhois(User* source, User* dest)
	{
		std::string webirc_host;
		if (!ReadCGIIRCExt("cgiirc_realhost", dest, webirc_host))
			ReadCGIIRCExt("cgiirc_realip", dest, webirc_host);
		if (!webirc_host.empty())
			ServerInstance->SendWhoisLine(source, dest, 672, "%s %s :is actually from %s", source->nick.c_str(), dest->nick.c_str(), webirc_host.c_str());
	}

	static bool ReadCGIIRCExt(const char* extname, User* user, std::string& out)
	{
		ExtensionItem* wiext = ServerInstance->Extensions.GetItem(extname);
		if (!wiext)
			return false;

		if (wiext->creator->ModuleSourceFile != "m_cgiirc.so")
			return false;

		StringExtItem* stringext = static_cast<StringExtItem*>(wiext);
		std::string* addr = stringext->get(user);
		if (!addr)
			return false;
		out = *addr;
		return true;
	}
};

MODULE_INIT(ModuleCGIircBan)

