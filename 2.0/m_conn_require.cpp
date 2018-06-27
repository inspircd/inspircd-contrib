/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
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

/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDesc: Allow or block connections based on multiple criteria */
/* $ModDepends: core 2.0 */
/* $ModConfig: <connrequire timeout="5" ctcpstring="TIME"> and see below comments */

/* <dualversion active="yes" show="yes" ban="yes" duration="7d" reason="Fix your client!">
 * <badversion mask="*terrible script*" reason="Your script is terrible, bye bye!">
 * <badversion mask="xchat*" ban="yes" duration="7d" reason="Time to upgrade to Hexchat!">
 * <banmissing cap="yes" version="yes" duration="14d" reason="Upgrade your client!">
 * <banmissing ctcp="yes" duration="1d" reason="Don't ignore me!">
 *
 * Descriptions and defaults:
 * <connrequire >
 * timeout:	max number of seconds to hold a user while waiting for replies. Default: 5
 * ctcpstring:	a secondary CTCP request, aside from "VERSION". Default: blank
 * <dualversion >
 * active:	controls the second "VERSION" request that blocks on mismatch. Default: no
 * show:	send a SNOTICE of the two replies when they don't match. Default: no
 * ban:		Z-Line the IP on a mismatch. Default: no
 * duration:	time string for the Z-Line duration. Default: 7d
 * reason:	used for both the block and Z-Line. Default: Fix your client!
 * <badversion >
 * mask:	wildcard mask to block/ban of an unwanted client version. Default: blank
 * ban:		Z-Line the IP on a match. Default: no
 * duration:	time string for the Z-Line duration. Default: 7d
 * reason:	used for both the block and Z-Line. Default: Upgrade your client!
 * <banmissing >
 * cap:		whether a lack of CAP matches this tag. Default: no
 * ctcp:	whether a lack of secondary CTCP (if enabled) reply matches this tag. Default: no
 * version:	whether a lack of VERSION reply matches this tag. Default: no
 * duration:	time string for the Z-Line duration. Default: 1d
 * reason:	used for the Z-Line. Default: Fix your client!
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
#include "xline.h"


// ExtItem per User, tracking CAP request and CTCP replies
struct UserData
{
	bool sentcap;
	bool ctcpreply;
	std::string firstversionreply;
	std::string secondversionreply;

	UserData()
		: sentcap(false)
		, ctcpreply(false)
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
	LocalIntExt zapped;

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
	std::string ctcpstring;
	time_t timeout;

	void SetZLine(User* user, time_t duration, const std::string& reason, const std::string& from)
	{
		XLineFactory* xlf = ServerInstance->XLines->GetFactory("Z");
		if (!xlf)
			return;

		const std::string& mask = user->GetIPString();
		const std::string& source = ServerInstance->Config->ServerName;
		const std::string timetype = (duration == 0 ? "permanent" : "timed");
		const std::string expires = (duration == 0 ? "" : ", to expire on " + ServerInstance->TimeString(ServerInstance->Time() + duration));
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
			ServerInstance->SNO->WriteToSnoMask('x', "%s added %s Z-Line on %s%s due to %s: %s", source.c_str(),
				timetype.c_str(), mask.c_str(), expires.c_str(), dueto.c_str(), reason.c_str());
		}
		else
			delete x;
	}

 public:
	ModuleConnRequire ()
		: userdata("USERDATA", this)
		, zapped("ZAPPED", this)
		, wrapper('\001')
		, ctcpversion("VERSION")
		, len_part(ctcpversion.length() + 2)
		, len_all(len_part + 1)
	{
	}

	void init()
	{
		if (ServerInstance->Modules->Find("m_requirectcp.so") != NULL)
			throw ModuleException("You have m_requirectcp loaded! This module will not work correctly alongside that.");

		OnRehash(NULL);
		ServerInstance->SNO->EnableSnomask('u', "CONN_REQUIRE");
		ServerInstance->Modules->AddService(userdata);
		Implementation eventlist[] = { I_OnCheckReady, I_OnLoadModule, I_OnPreCommand, I_OnPostCommand, I_OnRehash,
					       I_OnSetConnectClass, I_OnUserConnect, I_OnUserDisconnect, I_OnUserInit };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void Prioritize()
	{
		// So we can send the CTCP request(s) ASAP
		ServerInstance->Modules->SetPriority(this, I_OnUserInit, PRIORITY_FIRST);
	}

	void OnLoadModule(Module* mod)
	{
		// m_requirectcp will cause undesirable results
		if (mod->ModuleSourceFile != "m_requirectcp.so")
			return;

		const std::string message = "Warning: m_conn_require will not work correctly alongside m_requirectcp.";
		ServerInstance->SNO->WriteToSnoMask('a', message);
		throw ModuleException(message);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("connrequire");
		timeout = tag->getInt("timeout", 5);
		ctcpstring = tag->getString("ctcpstring");
		std::transform(ctcpstring.begin(), ctcpstring.end(), ctcpstring.begin(), ::toupper);

		tag = ServerInstance->Config->ConfValue("dualversion");
		dualversion = tag->getBool("active");
		dualshow = tag->getBool("showdual");
		dualban = tag->getBool("ban");
		dualduration = ServerInstance->Duration(tag->getString("duration", "7d"));
		dualreason = tag->getString("reason", "Fix your client!");

		// No need to send VERSION here, it's already taken care of
		if (ctcpstring == "VERSION")
		{
			throw ModuleException("Cannot use \"VERSION\" in secondary CTCP (\"ctcpstring\")");
			ctcpstring.clear();
		}

		// Keep timeout within sane limits
		if (timeout < 1)
			timeout = 5;
		else if (timeout > 30)
			timeout = 30;

		// Rebuild the badversions vector
		badversions.clear();
		ConfigTagList tags = ServerInstance->Config->ConfTags("badversion");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* itag = i->second;

			const std::string mask = itag->getString("mask");

			if (mask.empty())
			{
				throw ModuleException("Missing \"mask\" in <badversion> tag at " + itag->getTagLocation());
				continue;
			}

			BadVersion bv;
			bv.mask = mask;
			bv.ban = itag->getBool("ban");
			bv.duration = ServerInstance->Duration(itag->getString("duration", "7d"));
			bv.reason = itag->getString("reason", "Upgrade your client!");
			badversions.push_back(bv);
		}

		// Rebuild the banmissings vector
		banmissings.clear();
		tags = ServerInstance->Config->ConfTags("banmissing");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* itag = i->second;

			BanMissing bm;
			bm.cap = itag->getBool("cap");
			bm.ctcp = itag->getBool("ctcp");
			bm.version = itag->getBool("version");
			bm.duration = ServerInstance->Duration(itag->getString("duration", "1d"));
			bm.reason = itag->getString("reason", "Fix your client!");
			banmissings.push_back(bm);
		}
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		// Allow user to be held here for up to 'timeout' seconds
		if (user->signon + timeout > ServerInstance->Time())
			return MOD_RES_PASSTHRU;

		UserData* ud = userdata.get(user);
		if (!ud)
			return MOD_RES_PASSTHRU;

		// Hold while waiting for replies
		if (ud->firstversionreply.empty() || (dualversion && ud->secondversionreply.empty()) || (!ctcpstring.empty() && !ud->ctcpreply))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser* user, bool validated, const std::string &original_line)
	{
		// Make sure it's a NOTICE to us, with at least one more parameter
		if (command != "NOTICE" || validated || parameters.size() < 2 || parameters[0] != ServerInstance->Config->ServerName)
			return MOD_RES_PASSTHRU;

		const std::string& param = parameters[1];
		// Check for length and the wrapper char
		if (param.length() < 2 || param[0] != wrapper)
			return MOD_RES_PASSTHRU;

		UserData* ud = userdata.get(user);
		if (!ud)
			return MOD_RES_PASSTHRU;

		// VERSION reply
		if (!param.compare(1, ctcpversion.length(), ctcpversion))
		{
			const std::string& rplversion = (param.length() > len_part ? param.substr(len_part, param.length() - len_all) : "");
			const std::string& firstversionreply = ud->firstversionreply;
			const std::string& secondversionreply = ud->secondversionreply;

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

				ServerInstance->SNO->WriteToSnoMask('u', "Blocked user %s (%s) [%s] on port %d, version reply \"%s\" matched badversion mask \"%s\"",
					user->GetFullRealHost().c_str(), user->GetIPString(), user->fullname.c_str(), user->GetServerPort(), rplversion.c_str(), bv.mask.c_str());
				zapped.set(user, 1);
				ServerInstance->Users->QuitUser(user, bv.reason);

				return MOD_RES_DENY;
			}

			// First reply
			if (firstversionreply.empty())
			{
				ud->firstversionreply = rplversion;
				if (dualversion)
					user->WriteServ("PRIVMSG %s :%c%s%c", user->nick.c_str(), wrapper, ctcpversion.c_str(), wrapper);
			}
			// Second reply
			else if (secondversionreply.empty())
				ud->secondversionreply = rplversion;

			if (dualversion && (!firstversionreply.empty() && !secondversionreply.empty() && firstversionreply != secondversionreply))
			{
				if (dualban)
					SetZLine(user, dualduration, dualreason, "dual");

				ServerInstance->SNO->WriteToSnoMask('u', "Blocked user %s (%s) [%s] from connecting on port %d for mismatched version replies",
					user->GetFullRealHost().c_str(), user->GetIPString(), user->fullname.c_str(), user->GetServerPort());

				if (dualshow)
					ServerInstance->SNO->WriteToSnoMask('u', "Version replies \"%s\" and \"%s\"", firstversionreply.c_str(), secondversionreply.c_str());

				zapped.set(user, 1);
				ServerInstance->Users->QuitUser(user, dualreason);
			}
		}
		// Configurable second CTCP string reply
		else if (!ctcpstring.empty() && !param.compare(1, ctcpstring.length(), ctcpstring))
			ud->ctcpreply = true;

		return MOD_RES_DENY;
	}

	void OnPostCommand(const std::string& command, const std::vector<std::string>& parameters, LocalUser* user, CmdResult result, const std::string& original_line)
	{
		if (command != "CAP")
			return;

		UserData* ud = userdata.get(user);

		if (ud && !parameters.empty() && InspIRCd::Match(parameters[0], "LS"))
			ud->sentcap = true;
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* cc)
	{
		// Don't mess with the initial class setting
		// This way we only act after the client has had time to send CAP or CTCP replies
		if (user->registered != REG_NICKUSER)
			return MOD_RES_PASSTHRU;

		UserData* ud = userdata.get(user);
		if (!ud)
			return MOD_RES_PASSTHRU;

		// Check class requirements against our UserData
		if ((!cc->config->getBool("requirecap") || ud->sentcap) &&
		   (!cc->config->getBool("requireversion") || !ud->firstversionreply.empty()) &&
		   (ctcpstring.empty() || !cc->config->getBool("requirectcp") || ud->ctcpreply))
			return MOD_RES_PASSTHRU;

		return MOD_RES_DENY;
	}

	void OnUserInit(LocalUser* user)
	{
		// Initialize their UserData and send the CTCP request(s)
		UserData ud;
		userdata.set(user, ud);

		user->WriteServ("PRIVMSG %s :%c%s%c", user->nick.c_str(), wrapper, ctcpversion.c_str(), wrapper);
		if (!ctcpstring.empty())
			user->WriteServ("PRIVMSG %s :%c%s%c", user->nick.c_str(), wrapper, ctcpstring.c_str(), wrapper);
	}

	void OnUserConnect(LocalUser* user)
	{
		// If they made it here, they passed; ditch their UserData
		UserData* ud = userdata.get(user);
		if (ud)
			userdata.unset(user);
	}

	void OnUserDisconnect(LocalUser* user)
	{
		// Skip proper users
		if (user->registered == REG_ALL)
			return;

		UserData* ud = userdata.get(user);
		if (!ud)
			return;

		bool noCap = !ud->sentcap;
		bool noRpl = (!ctcpstring.empty() && !ud->ctcpreply);
		bool noVer = ud->firstversionreply.empty();

		// We didn't do it
		if (!noCap && !noRpl && !noVer)
			return;

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

		// We already sent a SNOTICE for these users
		if (zapped.get(user))
			return;

		// Send out a SNOTICE that we likely caused this user to not get through
		std::string buffer = "Disconnecting unregistered user " + user->GetFullRealHost();
		buffer.append(" (" + std::string(user->GetIPString()) + ") [" + user->fullname + "]");
		buffer.append(" on port " + ConvToStr(user->GetServerPort()));
		buffer.append(" that was missing: ");
		if (noCap)
			buffer.append("CAP, ");
		if (noRpl)
			buffer.append(ctcpstring + " REPLY, ");
		if (noVer)
			buffer.append("VERSION REPLY");

		// Remove trailing ", " if there
		if (buffer[buffer.length() - 1] == ' ')
			buffer.erase(buffer.length() - 2, 2);

		ServerInstance->SNO->WriteToSnoMask('u', buffer);
	}

	Version GetVersion()
	{
		return Version("Allow or block connections based on multiple criteria", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleConnRequire)
