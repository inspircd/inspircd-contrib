/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2024 Sadie Powell <sadie@witchery.services>
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

/// $CompilerFlags: find_compiler_flags("RapidJSON")

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Provides the DRAFT extjwt IRCv3 extension.

#include "inspircd.h"
#include "modules/account.h"
#include "modules/hash.h"
#include "modules/ircv3_replies.h"
#include "modules/isupport.h"
#include "numerichelper.h"
#include "stringutils.h"

#define RAPIDJSON_HAS_STDSTRING 1

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace
{
	// The characters allowed in the base64url encoding format.
	const char BASE64_URL[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

	// The maximum size of a token chunk in an outgoing EXTJWT message.
	const size_t MAX_MSG_SIZE = 200;

	std::string CreateJWT(dynamic_reference_nocheck<HashProvider>& sha256, const std::string& payload, const std::string& secret)
	{
		std::string token("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."); // {"alg":"HS256","typ":"JWT"}
		token.append(Base64::Encode(payload, BASE64_URL));
		const std::string signature = sha256->hmac(secret, token);
		token.append(".").append(Base64::Encode(signature, BASE64_URL));
		return token;
	}
}

struct JWTService final
{
	// The duration that a generated JWT is valid for.
	unsigned long duration;

	// The shared secret between the IRC server and the external service.
	std::string secret;

	JWTService(unsigned long d, const std::string& s)
		: duration(d)
		, secret(s)
	{
	}
};

typedef insp::flat_map<std::string, JWTService, irc::insensitive_swo> ServiceMap;

class ExtJWTMessage final
	: public ClientProtocol::Message
{
public:
	ExtJWTMessage(Channel* chan, const std::string& service, bool last, const std::string& data)
		: ClientProtocol::Message("EXTJWT", ServerInstance->Config->GetServerName())
	{
		if (chan)
			PushParamRef(chan->name);
		else
			PushParam("*");
		if (service.empty())
			PushParam("*");
		else
			PushParamRef(service);
		if (!last)
			PushParam("*");
		PushParam(data);
	}
};

class CommandExtJWT final
	: public SplitCommand
{
private:
	Account::API accountapi;
	ChanModeReference privatemode;
	ClientProtocol::EventProvider protoev;
	ChanModeReference secretmode;
	IRCv3::Replies::Fail failrpl;

public:
	IntExtItem ext;
	ServiceMap services;
	dynamic_reference_nocheck<HashProvider> sha256;

	CommandExtJWT(Module* Creator)
		: SplitCommand(Creator, "EXTJWT", 1, 2)
		, accountapi(Creator)
		, privatemode(Creator, "private")
		, protoev(Creator, "EXTJWT")
		, secretmode(Creator, "secret")
		, failrpl(Creator)
		, ext(Creator, "join-time", ExtensionType::MEMBERSHIP)
		, sha256(Creator, "hash/sha256")
	{
		penalty = 4000;
		syntax = { "*|<channel> [<service>]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		// Check that we actually have a SHA256 module.
		if (!sha256)
		{
			failrpl.Send(user, this, "UNSPECIFIED_ERROR", "JSON Web Token generation is currently unavailable!");
			return CmdResult::FAILURE;
		}

		// Is the user expecting a user token or a channel token?
		Channel* chan = NULL;
		Membership* memb = NULL;
		if (parameters[0] != "*")
		{
			// Chec that the target channel actually exists.
			chan = ServerInstance->Channels.Find(parameters[0]);
			memb = chan ? chan->GetUser(user) : NULL;
			if (!chan || (!memb && (chan->IsModeSet(privatemode) || chan->IsModeSet(secretmode))))
			{
				// The target channel does not exist.
				user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
				return CmdResult::FAILURE;
			}
		}

		// Look up the appropriate JWT service.
		ServiceMap::iterator siter = services.begin();
		std::string servicename = "*";
		if (parameters.size() > 1)
		{
			siter = services.find(parameters[1]);
			if (siter == services.end())
			{
				failrpl.Send(user, this, "INVALID_PROFILE", "You specified an invalid JSON Web Token profile!");
				return CmdResult::FAILURE;
			}
			servicename = siter->first;
		}

		// Create the token JSON.
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		{
			writer.Key("exp");
			writer.Uint64(ServerInstance->Time() + siter->second.duration);

			writer.Key("iss");
			writer.String(ServerInstance->Config->GetServerName());

			writer.Key("sub");
			writer.String(user->nick);

			writer.Key("account");
			const std::string* account = accountapi ? accountapi->GetAccountName(user) : NULL;
			writer.String(account ? account->c_str() : "");

			writer.Key("umodes");
			writer.StartArray();
			{
				for (auto mode : user->GetModeLetters().substr(1))
					writer.String(&mode, 1);
			}
			writer.EndArray();

			if (chan)
			{
				writer.Key("channel");
				writer.String(chan->name);

				writer.Key("joined");
				writer.Uint64(memb ? ext.Get(memb) : 0);

				writer.Key("cmodes");
				writer.StartArray();
				{
					for (const auto* mh : memb->modes)
					{
						auto chr = mh->GetModeChar();
						writer.String(&chr, 1);
					}
				}
				writer.EndArray();
			}
		}
		writer.EndObject();


		// Build the JWT.
		const std::string token = CreateJWT(sha256, buffer.GetString(), siter->second.secret);
		size_t startpos = 0;
		while (token.length() - startpos > MAX_MSG_SIZE)
		{
			ExtJWTMessage jwtmsg(chan, servicename, false, token.substr(startpos, MAX_MSG_SIZE));
			ClientProtocol::Event jwtev(protoev, jwtmsg);
			user->Send(jwtev);
			startpos += MAX_MSG_SIZE;
		}

		ExtJWTMessage jwtmsg(chan, servicename, true, token.substr(startpos));
		ClientProtocol::Event jwtev(protoev, jwtmsg);
		user->Send(jwtev);
		return CmdResult::SUCCESS;
	}
};

class ModuleIRCv3ExtJWT final
	: public Module
	, public ISupport::EventListener
{
private:
	CommandExtJWT cmd;

public:
	ModuleIRCv3ExtJWT()
		: Module(VF_OPTCOMMON, "Provides the DRAFT extjwt IRCv3 extension.")
		, ISupport::EventListener(this)
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		auto tags = ServerInstance->Config->ConfTags("extjwt");
		if (tags.empty())
			throw ModuleException(this, "You have loaded the ircv3_extjwt module but not configured any <extjwt> tags!");

		ServiceMap newservices;
		for (const auto& [_, tag] : tags)
		{
			// A JWT can live for any time between ten seconds and ten minutes (default: 30 seconds).
			const auto duration = tag->getDuration("duration", 30, 10, 10*60);

			// Always require a secret.
			const auto secret = tag->getString("secret");
			if (secret.empty())
				throw ModuleException(this, "<extjwt:secret> is a required field, at " + tag->source.str());

			// Only require a name when more than one service is configured.
			const auto name = tag->getString("name");
			if (name.empty() && tags.count() > 1)
				throw ModuleException(this, "<extjwt:name> is a required field, at " + tag->source.str());

			// If the insertion fails a JWTService with this name already exists.
			if (!newservices.emplace(name, JWTService(duration, secret)).second)
				throw ModuleException(this, "<extjwt:name> (" + name + ") must be unique, at " + tag->source.str());
		}

		std::swap(newservices, cmd.services);
	}

	void OnBuildISupport(ISupport::TokenMap& tokens) override
	{
		tokens["EXTJWT"] = "1";
	}

	void OnPostJoin(Membership* memb) override
	{
		if (IS_LOCAL(memb->user))
			cmd.ext.Set(memb, ServerInstance->Time());
	}
};

MODULE_INIT(ModuleIRCv3ExtJWT)
