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
#include <cctype>
#include <climits>
#include <ctime>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <vector>

namespace
{
	enum MatchType { MATCH_ONLY, MATCH_NONE, MATCH_ANY };

	/* Small printf-style helper that returns a std::string. */
	std::string Format(const char* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);
		char stackbuf[512];
		va_list ap_copy;
		va_copy(ap_copy, ap);
		int needed = vsnprintf(stackbuf, sizeof(stackbuf), fmt, ap_copy);
		va_end(ap_copy);

		std::string out;
		if (needed < 0) { va_end(ap); return std::string(); }
		if (static_cast<size_t>(needed) < sizeof(stackbuf)) { out.assign(stackbuf, static_cast<size_t>(needed)); va_end(ap); return out; }

		std::vector<char> buf(static_cast<size_t>(needed) + 1);
		va_list ap_copy2;
		va_copy(ap_copy2, ap);
		vsnprintf(buf.data(), buf.size(), fmt, ap_copy2);
		va_end(ap_copy2);

		out.assign(buf.data(), static_cast<size_t>(needed));
		va_end(ap);
		return out;
	}

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

	/* TimeString: format a unix timestamp to "YYYY-MM-DD HH:MM:SS" */
	std::string TimeString(time_t ts)
	{
		char buf[64] = {0};
#if defined(_MSC_VER)
		std::tm tm;
		localtime_s(&tm, &ts);
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
#else
		std::tm tm;
#if defined(_POSIX_VERSION) && !defined(__APPLE__)
		localtime_r(&ts, &tm);
#else
		std::tm* tptr = std::localtime(&ts);
		if (tptr) tm = *tptr;
		else std::memset(&tm, 0, sizeof(tm));
#endif
		std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
#endif
		return std::string(buf);
	}

	/* ParseDuration: parse strings like "1h30m" or "1y2w3d..." into seconds */
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

	std::string DurationString(unsigned long seconds)
	{
		if (seconds == 0) return "0s";
		unsigned long rem = seconds;
		const unsigned long YEAR = 31536000UL, WEEK = 604800UL, DAY = 86400UL, HOUR = 3600UL, MIN = 60UL;
		std::string out;
		auto push = [&](unsigned long v, char unit) { if (v == 0) return; out += std::to_string(v); out.push_back(unit); };
		if (rem >= YEAR) { push(rem / YEAR, 'y'); rem %= YEAR; }
		if (rem >= WEEK) { push(rem / WEEK, 'w'); rem %= WEEK; }
		if (rem >= DAY)  { push(rem / DAY, 'd'); rem %= DAY; }
		if (rem >= HOUR) { push(rem / HOUR, 'h'); rem %= HOUR; }
		if (rem >= MIN)  { push(rem / MIN, 'm'); rem %= MIN; }
		if (rem > 0)     { push(rem, 's'); }
		return out;
	}

	bool IsValidDurationString(const std::string& s)
	{
		if (s.empty()) return false;
		if (s == "0") return true;
		unsigned long dummy = 0;
		return ParseDuration(s, dummy);
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

			std::string expires;
			const std::string display = xline->Displayable();
			const std::string duration = (xline->duration == 0 ? "permanent" : DurationString(xline->duration));
			const std::string reason = xline->reason;
			const std::string settime = TimeString(xline->set_time);
			if (xline->duration == 0) expires = "doesn't expire";
			else expires = Format("expires in %s (on %s)", DurationString(xline->expiry - ServerInstance->Time()).c_str(), TimeString(xline->expiry).c_str());

			std::string ret;
			if (remove && ServerInstance->XLines->DelLine(display.c_str(), linetype, ret, user))
				ServerInstance->SNO.WriteToSnoMask('x', "%s removed %s on %s: %s", user->nick.c_str(), BuildTypeStr(linetype).c_str(), display.c_str(), reason.c_str());
			else
				user->WriteNotice(Format("%s on %s set by %s on %s, duration '%s', %s: %s", BuildTypeStr(linetype).c_str(), display.c_str(), xline->source.c_str(), settime.c_str(), duration.c_str(), expires.c_str(), reason.c_str()));

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
			if (!count) user->WriteNotice(Format("%s matches from all X-line types (%s)", action.c_str(), criteria.c_str()));
			std::vector<std::string> xlinetypes = ServerInstance->XLines->GetAllTypes();
			for (const auto& x : xlinetypes)
			{
				if (remove && !HasCommandPermission(user, x)) { user->WriteNotice(Format("Skipping type '%s' as your oper type does not have access to remove these.", x.c_str())); continue; }
				XLineLookup* xlines = ServerInstance->XLines->GetAll(x);
				if (xlines) ProcessLines(user, args, x, xlines, matched, total, count, remove);
			}
			if (count) user->WriteNotice(Format("%u of %u X-lines matched (%s)", matched, total, criteria.c_str()));
			else user->WriteNotice(Format("End of list, %u/%u X-lines %s", matched, total, (remove ? "removed" : "matched")));
		}
		else
		{
			std::string linetype = args.type; std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);
			if (remove && !HasCommandPermission(user, linetype)) { user->WriteNumeric(ERR_NOPRIVILEGES, Format("%s :Permission Denied - your oper type does not have access to remove X-lines of type '%s'", user->nick.c_str(), linetype.c_str())); return false; }
			XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
			if (!xlines) { user->WriteNotice(Format("Invalid X-line type '%s' (or not yet used X-line)", linetype.c_str())); return false; }
			if (xlines->empty()) { user->WriteNotice(Format("No X-lines of type '%s' exist", linetype.c_str())); return false; }

			if (!count) user->WriteNotice(Format("%s matches of X-line type '%s' (%s)", action.c_str(), linetype.c_str(), criteria.c_str()));
			ProcessLines(user, args, linetype, xlines, matched, total, count, remove);
			if (count) user->WriteNotice(Format("%u of %u X-lines of type '%s' matched (%s)", matched, total, linetype.c_str(), criteria.c_str()));
			else user->WriteNotice(Format("End of list, %u/%u X-lines of type '%s' %s", matched, total, linetype.c_str(), (remove ? "removed" : "matched")));
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
			user->WriteNumeric(ERR_NOPRIVILEGES, Format("%s :Permission Denied - this command is for operators only", user->nick.c_str()));
			return CmdResult::FAILURE;
		}

		if (parameters.empty() || parameters[0].empty() || parameters[0][0] != '-' || parameters[0].find('=') == std::string::npos)
		{
			user->WriteNotice(Format("Incorrect argument syntax \"%s\"", parameters.empty() ? "" : parameters[0].c_str()));
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
			user->WriteNumeric(ERR_NOPRIVILEGES, Format("%s :Permission Denied - this command is for operators only", user->nick.c_str()));
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
		if (!HasCommandPermission(user, linetype)) { user->WriteNumeric(ERR_NOPRIVILEGES, Format("%s :Permission Denied - your oper type does not have access to copy an X-line of type '%s'", user->nick.c_str(), linetype.c_str())); return CmdResult::FAILURE; }

		XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
		if (!xlines) { user->WriteNotice(Format("Invalid X-line type '%s' (or not yet used X-line)", linetype.c_str())); return CmdResult::FAILURE; }

		XLine* oldxline = NULL;
		for (LookupIter i = xlines->begin(); i != xlines->end(); ++i) { if (irc::equals(i->second->Displayable(), oldmask)) { oldxline = i->second; break; } }
		if (!oldxline) { user->WriteNotice(Format("Could not find \"%s\" in %ss", oldmask.c_str(), BuildTypeStr(linetype).c_str())); return CmdResult::FAILURE; }

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
		const std::string expires = (duration == 0 ? "" : Format(", expires in %s (on %s)", DurationString(duration).c_str(), TimeString(ServerInstance->Time() + duration).c_str()));

		XLine* newxline = xlf->Generate(ServerInstance->Time(), duration, user->nick, reason, newmask);
		if (ServerInstance->XLines->AddLine(newxline, user))
			ServerInstance->SNO.WriteToSnoMask('x', "%s added %s %s for %s%s: %s", user->nick.c_str(), (duration == 0 ? "permanent" : "timed"), BuildTypeStr(linetype).c_str(), newmask.c_str(), expires.c_str(), reason.c_str());
		else { user->WriteNotice(Format("Failed to add %s on \"%s\"", BuildTypeStr(linetype).c_str(), newmask.c_str())); delete newxline; return CmdResult::FAILURE; }

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
		: Module(0, "X-line management tools")
		, xcount(this, "XCOUNT")
		, xremove(this, "XREMOVE")
		, xsearch(this, "XSEARCH")
		, xcopy(this)
	{
	}

	std::string GetVersion() const
	{
		return "X-line management tools";
	}
};

MODULE_INIT(ModuleXLineTools)
