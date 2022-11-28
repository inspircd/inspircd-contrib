/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2020 Sadie Powell <sadie@witchery.services>
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
#include "modules/httpd.h"
#include "modules/ircv3_replies.h"

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
		token.append(BinToBase64(payload, BASE64_URL));
		const std::string signature = sha256->hmac(secret, token);
		token.append(".").append(BinToBase64(signature, BASE64_URL));
		return token;
	}
}

struct JWTService CXX11_FINAL
{
	// The duration that a generated JWT is valid for.
	unsigned long duration;

	// The shared secret between the IRC server and the external service.
	std::string secret;

	// The URL for verifying JWT for this service.
	std::string verifyurl;

	JWTService(unsigned long Duration, const std::string& Secret, const std::string& VerifyURL)
		: duration(Duration)
		, secret(Secret)
		, verifyurl(VerifyURL)
	{
	}
};

typedef insp::flat_map<std::string, JWTService, irc::insensitive_swo> ServiceMap;

class ExtJWTMessage CXX11_FINAL
	: public ClientProtocol::Message
{
 public:
	ExtJWTMessage(Channel* chan, const std::string& service, bool last, const std::string& data)
		: ClientProtocol::Message("EXTJWT", ServerInstance->Config->ServerName)
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

class ExtJWTVerifier CXX11_FINAL
	: public HTTPRequestEventListener
{
 private:
	HTTPdAPI httpapi;
	dynamic_reference_nocheck<HashProvider>& sha256;
	ServiceMap& services;


	ModResult HandleResponse(HTTPRequest& request, unsigned int code, const std::string& reason)
	{
		std::stringstream data(reason); // This API is awful.
		HTTPDocumentResponse response(const_cast<Module*>(GetModule()), request, &data, code);
		response.headers.SetHeader("X-Powered-By", MODNAME);
		if (!reason.empty())
			response.headers.SetHeader("Content-Type", "text/plain");
		httpapi->SendResponse(response);
		return MOD_RES_DENY;
	}

 public:
	ExtJWTVerifier(Module* Creator, ServiceMap& Services, dynamic_reference_nocheck<HashProvider>& SHA256)
		: HTTPRequestEventListener(Creator)
		, httpapi(Creator)
		, sha256(SHA256)
		, services(Services)
	{
	}

	ModResult OnHTTPRequest(HTTPRequest& request) CXX11_OVERRIDE
	{
		irc::sepstream pathstream(request.GetPath(), '/');
		std::string pathtoken;

		// Check the root /extjwt path was specified.
		if (!pathstream.GetToken(pathtoken) || !stdalgo::string::equalsci(pathtoken, "extjwt"))
			return MOD_RES_PASSTHRU; // Pass through to another module.

		// Check a service was specified if multiple exist.
		ServiceMap::iterator siter = services.begin();
		if (services.size() > 1)
		{
			if (!pathstream.GetToken(pathtoken))
				return HandleResponse(request, 400, "No JWT service specified");

			siter = services.find(pathtoken);
			if (siter == services.end())
				return HandleResponse(request, 400, "No such JWT service: " + pathtoken);
		}

		// Check that a pathtoken was specified.
		if (!pathstream.GetToken(pathtoken))
			return HandleResponse(request, 400, "No JWT specified");

		// The server does not have the sha256 module loaded.
		if (!sha256)
			return HandleResponse(request, 500, "HMAC-SHA256 support is not available");

		// Check that the JWT is well formed.
		std::string header;
		std::string payload;
		std::string signature;
		irc::sepstream tokenstream(pathtoken, '.');
		if (!tokenstream.GetToken(header) || !tokenstream.GetToken(payload) || !tokenstream.GetToken(signature))
			return HandleResponse(request, 401, "Malformed JWT specified");

		// Decode the payload.
		payload = Base64ToBin(payload, BASE64_URL);

		// Check the header and signature are valid.
		if (pathtoken != CreateJWT(sha256, payload, siter->second.secret))
			return HandleResponse(request, 401, "Invalid JWT signature specified");

		// Validate the expiry time.
		rapidjson::Document document;
		if (document.Parse(payload).HasParseError() || !document.IsObject())
			return HandleResponse(request, 401, "Malformed JWT payload specified");

		rapidjson::Value::ConstMemberIterator eiter = document.FindMember("exp");
		if (eiter == document.MemberEnd() || !eiter->value.IsInt64())
			return HandleResponse(request, 401, "Malformed JWT payload specified");

		time_t expiry = eiter->value.GetInt64();
		if (expiry < ServerInstance->Time())
			return HandleResponse(request, 401, "Expired JWT specified");

		// XXX: should the issuer be verified here too?
		return HandleResponse(request, 200, "JWT token validated");
	}
};

class CommandExtJWT CXX11_FINAL
	: public SplitCommand
{
 private:
	ChanModeReference privatemode;
	ClientProtocol::EventProvider protoev;
	ChanModeReference secretmode;
	IRCv3::Replies::Fail fail;
	ExtJWTVerifier verifier;

 public:
	LocalIntExt ext;
	ServiceMap services;
	dynamic_reference_nocheck<HashProvider> sha256;

	CommandExtJWT(Module* Creator)
		: SplitCommand(Creator, "EXTJWT", 1, 2)
		, privatemode(Creator, "private")
		, protoev(Creator, "EXTJWT")
		, secretmode(Creator, "secret")
		, fail(Creator)
		, verifier(Creator, services, sha256)
		, ext("join-time", ExtensionItem::EXT_MEMBERSHIP, Creator)
		, sha256(Creator, "hash/sha256")
	{
		allow_empty_last_param = false;
		Penalty = 4;
		syntax = "*|<channel> [<service>]";
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		// Check that we actually have a SHA256 module.
		if (!sha256)
		{
			fail.Send(user, this, "UNSPECIFIED_ERROR", "JSON Web Token generation is currently unavailable!");
			return CMD_FAILURE;
		}

		// Is the user expecting a user token or a channel token?
		Channel* chan = NULL;
		if (parameters[0] != "*")
		{
			// Chec that the target channel actually exists.
			chan = ServerInstance->FindChan(parameters[0]);
			if (!chan || chan->IsModeSet(privatemode) || chan->IsModeSet(secretmode))
			{
				// The target channel does not exist.
				user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
				return CMD_FAILURE;
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
				fail.Send(user, this, "INVALID_PROFILE", "You specified an invalid JSON Web Token profile!");
				return CMD_FAILURE;
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
			writer.String(ServerInstance->Config->ServerName);

			writer.Key("sub");
			writer.String(user->nick);

			writer.Key("account");
			const AccountExtItem* accountext = GetAccountExtItem();
			const std::string* account = accountext ? accountext->get(user) : NULL;
			writer.String(account ? account->c_str() : "");

			writer.Key("umodes");
			writer.StartArray();
			{
				const std::string modes = user->GetModeLetters();
				for (size_t i = 1; i < modes.length(); ++i)
					writer.String(modes.c_str() + i, 1);
			}
			writer.EndArray();

			if (!siter->second.verifyurl.empty())
			{
				writer.Key("vfy");
				writer.String(siter->second.verifyurl);
			}

			if (chan)
			{
				writer.Key("channel");
				writer.String(chan->name);

				writer.Key("joined");
				Membership* memb = chan->GetUser(user);
				writer.Uint64(memb ? ext.get(memb) : 0);

				writer.Key("cmodes");
				writer.StartArray();
				{
					for (size_t i = 0; memb && i < memb->modes.length(); ++i)
						writer.String(memb->modes.c_str() + i, 1);
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
		return CMD_SUCCESS;
	}
};

class ModuleIRCv3ExtJWT CXX11_FINAL
	: public Module
{
 private:
	CommandExtJWT cmd;

 public:
	ModuleIRCv3ExtJWT()
		: cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ServiceMap newservices;
		ConfigTagList tags = ServerInstance->Config->ConfTags("extjwt");
		if (tags.first == tags.second)
			throw ModuleException("You have loaded the ircv3_extjwt module but not configured any <extjwt> tags!");

		for (ConfigIter iter = tags.first; iter != tags.second; ++iter)
		{
			ConfigTag* tag = iter->second;

			// A JWT can live for any time between ten seconds and ten minutes (default: 30 seconds).
			const unsigned long duration = tag->getDuration("duration", 30, 10, 10*60);
			const std::string verifyurl = tag->getString("verifyurl");

			// Always require a secret.
			const std::string secret = tag->getString("secret");
			if (secret.empty())
				throw ModuleException("<extjwt:secret> is a required field, at " + tag->getTagLocation());

			// Only require a name when more than one service is configured.
			const std::string name = tag->getString("name");
			if (name.empty() && std::distance(tags.first, tags.second) > 1)
				throw ModuleException("<extjwt:name> is a required field, at " + tag->getTagLocation());

			// If the insertion fails a JWTService with this name already exists.
			if (!newservices.insert(std::make_pair(name, JWTService(duration, secret, verifyurl))).second)
				throw ModuleException("<extjwt:name> (" + name + ") must be unique, at " + tag->getTagLocation());
		}

		std::swap(newservices, cmd.services);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTJWT"] = "1";
	}

	void OnPostJoin(Membership* memb) CXX11_OVERRIDE
	{
		if (IS_LOCAL(memb->user))
			cmd.ext.set(memb, ServerInstance->Time());
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the DRAFT extjwt IRCv3 extension", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleIRCv3ExtJWT)
