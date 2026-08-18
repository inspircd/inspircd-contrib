/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 TheUnstoppable <theunstoppable1000@gmail.com>
 *   Copyright (C) 2018-2020 Matt Schatz <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <connrequire timeout="5" ctcpstring="TIME" blockmessage="Your client isn't up to spec!">
/// $ModDepends: core 4
/// $ModDesc: Allow or block connections based on multiple criteria

/* More available config tags:
 * <dualversion active="yes" show="yes" ban="yes" duration="7d" reason="Fix your client!">
 * <badversion mask="*terrible script*" reason="Your script is terrible, bye bye!">
 * <badversion mask="xchat*" ban="yes" duration="7d" reason="Time to upgrade to Hexchat!">
 * <banmissing cap="yes" version="yes" duration="14d" reason="Upgrade your client!">
 * <banmissing ctcp="yes" duration="1d" reason="Don't ignore me!">
 *
 * Descriptions and defaults:
 * <connrequire >
 * timeout:        max number of seconds to hold a user while waiting for replies. Default: 5
 * ctcpstring:     a secondary CTCP request, aside from "VERSION". Default: blank
 * blockmessage:   a message sent to the user upon disconnect if we likely caused it. Default: blank
 * disableversion: disable the CTCP "VERSION" request (leaves just CAP and ctcpstring useful). Default: no
 * <dualversion >
 * active:         controls the second "VERSION" request that blocks on mismatch. Default: no
 * show:           send a SNOTICE of the two replies when they don't match. Default: no
 * ban:            Z-Line the IP on a mismatch. Default: no
 * duration:       time string for the Z-Line duration. Default: 7d
 * reason:         used for both the block and Z-Line. Default: Fix your client!
 * <badversion >
 * mask:           wildcard mask to block/ban of an unwanted client version. Default: blank
 * ban:            Z-Line the IP on a match. Default: no
 * duration:       time string for the Z-Line duration. Default: 7d
 * reason:         used for both the block and Z-Line. Default: Upgrade your client!
 * <banmissing >
 * cap:            whether a lack of CAP matches this tag. Default: no
 * ctcp:           whether a lack of secondary CTCP (if enabled) reply matches this tag. Default: no
 * version:        whether a lack of VERSION reply matches this tag. Default: no
 * duration:       time string for the Z-Line duration. Default: 1d
 * reason:         used for the Z-Line. Default: Fix your client!
 *
 * <badversion> and <banmissing> tags will be matched in the same order as they appear
 * in the config.
 *
 * Connect class options:
 * requirecap:		require the client to have requested CAP.
 * requirectcp:		requre the client to have replied to the secondary CTCP request.
 * requireversion:	require the client to have replied to the VERSION request.
 * All three default to "no".
 *
 * The intent here is to harden your "open" connect classes with these options to
 * deny them a connection. If you wish to Z-Line for certain reasons, use a
 * <banmissing> tag. The dual version check acts on it's own.
 * You can also use the <banversion> tags as freely as you wish.
 *
 * SNOTICES regarding blocked user connections are sent to SNOMASK 'u'
 */

/* NOTE: This module is not a direct replacement for m_requirectcp but is a
 * complete rework of the original idea. They cannot be loaded at the same time.
 */


#include "inspircd.h"
#include "extension.h"
#include "timeutils.h"
#include "clientprotocolmsg.h"
#include "xline.h"

// ExtItem per User, tracking CAP request and CTCP replies
struct UserData
{
	bool ccblocked;
	bool ctcpreply;
	bool expectctcp;
	bool expectversion;
	bool selfquit;
	bool sentcap;
	bool zapped;
	std::string firstversionreply;
	std::string secondversionreply;

	UserData()
		: ccblocked(false)
		, ctcpreply(false)
		, expectctcp(false)
		, expectversion(false)
		, selfquit(false)
		, sentcap(false)
		, zapped(false)
	{
	}
};

// Data from one <badversion> tag
struct BadVersion
{
	bool ban;
	time_t duration;
	std::string mask;
	std::string reason;
};

// Data from one <banmising> tag
struct BanMissing
{
	bool cap;
	bool ctcp;
	bool version;
	time_t duration;
	std::string reason;
};

class ModuleConnRequire : public Module
{
	SimpleExtItem<UserData> userdata;

	std::vector<BadVersion> badversions;
	std::vector<BanMissing> banmissings;

	bool dualversion;
	bool dualshow;
	bool dualban;
	time_t dualduration;
	std::string dualreason;

	const char wrapper;
	const std::string ctcpversion;
	const std::string::size_type len_part;
	const std::string::size_type len_all;
	bool disableversion;
	std::string ctcpstring;
	std::string blockmessage;
	time_t timeout;

