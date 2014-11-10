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
/* $ModDesc: Adds channel list mode +Z which only lets clients join who use a ciphersuite on the list */
/* $ModDepends: core 2.0 */


// Needs m_sslstats_gnutls

#include "inspircd.h"
#include "u_listmode.h"

class CiphersuiteJoinMode : public ListModeBase
{
 public:
	CiphersuiteJoinMode(Module* Creator)
		: ListModeBase(Creator, "ciphersuite", 'Z', "End of channel allowed SSL ciphersuite list", 961, 960, false, "ciphersuitelist")
	{
	}

	bool TellListTooLong(User* user, Channel* chan, std::string& param)
	{
		user->WriteNumeric(962, "%s %s %s :Channel ciphersuite whitelist is full", user->nick.c_str(), chan->name.c_str(), param.c_str());
		return true;
	}

	void TellAlreadyOnList(User* user, Channel* chan, std::string& param)
	{
		user->WriteNumeric(963, "%s %s :Ciphersuite %s is already whitelisted", user->nick.c_str(), chan->name.c_str(), param.c_str());
	}

	void TellNotSet(User* user, Channel* chan, std::string& param)
	{
		user->WriteNumeric(964, "%s %s :Ciphersuite %s is not on the whitelist, cannot remove",user->nick.c_str(), chan->name.c_str(), param.c_str());
	}

	bool ValidateParam(User* user, Channel* chan, std::string& param)
	{
		return ((param.length() <= 100) && (!param.empty()));
	}
};

class ModuleCiphersuiteJoin : public Module
{
	CiphersuiteJoinMode mode;

 public:
	ModuleCiphersuiteJoin()
		: mode(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(mode);
		mode.DoImplements(this);
		ServerInstance->Modules->Attach(I_OnUserPreJoin, this);
	}

	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string& privs, const std::string& keygiven)
	{
		if (!chan)
			return MOD_RES_PASSTHRU;

		modelist* list = mode.extItem.get(chan);
		// Empty list, no restrictions
		if (!list || list->empty())
			return MOD_RES_PASSTHRU;

		StringExtItem* ext = static_cast<StringExtItem*>(ServerInstance->Extensions.GetItem("sslciphersuite"));
		if (!ext) // Stats mod not loaded
			return MOD_RES_PASSTHRU;

		const std::string* const ciphersuite = ext->get(user);
		if (!ciphersuite)
		{
			user->WriteServ("489 %s %s :Cannot join channel; SSL users only (+Z)", user->nick.c_str(), cname);
			return MOD_RES_DENY;
		}

		for (modelist::const_iterator i = list->begin(); i != list->end(); ++i)
		{
			if (InspIRCd::Match(*ciphersuite, i->mask))
				return MOD_RES_PASSTHRU;
		}

		user->WriteServ("489 %s %s :Cannot join channel because you are not using a whitelisted ciphersuite (+Z)", user->nick.c_str(), cname);
		return MOD_RES_DENY;
	}

	void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		mode.DoSyncChannel(chan, proto, opaque);
	}

	void OnRehash(User* user)
	{
		mode.DoRehash();
	}

	Version GetVersion()
	{
		return Version("Adds channel list mode +Z which only lets clients join who use a ciphersuite on the list");
	}
};

MODULE_INIT(ModuleCiphersuiteJoin)
