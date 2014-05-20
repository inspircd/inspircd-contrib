/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
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

/* $ModAuthor: Daniel Vassdal */
/* $ModAuthorMail: shutter@canternet.org */
/* $ModDesc: Enables two factor authentification for oper blocks using authy */
/* $ModDepends: core 2.0 */
/* $ModConfig: <authy apikey="" ssl="" graceful="false"> */

/*
	Note:
	You can select what SSL module to chose by specifying e.g. ssl="gnutls"
	Assign an authyid to your oper's oper blocks.
	You get their ID from your Authy control panel on https://dashboard.authy.com/

	If graceful is set to true, the opers will still be allowed to log in, in case
	of failure to communicate with Authy.com
*/

#include "inspircd.h"

class ResolveAPI : public Resolver
{
 public:
	std::string& ip;

	ResolveAPI(Module* mod, std::string& ipa, bool& cached) : Resolver("api.authy.com", DNS_QUERY_A, cached, mod), ip(ipa)
	{
	}

	void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
	{
		ip = result;
	}
};

static bool auth_req_active = false;
class AuthRequest : public BufferedSocket
{
 public:
	struct AuthData
	{
		const std::string uuid;
		const std::string username;
		const std::string password;
		const std::string authy_id;
		const std::string token;
		AuthData(User* u, const std::string& unm, const std::string& pwd, const std::string& aid, const std::string& tok)
			: uuid(u->uuid), username(unm), password(pwd), authy_id(aid), token(tok)
		{
		}
	};

 private:
	bool graceful_failure;
	const std::string api_key;
	AuthData AData;
	std::string ip;

	void ForwardAuthRequest()
	{
		User* u = ServerInstance->FindUUID(AData.uuid);
		LocalUser* user = (u ? IS_LOCAL(u) : NULL);
		if (!user)
			return;

		auth_req_active = true;
		std::string line = "OPER " + AData.username + " :" + AData.password;
		ServerInstance->Parser->ProcessBuffer(line, user);
		auth_req_active = false;
	}

	void Failure()
	{
		User* u = ServerInstance->FindUUID(AData.uuid);
		LocalUser* user = (u ? IS_LOCAL(u) : NULL);
		if (!user)
			return;

		user->WriteNumeric(491, "%s :Invalid oper credentials", user->nick.c_str());
		user->CommandFloodPenalty += 10000;
	}

 public:
	AuthRequest(bool grace, dynamic_reference<ServiceProvider>& prov, const std::string& ipa, const std::string& apk, const AuthData& ad) : BufferedSocket(-1), graceful_failure(grace), api_key(apk), AData(ad), ip(ipa)
	{
		if (prov)
			AddIOHook(prov->creator);

		DoConnect(ip, (prov ? 443 : 80), 10, "");
	}

	void OnConnected()
	{
		const std::string req_url = "/protected/json/verify/" + AData.token + "/" + AData.authy_id + "?api_key=" + api_key + "&force=true";
		WriteData("GET " + req_url + " HTTP/1.1\r\nHost: api.authy.com" + "\r\n\r\n");
	}

	void OnDataReady()
	{
		// We only need the response code
		std::string line;
		GetNextLine(line);

		// 200 always means the auth is OK as long as force=true
		if (line == "HTTP/1.1 200 OK\r")
			ForwardAuthRequest();
		else if (line == "HTTP/1.1 401 Unauthorized\r")
			Failure();
		else
		{
			ServerInstance->SNO->WriteToSnoMask('a', "\2WARNING\2: Got unknown response code %s from Authy on OPER attempt from %s - %s",
				line.c_str(), AData.username.c_str(), (graceful_failure ? "pretending OTP OK" : "rejected OPER attempt"));
			if (graceful_failure)
				ForwardAuthRequest();
			else
				Failure();
		}
		Close();
		ServerInstance->GlobalCulls.AddItem(this);
	}

	void OnError(BufferedSocketError e)
	{
		ServerInstance->SNO->WriteToSnoMask('a', "\2WARNING\2: Could not connect to Authy to verify OPER attemt from %s - %s",
			AData.username.c_str(), (graceful_failure ? "pretending OTP OK" : "rejected OPER attempt"));
		if (graceful_failure)
			ForwardAuthRequest();
		else
			Failure();

		Close();
		ServerInstance->GlobalCulls.AddItem(this);
	}
};

class ModuleAuthy : public Module
{
	std::string ip;
	std::string api_key;
	bool graceful;
	dynamic_reference<ServiceProvider> SSLProv;

 public:
	ModuleAuthy() : SSLProv(this, "ssl"), bghits(0)
	{
	}

	void init()
	{
		Implementation eventlist[] = { I_OnPreCommand, I_OnRehash, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist) / sizeof(Implementation));
		OnRehash(NULL);
	}

	void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("authy");
		graceful = tag->getBool("graceful", false);
		api_key = tag->getString("apikey");

		std::string ssl = tag->getString("ssl", "ssl");
		SSLProv.SetProvider((!ssl.empty() && ssl != "ssl") ? "ssl/" + ssl : ssl);
		if (!SSLProv)
			ServerInstance->SNO->WriteToSnoMask('a', "\2WARNING\2: m_authy was told to use " + ssl
				+ " for encryption, but the provider is not loaded. Falling back to plain text");
	}

	// In case the location for the API changes
	size_t bghits;
	void OnBackgroundTimer(time_t curtime)
	{
		if (bghits++ % 120)
			return;

		try
		{
			bool cached;
			ResolveAPI* res = new ResolveAPI(this, ip, cached);
			ServerInstance->AddResolver(res, cached);
		}
		catch (...)
		{
		}
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		if (auth_req_active)
			return MOD_RES_PASSTHRU;

		if (validated && command == "OPER" && parameters.size() > 1)
		{
			OperIndex::iterator it = ServerInstance->Config->oper_blocks.find(parameters[0]);
			if (it == ServerInstance->Config->oper_blocks.end())
				return MOD_RES_PASSTHRU;

			OperInfo* info = it->second;

			std::string authyid;
			if (!info->oper_block->readString("authyid", authyid))
				return MOD_RES_PASSTHRU;

			size_t pos = parameters[1].rfind(' ');
			if (pos == std::string::npos)
			{
				user->WriteNumeric(491, "%s :This oper login requires an Authy token.", user->nick.c_str());
				return MOD_RES_DENY;
			}

			std::string otp = parameters[1].substr(pos + 1);
			parameters[1].erase(pos);

			new AuthRequest(graceful, SSLProv, ip, api_key, AuthRequest::AuthData(user, parameters[0], parameters[1], authyid, otp));

			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Enables two factor authentification for oper blocks using Authy");
	}
};

MODULE_INIT(ModuleAuthy)