	void SetZLine(User* user, time_t duration, const std::string& reason, const std::string& from)
	{
		XLineFactory* xlf = ServerInstance->XLines->GetFactory("Z");
		if (!xlf)
			return;

		const std::string& mask = user->GetAddress();
		const std::string& source = ServerInstance->Config->ServerName;
		const std::string timetype = (duration == 0 ? "permanent" : "timed");
		const std::string expires = (duration == 0 ? "" : INSP_FORMAT(", expires in {} (on {})",
			Duration::ToString(duration),
			Time::ToString(ServerInstance->Time() + duration)));

		std::string dueto;
		if (from == "dual")
			dueto = "a version reply mismatch";
		else if (from == "badversion")
			dueto = "a match to a bad version";
		else if (from == "banmissing")
			dueto = "a match to a ban on missing data";

		XLine* x = xlf->Generate(ServerInstance->Time(), duration, source, reason, mask);
		if (ServerInstance->XLines->AddLine(x, NULL))
		{
			ServerInstance->SNO.WriteToSnoMask('x', "{} added {} Z-line for {}{} due to {}: {}", source,
				timetype, mask, expires, dueto, reason);
		}
		else
			delete x;
	}

 public:
	ModuleConnRequire ()
		: Module(VF_NONE, "Allow or block connections based on multiple criteria")
		, userdata(this, "userdata", ExtensionType::USER)
		, wrapper('\001')
		, ctcpversion("VERSION")
		, len_part(ctcpversion.length() + 2)
		, len_all(len_part + 1)
	{
	}

	void init() override
	{
		if (ServerInstance->Modules.Find("m_requirectcp.so") != NULL)
			throw ModuleException(this, "You have m_requirectcp loaded! This module will not work correctly alongside that.");

		ServerInstance->SNO.EnableSnomask('u', "CONN_REQUIRE");
	}

	void Prioritize() override
	{
		// Send the CTCP requests ASAP.
		ServerInstance->Modules.SetPriority(this, I_OnChangeRemoteAddress, PRIORITY_FIRST);
	}

	void OnLoadModule(Module* mod) override
	{
		// m_requirectcp will cause undesirable results
		if (mod->ModuleFile != "m_requirectcp.so")
			return;

		const std::string message = "Warning: m_conn_require will not work correctly alongside m_requirectcp.";
		ServerInstance->SNO.WriteToSnoMask('a', message);
		throw ModuleException(this, message);
	}

	void ReadConfig(ConfigStatus&) override
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("connrequire").get();
		timeout = tag->getNum("timeout", 5, 1, 30);
		disableversion = tag->getBool("disableversion");
		ctcpstring = tag->getString("ctcpstring");
		blockmessage = tag->getString("blockmessage");
		std::transform(ctcpstring.begin(), ctcpstring.end(), ctcpstring.begin(), ::toupper);

		tag = ServerInstance->Config->ConfValue("dualversion").get();
		dualversion = tag->getBool("active");
		dualshow = tag->getBool("showdual");
		dualban = tag->getBool("ban");
		dualduration = tag->getDuration("duration", 60*60*24*7);
		dualreason = tag->getString("reason", "Fix your client!");

		// No need to send VERSION here, it's already taken care of
		if (!strcmp(ctcpstring.c_str(), "VERSION"))
		{
			throw ModuleException(this, "Cannot use \"VERSION\" in secondary CTCP (\"ctcpstring\")");
			ctcpstring.clear();
		}

		// Rebuild the badversions vector
		badversions.clear();
		for (const auto& [_, itag] : ServerInstance->Config->ConfTags("badversion"))
		{
			const std::string mask = itag->getString("mask");

			if (mask.empty())
			{
				throw ModuleException(this, "Missing \"mask\" in <badversion> tag at " + itag->source.str());
				continue;
			}

			BadVersion bv;
			bv.mask = mask;
			bv.ban = itag->getBool("ban");
			bv.duration = itag->getDuration("duration", 60*60*24*7);
			bv.reason = itag->getString("reason", "Upgrade your client!");
			badversions.push_back(bv);
		}

		// Rebuild the banmissings vector
		banmissings.clear();
		for (const auto& [_, itag] : ServerInstance->Config->ConfTags("banmissing"))
		{
			BanMissing bm;
			bm.cap = itag->getBool("cap");
			bm.ctcp = itag->getBool("ctcp");
			bm.version = itag->getBool("version");
			bm.duration = itag->getDuration("duration", 60*60*24);
			bm.reason = itag->getString("reason", "Fix your client!");
			banmissings.push_back(bm);
		}
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		// Allow user to be held here for up to 'timeout' seconds
		if (user->signon + timeout <= ServerInstance->Time())
			return MOD_RES_PASSTHRU;

