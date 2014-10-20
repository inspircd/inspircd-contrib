/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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


/* $ModAuthor: Attila Molnar */
/* $ModAuthorMail: attilamolnar@hush.com */
/* $ModDesc: Provides stats about SSL ciphersuites in use by users */
/* $ModDepends: core 2.0 */

/* $CompileFlags: pkgconfincludes("gnutls","/gnutls/gnutls.h","") */
/* $LinkerFlags: rpath("pkg-config --libs gnutls") pkgconflibs("gnutls","/libgnutls.so","-lgnutls") */
/* $NoPedantic */

#include "inspircd.h"
#include "ssl.h"
#include <gnutls/gnutls.h>

class ModuleSSLStatsGnuTLS : public Module
{
	StringExtItem ext;

	static const char* UnknownIfNULL(const char* str)
	{
		return str ? str : "UNKNOWN";
	}

	static std::string BuildString(gnutls_session_t sess)
	{
		std::string cipher = UnknownIfNULL(gnutls_protocol_get_name(gnutls_protocol_get_version(sess)));
		cipher.push_back('-');
		cipher.append(UnknownIfNULL(gnutls_kx_get_name(gnutls_kx_get(sess)))).push_back('-');
		cipher.append(UnknownIfNULL(gnutls_cipher_get_name(gnutls_cipher_get(sess)))).push_back('-');
		cipher.append(UnknownIfNULL(gnutls_mac_get_name(gnutls_mac_get(sess))));
		return cipher;
	}

 public:
	ModuleSSLStatsGnuTLS()
		: ext("sslciphersuite", this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(ext);
		Implementation eventlist[] = { I_OnStats, I_OnWhois, I_OnUserConnect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		ServerInstance->SNO->EnableSnomask('z', "CIPHERSUITE");
	}

	void OnUserConnect(LocalUser* user)
	{
		Module* const mod = user->eh.GetIOHook();
		if (!mod)
			return;

		if (mod->ModuleSourceFile != "m_ssl_gnutls.so")
			return;

		SSLRawSessionRequest req(user->eh.GetFd(), this, mod);
		if (req.data)
		{
			std::string ciphersuite = BuildString(reinterpret_cast<gnutls_session_t>(req.data));
			ext.set(user, ciphersuite);
			ServerInstance->SNO->WriteToSnoMask('z', "Connecting user %s is using ciphersuite %s", user->GetFullRealHost().c_str(), ciphersuite.c_str());
		}
	}

	void OnWhois(User* src, User* dest)
	{
		if (!IS_OPER(src))
			return;

		const std::string* const str = ext.get(dest);
		if (str)
			ServerInstance->SendWhoisLine(src, dest, 336, src->nick + " " + dest->nick + " is using SSL ciphersuite " + *str);
	}

	ModResult OnStats(char symbol, User* user, string_list& results)
	{
		if (symbol != '1')
			return MOD_RES_PASSTHRU;

		unsigned int total = 0;
		std::map<std::string, unsigned int> counts;
		LocalUserList& list = ServerInstance->Users->local_users;
		for (LocalUserList::const_iterator i = list.begin(); i != list.end(); ++i)
		{
			LocalUser* curr = *i;
			if ((curr->registered != REG_ALL) || (curr->quitting))
				continue;

			const std::string* const str = ext.get(curr);
			if (str)
			{
				total++;
				counts[*str]++;
			}
		}

		std::string line = ":" + ServerInstance->Config->ServerName + " NOTICE " + user->nick + " :";
		for (std::map<std::string, unsigned int>::const_iterator i = counts.begin(); i != counts.end(); ++i)
			user->SendText("%s%s %u", line.c_str(), i->first.c_str(), i->second);

		user->SendText("%sEnd of list - %u user(s) total", line.c_str(), total);
		return MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Provides stats about SSL ciphersuites in use by users", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleSSLStatsGnuTLS)
