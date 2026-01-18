/* 
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
/// $ModDepends: core 3
/// $ModDesc: X-line management with XCOPY, XCOUNT, XREMOVE, and XSEARCH

#include "inspircd.h"
#include "xline.h"
#include "timeutils.h"
#include <cctype>
#include <climits>
#include <ctime>
#include <cstring>
#include <vector>
#include <string>

namespace
{
	enum MatchType { MATCH_ONLY, MATCH_NONE, MATCH_ANY };

	struct Criteria
	{
		MatchType config;
		std::string type, mask, reason, source, set, duration, expires;
		Criteria() { }
		Criteria(const std::string& t, const std::string& m, const std::string& r, const std::string& s)
			: type(t), mask(m), reason(r), source(s) { config = MATCH_ANY; }
	};

	bool HasCommandPermission(LocalUser* user, std::string type)
	{
		if (type.length() <= 2) type.append("LINE");
		return user->HasCommandPermission(type);
	}

	/* ParseDuration: parse strings like "1h30m" or "1y2w3d..." into seconds.
	 * Keep a local parser for conversion but prefer core Duration::IsValid for validation. */
	bool ParseDuration(const std::string& s_in, unsigned long& out)
	{
		out = 0;
		if (s_in.empty()) return false;
		std::string s = s_in;
		if (s[0] == '+' || s[0] == '-') s = s.substr(1);
		if (s.empty()) return false;

		size_t i = 0, len = s.length();
		auto addmul = [&](unsigned long val, unsigned long mul) -> bool {
			if (val == 0) return true;
			if (mul != 0 && val > (ULONG_MAX / mul)) return false;
			unsigned long add = val * mul;
			if (add > ULONG_MAX - out) return false;
			out += add;
			return true;
		};

		while (i < len)
		{
			if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
			unsigned long num = 0; bool anydigit = false;
			while (i < len && std::isdigit(static_cast<unsigned char>(s[i])))
			{
				anydigit = true;
				int digit = s[i] - '0';
				if (num > (ULONG_MAX - digit) / 10) return false;
				num = num * 10 + digit;
				++i;
			}
			if (!anydigit) return false;
			if (i == len) { if (!addmul(num, 1)) return false; break; }
			char unit = std::tolower(static_cast<unsigned char>(s[i++])); unsigned long mul = 0;
			switch (unit)
			{
				case 'y': mul = 31536000UL; break;
				case 'w': mul = 604800UL; break;
				case 'd': mul = 86400UL; break;
				case 'h': mul = 3600UL; break;
				case 'm': mul = 60UL; break;
				case 's': mul = 1UL; break;
				default: return false;
			}
			if (!addmul(num, mul)) return false;
		}
		return true;
	}

	bool IsValidDurationString(const std::string& s)
	{
		if (s.empty()) return false;
		if (s == "0") return true;
		// Prefer core Duration validation if available.
		return Duration::IsValid(s);
	}

	bool ProcessArgs(const CommandBase::Params& params, Criteria& args)
	{
		if (!params.size()) return false;
		bool argreason = false;
		const std::string mconfig("-config="), mtype("-type="), mmask("-mask="), mreason("-reason=");
		const std::string msource("-source="), msetby("-setby="), mset("-set=");
		const std::string mduration("-duration="), mexpires("-expires=");

		for (const auto& param : params)
		{
			if (irc::find(param, mconfig) != std::string::npos)
			{
				argreason = false;
				const std::string val(param.substr(mconfig.length()));
				if (!val.empty() && (irc::equals(val, "yes") || irc::equals(val, "true"))) args.config = MATCH_ONLY;
				else if (!val.empty() && (irc::equals(val, "no") || irc::equals(val, "false"))) args.config = MATCH_NONE;
			}
			else if (irc::find(param, mtype) != std::string::npos) { argreason = false; const std::string val(param.substr(mtype.length())); args.type = (!val.empty() ? val : "*"); }
			else if (irc::find(param, mmask) != std::string::npos) { argreason = false; const std::string val(param.substr(mmask.length())); args.mask = (!val.empty() ? val : "*"); }
			else if (irc::find(param, mreason) != std::string::npos) { argreason = true; const std::string val(param.substr(mreason.length())); args.reason = (!val.empty() ? val : "*"); }
			else if (irc::find(param, msource) != std::string::npos) { argreason = false; const std::string val(param.substr(msource.length())); args.source = (!val.empty() ? val : "*"); }
			else if (irc::find(param, msetby) != std::string::npos) { argreason = false; const std::string val(param.substr(msetby.length())); args.source = (!val.empty() ? val : "*"); }
			else if (irc::find(param, mset) != std::string::npos) { argreason = false; const std::string val(param.substr(mset.length())); args.set = IsValidDurationString(val[0] == '-' ? val.substr(1) : val) ? val : ""; }
			else if (irc::find(param, mduration) != std::string::npos) { argreason = false; const std::string val(param.substr(mduration.length())); if (val == "0") args.duration = val; else { bool prefixed = val[0] == '-' || val[0] == '+'; args.duration = IsValidDurationString(prefixed ? val.substr(1) : val) ? val : ""; } }
			else if (irc::find(param, mexpires) != std::string::npos) { argreason = false; const std::string val(param.substr(mexpires.length())); args.expires = IsValidDurationString(val[0] == '+' ? val.substr(1) : val) ? val : ""; }
			else { if (argreason) args.reason.append(" " + param); else return false; }
		}
		return true;
	}

	const std::string BuildCriteriaStr(const Criteria& args)
	{
		std::string criteria; const std::string sep(", ");
		if (args.mask != "*") criteria.append("Mask: " + args.mask + sep);
		if (args.reason != "*") criteria.append("Reason: " + args.reason + sep);
		if (args.source != "*") criteria.append("Source: " + args.source + sep);
		if (args.config != MATCH_ANY) criteria.append("From Config: " + std::string(args.config == MATCH_ONLY ? "yes" : "no") + sep);
		if (!args.set.empty()) criteria.append("Set: " + args.set + sep);
		if (!args.duration.empty()) criteria.append("Duration: " + args.duration + sep);
		if (!args.expires.empty()) criteria.append("Expires: " + args.expires + sep);
		if (criteria.empty()) criteria.append("No specific criteria");
		else if (criteria[criteria.length() - 1] == ' ') criteria.erase(criteria.length() - 2, 2);
		return criteria;
	}

	const std::string BuildTypeStr(const std::string& type) { if (type.length() <= 2) return type + "-line"; return type; }
}

class CommandXBase : public SplitCommand
{
	void ProcessLines(LocalUser* user, const Criteria& args, const std::string& linetype, XLineLookup* xlines, unsigned int& matched, unsigned int& total, const bool count, const bool remove)
	{
		total += xlines->size();
		LookupIter safei;
		for (LookupIter i = xlines->begin(); i != xlines->end(); )
		{
			safei = i; ++safei;
			XLine* xline = i->second;

			if ((args.config == MATCH_ONLY && (!xline->from_config && xline->source != "<Config>")) ||
			    (args.config == MATCH_NONE && (xline->from_config || xline->source == "<Config>"))) { i = safei; continue; }

			bool negate = args.mask[0] == '!';
			bool match = InspIRCd::MatchCIDR(xline->Displayable(), (negate ? args.mask.substr(1) : args.mask)) ||
			             InspIRCd::MatchCIDR((negate ? args.mask.substr(1) : args.mask), xline->Displayable());
			if ((negate && match) || (!negate && !match)) { i = safei; continue; }

			negate = args.reason[0] == '!'; match = InspIRCd::Match(xline->reason, (negate ? args.reason.substr(1) : args.reason));
			if ((negate && match) || (!negate && !match)) { i = safei; continue; }

			negate = args.source[0] == '!'; match = InspIRCd::Match(xline->source, (negate ? args.source.substr(1) : args.source));
			if ((negate && match) || (!negate && !match)) { i = safei; continue; }

			if (!args.set.empty())
			{
				bool prefixed = args.set[0] == '-'; unsigned long dur = 0;
				if (!ParseDuration(prefixed ? args.set.substr(1) : args.set, dur)) { i = safei; continue; }
				long set = static_cast<long>(ServerInstance->Time()) - static_cast<long>(dur);
				if ((prefixed && xline->set_time < set) || (!prefixed && xline->set_time > set)) { i = safei; continue; }
			}

			if (!args.duration.empty())
			{
				bool prefixed = args.duration[0] == '+' || args.duration[0] == '-'; unsigned long duration = 0;
				if (!ParseDuration(prefixed ? args.duration.substr(1) : args.duration, duration)) { i = safei; continue; }
				if ((xline->duration == 0 && args.duration != "0") ||
				    (args.duration[0] == '+' && xline->duration <= duration) ||
				    (args.duration[0] == '-' && xline->duration >= duration) ||
				    (!prefixed && xline->duration != duration)) { i = safei; continue; }
			}

			if (!args.expires.empty())
			{
				bool prefixed = args.expires[0] == '+'; unsigned long expires_dur = 0;
				if (!ParseDuration(prefixed ? args.expires.substr(1) : args.expires, expires_dur)) { i = safei; continue; }
				unsigned long expires = ServerInstance->Time() + static_cast<long>(expires_dur);
				if ((xline->duration == 0) ||
				    (prefixed && xline->set_time + xline->duration < expires) ||
				    (!prefixed && xline->set_time + xline->duration > expires)) { i = safei; continue; }
			}

			matched++;
			if (count) { i = safei; continue; }

			const std::string display = xline->Displayable();
			const std::string duration = (xline->duration == 0 ? "permanent" : Duration::ToString(xline->duration));
			const std::string reason = xline->reason;
			const std::string settime = Time::ToString(xline->set_time);

			std::string expires;
			if (xline->duration == 0)
				expires = "doesn't expire";
			else
				expires = std::string("expires in ") + Duration::ToString(xline->expiry - ServerInstance->Time()) + " (on " + Time::ToString(xline->expiry) + ")";

			std::string out;
			if (remove && ServerInstance->XLines->DelLine(display.c_str(), linetype, out, user))
			{
				ServerInstance->SNO.WriteToSnoMask('x', "%s removed %s on %s: %s", user->nick.c_str(), BuildTypeStr(linetype).c_str(), display.c_str(), reason.c_str());
			}
			else
			{
				out = BuildTypeStr(linetype) + " on " + display + " set by " + xline->source + " on " + settime + ", duration '" + duration + "', " + expires + ": " + reason;
				user->WriteNotice(out);
			}

			i = safei;
		}
	}

	bool HandleCmd(LocalUser* user, const Criteria& args, Command* cmd)
	{
		bool count = (cmd->name == "XCOUNT");
		bool remove = (cmd->name == "XREMOVE");
		const std::string action = (remove ? "Removing" : "Listing");
		const std::string criteria = BuildCriteriaStr(args);
		unsigned int matched = 0, total = 0;

		if (args.type == "*")
		{
			if (!count) user->WriteNotice(action + " matches from all X-line types (" + criteria + ")");
			std::vector<std::string> xlinetypes = ServerInstance->XLines->GetAllTypes();
			for (const auto& x : xlinetypes)
			{
				if (remove && !HasCommandPermission(user, x)) { user->WriteNotice(std::string("Skipping type '") + x + "' as your oper type does not have access to remove these."); continue; }
				XLineLookup* xlines = ServerInstance->XLines->GetAll(x);
				if (xlines) ProcessLines(user, args, x, xlines, matched, total, count, remove);
			}
			if (count) user->WriteNotice(std::to_string(matched) + " of " + std::to_string(total) + " X-lines matched (" + criteria + ")");
			else user->WriteNotice(std::string("End of list, ") + std::to_string(matched) + "/" + std::to_string(total) + " X-lines " + (remove ? "removed" : "matched"));
		}
		else
		{
			std::string linetype = args.type; std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);
			if (remove && !HasCommandPermission(user, linetype)) { user->WriteNumeric(ERR_NOPRIVILEGES, std::string(user->nick + " :Permission Denied - your oper type does not have access to remove X-lines of type '" + linetype + "'")); return false; }
			XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
			if (!xlines) { user->WriteNotice(std::string("Invalid X-line type '") + linetype + "' (or not yet used X-line)"); return false; }
			if (xlines->empty()) { user->WriteNotice(std::string("No X-lines of type '") + linetype + "' exist"); return false; }

			if (!count) user->WriteNotice(action + " matches of X-line type '" + linetype + "' (" + criteria + ")");
			ProcessLines(user, args, linetype, xlines, matched, total, count, remove);
			if (count) user->WriteNotice(std::to_string(matched) + " of " + std::to_string(total) + " X-lines of type '" + linetype + "' matched (" + criteria + ")");
			else user->WriteNotice(std::string("End of list, ") + std::to_string(matched) + "/" + std::to_string(total) + " X-lines of type '" + linetype + "' " + (remove ? "removed" : "matched"));
		}
		return true;
	}

 public:
	CommandXBase(Module* Creator, const std::string& cmdname) : SplitCommand(Creator, cmdname, 1)
	{
		syntax = { "-type=<type|*> -mask=[!]<> -reason=[!]<> -source=[!]<> -set=[-]<time> -duration=[-+]<time> -expires=[+]<time> -config=<yes|no>" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (!user->IsOper())
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, std::string(user->nick + " :Permission Denied - this command is for operators only"));
			return CmdResult::FAILURE;
		}

		if (parameters.empty() || parameters[0].empty() || parameters[0][0] != '-' || parameters[0].find('=') == std::string::npos)
		{
			user->WriteNotice(std::string("Incorrect argument syntax \"") + (parameters.empty() ? "" : parameters[0]) + "\"");
			return CmdResult::FAILURE;
		}

		Criteria args("*", "*", "*", "*");
		if (!ProcessArgs(parameters, args))
		{
			user->WriteNotice("There was a problem processing the given arguments");
			return CmdResult::FAILURE;
		}

		if (!HandleCmd(user, args, this)) return CmdResult::FAILURE;
		return CmdResult::SUCCESS;
	}
};

class CommandXCopy : public SplitCommand
{
 public:
	CommandXCopy(Module* Creator) : SplitCommand(Creator, "XCOPY", 3)
	{
		syntax = { "<X-line type> <old mask> <new mask> [-duration=<> -reason=<>]" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		if (!user->IsOper())
		{
			user->WriteNumeric(ERR_NOPRIVILEGES, std::string(user->nick + " :Permission Denied - this command is for operators only"));
			return CmdResult::FAILURE;
		}

		if (parameters.size() < 3) { user->WriteNotice("Usage: /XCOPY <X-line type> <old mask> <new mask> [-duration=<> -reason=<>]"); return CmdResult::FAILURE; }

		const std::string& xtype = parameters[0];
		const std::string& oldmask = parameters[1];
		const std::string& newmask = parameters[2];
		Criteria args;

		if (parameters.size() > 3)
		{
			CommandBase::Params optional(parameters.begin() + 3, parameters.end());
			if (!ProcessArgs(optional, args)) { user->WriteNotice("There was a problem processing the given arguments"); return CmdResult::FAILURE; }
		}

		std::string linetype = xtype; std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);
		if (!HasCommandPermission(user, linetype)) { user->WriteNumeric(ERR_NOPRIVILEGES, std::string(user->nick + " :Permission Denied - your oper type does not have access to copy an X-line of type '" + linetype + "'")); return CmdResult::FAILURE; }

		XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
		if (!xlines) { user->WriteNotice(std::string("Invalid X-line type '") + linetype + "' (or not yet used X-line)"); return CmdResult::FAILURE; }

		XLine* oldxline = NULL;
		for (LookupIter i = xlines->begin(); i != xlines->end(); ++i) { if (irc::equals(i->second->Displayable(), oldmask)) { oldxline = i->second; break; } }
		if (!oldxline) { user->WriteNotice(std::string("Could not find \"") + oldmask + "\" in " + BuildTypeStr(linetype) + "s"); return CmdResult::FAILURE; }

		if ((oldmask.find('!') != std::string::npos && newmask.find('!') == std::string::npos) ||
		    (oldmask.find('!') == std::string::npos && newmask.find('!') != std::string::npos) ||
		    (oldmask.find('@') != std::string::npos && newmask.find('@') == std::string::npos) ||
		    (oldmask.find('@') == std::string::npos && newmask.find('@') != std::string::npos))
		{
			user->WriteNotice("Old and new mask must follow the same format (n!u@h or u@h or h)");
			return CmdResult::FAILURE;
		}

		XLineFactory* xlf = ServerInstance->XLines->GetFactory(linetype);
		if (!xlf) { user->WriteNotice("Great! You just broke the matrix!"); return CmdResult::FAILURE; }

		unsigned long duration = 0;
		if (!args.duration.empty())
		{
			if (!ParseDuration(args.duration[0] == '+' || args.duration[0] == '-' ? args.duration.substr(1) : args.duration, duration))
			{
				user->WriteNotice("Invalid duration string");
				return CmdResult::FAILURE;
			}
		}
		else duration = (oldxline->duration == 0 ? 0 : (oldxline->set_time + oldxline->duration - ServerInstance->Time()));

		const std::string& reason = (!args.reason.empty() ? args.reason : oldxline->reason);
		std::string expires;
		if (duration == 0) expires = "";
		else expires = std::string(", expires in ") + Duration::ToString(duration) + " (on " + Time::ToString(ServerInstance->Time() + duration) + ")";

		XLine* newxline = xlf->Generate(ServerInstance->Time(), duration, user->nick, reason, newmask);
		std::string ret;
		if (ServerInstance->XLines->AddLine(newxline, user))
			ServerInstance->SNO.WriteToSnoMask('x', "%s added %s %s for %s%s: %s", user->nick.c_str(), (duration == 0 ? "permanent" : "timed"), BuildTypeStr(linetype).c_str(), newmask.c_str(), expires.c_str(), reason.c_str());
		else { user->WriteNotice(std::string("Failed to add ") + BuildTypeStr(linetype) + " on \"" + newmask + "\""); delete newxline; return CmdResult::FAILURE; }

		return CmdResult::SUCCESS;
	}
};

class ModuleXLineTools : public Module
{
	CommandXBase xcount;
	CommandXBase xremove;
	CommandXBase xsearch;
	CommandXCopy xcopy;

 public:
	ModuleXLineTools()
		: Module(VF_NONE, "X-line management tools")
		, xcount(this, "XCOUNT")
		, xremove(this, "XREMOVE")
		, xsearch(this, "XSEARCH")
		, xcopy(this)
	{
	}
};

MODULE_INIT(ModuleXLineTools)