		UserData* ud = userdata.Get(user);
		if (!ud)
			return MOD_RES_PASSTHRU;

		// Hold while waiting for replies
		if ((!disableversion && ud->firstversionreply.empty()) ||
		   (dualversion && ud->secondversionreply.empty()) ||
		   (!ctcpstring.empty() && !ud->ctcpreply))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		// Make sure we care about this user first
		UserData* ud = userdata.Get(user);
		if (!ud)
			return MOD_RES_PASSTHRU;

		// Mark self quitters to avoid messaging or Z-Lining them
		if (command == "QUIT")
		{
			ud->selfquit = true;
			return MOD_RES_PASSTHRU;
		}

		// Now we check for three important things:
		// 1) The command is a NOTICE (not yet validated)
		// 2) The NOTICE is to us
		// 3) The NOTICE contains at least one parameter after the target
		if (command != "NOTICE" || validated || parameters.size() < 2 || parameters[0] != ServerInstance->Config->ServerName)
			return MOD_RES_PASSTHRU;

		const std::string& param = parameters[1];
		// Check for length and the wrapper char
		if (param.length() < 2 || param[0] != wrapper)
			return MOD_RES_PASSTHRU;

		// VERSION reply that we are expecting
		if (!disableversion && ud->expectversion && !param.compare(1, ctcpversion.length(), ctcpversion))
		{
			const std::string& rplversion = (param.length() > len_part ? param.substr(len_part, param.length() - len_all) : "");
			const std::string& firstversionreply = ud->firstversionreply;
			const std::string& secondversionreply = ud->secondversionreply;

			ud->expectversion = false;

			// Ignore empty replies
			if (rplversion.empty())
				return MOD_RES_DENY;

			// Check for a match to a configured <badversion>
			for (std::vector<BadVersion>::const_iterator it = badversions.begin(); it != badversions.end(); ++it)
			{
				const BadVersion& bv = *it;
				if (!InspIRCd::Match(rplversion, bv.mask))
					continue;

				if (bv.ban)
					SetZLine(user, bv.duration, bv.reason, "badversion");

				ServerInstance->SNO.WriteToSnoMask('u', "Blocked user {} ({}) [{}] on port {}, version reply \"{}\" matched badversion mask \"{}\"",
					user->GetRealMask(), user->GetAddress(), user->GetRealName(), user->server_sa.port(), rplversion, bv.mask);
				ud->zapped = true;
				ServerInstance->Users.QuitUser(user, bv.reason);

				return MOD_RES_DENY;
			}

			// First reply
			if (firstversionreply.empty())
			{
				ud->firstversionreply = rplversion;
				if (dualversion)
				{
					ud->expectversion = true;
					const std::string msg = wrapper + ctcpversion + wrapper;
					ClientProtocol::Messages::Privmsg ctcp(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->Config->ServerName, user, msg);
					user->Send(ServerInstance->GetRFCEvents().privmsg, ctcp);
				}
			}
			// Second reply
			else if (secondversionreply.empty())
				ud->secondversionreply = rplversion;

			if (dualversion && (!firstversionreply.empty() && !secondversionreply.empty() && firstversionreply != secondversionreply))
			{
				if (dualban)
					SetZLine(user, dualduration, dualreason, "dual");

				ServerInstance->SNO.WriteToSnoMask('u', "Blocked user {} ({}) [{}] from connecting on port {} for mismatched version replies",
					user->GetRealMask(), user->GetAddress(), user->GetRealName(), user->server_sa.port());

				if (dualshow)
					ServerInstance->SNO.WriteToSnoMask('u', "Version replies \"{}\" and \"{}\"", firstversionreply, secondversionreply);

				ud->zapped = true;
				ServerInstance->Users.QuitUser(user, dualreason);
			}

			return MOD_RES_DENY;
		}
		// Configurable CTCP string reply that we are expecting
		else if (!ctcpstring.empty() && ud->expectctcp && !param.compare(1, ctcpstring.length(), ctcpstring))
		{
			ud->expectctcp = false;
			ud->ctcpreply = true;

			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void OnPostCommand(Command* command, const CommandBase::Params& parameters, LocalUser* user, CmdResult result, bool loop) override
	{
		if (command->name != "CAP")
			return;

		UserData* ud = userdata.Get(user);

		if (ud && !parameters.empty() && irc::equals(parameters[0], "LS"))
			ud->sentcap = true;
	}

	ModResult OnPreChangeConnectClass(LocalUser* user, const std::shared_ptr<ConnectClass>& klass, std::optional<Numeric::Numeric>& errnum) override
	{
		auto cc = klass.get();

		// Don't mess with the initial class setting
		// This way we only act after the client has had time to send CAP or CTCP replies
		if (user->connected != User::CONN_NICKUSER)
			return MOD_RES_PASSTHRU;

		UserData* ud = userdata.Get(user);
		if (!ud)
			return MOD_RES_PASSTHRU;

		// Check class requirements against our UserData
		if ((!cc->config->getBool("requirecap") || ud->sentcap) &&
		   (disableversion || !cc->config->getBool("requireversion") || !ud->firstversionreply.empty()) &&
		   (ctcpstring.empty() || !cc->config->getBool("requirectcp") || ud->ctcpreply))
			return MOD_RES_PASSTHRU;

		ud->ccblocked = true;
		return MOD_RES_DENY;
	}

	void OnUserPostInit(LocalUser* user) override
	{
		// Initialize their UserData and send the CTCP request(s)
		UserData* ud = new UserData;
		userdata.Set(user, ud);

		if (!disableversion)
		{
			ud->expectversion = true;
			const std::string msg = wrapper + ctcpversion + wrapper;
			ClientProtocol::Messages::Privmsg ctcp(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->Config->ServerName, user, msg);
			user->Send(ServerInstance->GetRFCEvents().privmsg, ctcp);
		}
		if (!ctcpstring.empty())
		{
			ud->expectctcp = true;
			const std::string msg = wrapper + ctcpstring + wrapper;
			ClientProtocol::Messages::Privmsg ctcp(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->Config->ServerName, user, msg);
			user->Send(ServerInstance->GetRFCEvents().privmsg, ctcp);
		}
	}

	void OnUserConnect(LocalUser* user) override
	{
		// If they made it here, they passed; ditch their UserData
		if (userdata.Get(user))
			userdata.Unset(user);
	}

	void OnUserDisconnect(LocalUser* user) override
	{
		// Skip proper users
		if (user->IsFullyConnected())
			return;

		// Skip users with a socket level error
		// This ignores things like port scans, ZNC cert errors, etc.
		if (!user->eh.GetError().empty())
			return;

		// Skip users we don't know about or that self quit
		UserData* ud = userdata.Get(user);
		if (!ud || ud->selfquit)
			return;

		// We already disconnected (and possibly banned) these users
		if (ud->zapped)
			return;

		// We didn't block any connect classes for this user.
		if (!ud->ccblocked)
		{
			ServerInstance->Logs.Debug(MODNAME, "Unregistered user exiting for unknown reasons: {} ({}) [{}]",
				user->GetRealMask(), user->GetAddress(), user->GetRealName());
			return;
		}

		// Send them a message if configured
		if (!blockmessage.empty())
			user->WriteNotice(blockmessage);

		bool noCap = !ud->sentcap;
		bool noRpl = (!ctcpstring.empty() && !ud->ctcpreply);
		bool noVer = (!disableversion && ud->firstversionreply.empty());

		// Check for a match to our BanMissing and then Z-Line
		for (std::vector<BanMissing>::const_iterator it = banmissings.begin(); it != banmissings.end(); ++it)
		{
			const BanMissing& bm = *it;

			// We need to match a <banmissing> entirely. So if we want to match
			// to missing version and ctcpstring but not cap, we need to skip
			// any users that are missing cap; and so forth.
			if (((!bm.cap && noCap) || (bm.cap && !noCap)) ||
			   ((!bm.ctcp && noRpl) || (bm.ctcp && !noRpl)) ||
			   ((!bm.version && noVer) || (bm.version && !noVer)))
				continue;

			SetZLine(user, bm.duration, bm.reason, "banmissing");
		}

		// Send out a SNOTICE that we likely caused this user to not get through
		std::string buffer = "Unregistered user exiting: " + user->GetRealMask();
		buffer.append(" (" + std::string(user->GetAddress()) + ") [" + user->GetRealName() + "]");
		buffer.append(" on port " + ConvToStr(user->server_sa.port()));
		buffer.append(". Possibly due to missing: ");
		if (noCap)
			buffer.append("CAP, ");
		if (noRpl)
			buffer.append(ctcpstring + " REPLY, ");
		if (noVer)
			buffer.append("VERSION REPLY");

		// Remove trailing ", " if there
		if (buffer[buffer.length() - 1] == ' ')
			buffer.erase(buffer.length() - 2, 2);

		ServerInstance->SNO.WriteToSnoMask('u', buffer);
	}
};

MODULE_INIT(ModuleConnRequire)
