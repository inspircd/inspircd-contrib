/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 genius3000 <genius3000@g3k.solutions>
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
/* $ModDesc: X-Line management with XCOUNT, XREMOVE, XSEARCH, and XCOPY */
/* $ModDepends: core 2.0 */

/* XCOUNT, XREMOVE, and XSEARCH are the same except for the end action:
 * XCOUNT will just return a count of matching X-Lines.
 * XREMOVE will remove the matching X-Lines and return that count.
 * XSEARCH will list the matching X-Lines and that count.
 * Syntax is argument and value style "-arg=val" and is the same for each.
 * All arguments are optional:
 * -type=<xline type or *>
 * -mask=<mask> CIDR supported>
 * -reason=<reason> Spaces allowed
 * -source=<source> Nick, server, or "<Config>" ("setby" can be used instead of "source")
 * -set=<time string> (time ago) Prefix with '-' for less than
 * -duration=<time string> (actual set duration) Exact match, prefix '+' for longer than, or '-' for shorter than
 * -expires=<time string> (time ahead) Prefix with '+' for more than
 *
 * XCOPY copies a specified X-Line (by type and mask) to a
 * new X-Line of same type with a new mask.
 * Original reason and expiry time are copied but can be
 * overridden with the 'duration' or 'reason' arguments.
 */

/* Helpop Lines for the COPER section
 * Find: '<helpop key="coper" value="Oper Commands
 * Place 'XCOUNT    XREMOVE   XSEARCH     XCOPY' on a new
 * line after 'FILTER OJOIN'.
 * Find: '<helpop key="clones" ...'
 * Place just above that line:
<helpop key="xcount" value="/XCOUNT -type=<xline type|*> -mask=[!]<m> -reason=[!]<r> -source=[!]<s> -set=[-]<t> -duration=[-+]<t> -expires=[+]<t>

Returns a count of matching X-Lines of the specified type (or all types). Mask(m) supports CIDR, reason(r) can include spaces, source(s)
is a nick, server, or <Config>. 'Prefix your value for these 3 arguments with a '!' to negate the match. 't' is a
time-string (seconds or 1y2w3d4h5m6s) and can be prefixed with '+' or '-' to adjust matching. All arguments are optional.">

<helpop key="xremove" value="/XREMOVE -type=<xline type|*> -mask=[!]<m> -reason=[!]<r> -source=[!]<s> -set=[-]<t> -duration=[-+]<t> -expires=[+]<t>

Removes matching X-Lines of the specified type (or all types). Mask(m) supports CIDR, reason(r) can include spaces, source(s)
is a nick, server, or <Config>. 'Prefix your value for these 3 arguments with a '!' to negate the match. 't' is a
time-string (seconds or 1y2w3d4h5m6s) and can be prefixed with '+' or '-' to adjust matching. All arguments are optional.
Use /XSEARCH to test before removing.">

<helpop key="xsearch" value="/XSEARCH -type=<xline type|*> -mask=[!]<m> -reason=[!]<r> -source=[!]<s> -set=[-]<t> -duration=[-+]<t> -expires=[+]<t>

Lists matching X-Lines of the specified type (or all types). Mask(m) supports CIDR, reason(r) can include spaces, source(s)
is a nick, server, or <Config>. Prefix your value for these 3 arguments with a '!' to negate the match. 't' is a
time-string (seconds or 1y2w3d4h5m6s) and can be prefixed with '+' or '-' to adjust matching. All arguments are optional.">

<helpop key="xcopy" value="/XCOPY <xline type> <old mask> <new mask> [-duration=<> -reason=<>]

Copies the specified X-Line (if found) to a new X-Line. The original reason and expiry time
are copied unless overridden with '-duration=' or '-reason='">


 */

#include "inspircd.h"
#include "xline.h"


struct Criteria
{
	std::string type;
	std::string mask;
	std::string reason;
	std::string source;
	std::string set;
	std::string duration;
	std::string expires;

	Criteria() { }
	Criteria(const std::string& t, const std::string& m, const std::string& r, const std::string& s)
		: type(t)
		, mask(m)
		, reason(r)
		, source(s)
	{
	}
};

bool ProcessArgs(const std::vector<std::string>& params, Criteria& args)
{
	if (!params.size())
		return false;

	// Track arg reason to include space separated params
	bool argreason = false;

	const std::string mtype("-type=");
	const std::string mmask("-mask=");
	const std::string mreason("-reason=");
	const std::string msource("-source=");
	const std::string msetby("-setby=");
	const std::string mset("-set=");
	const std::string mduration("-duration=");
	const std::string mexpires("-expires=");

	for (std::vector<std::string>::const_iterator p = params.begin(); p != params.end(); ++p)
	{
		const std::string param = *p;

		if (param.find(mtype) != std::string::npos)
		{
			argreason = false;
			const std::string& val(param.substr(mtype.length()));
			args.type = (!val.empty() ? val : "*");
		}
		else if (param.find(mmask) != std::string::npos)
		{
			argreason = false;
			const std::string& val(param.substr(mmask.length()));
			args.mask = (!val.empty() ? val : "*");
		}
		else if (param.find(mreason) != std::string::npos)
		{
			argreason = true;
			const std::string& val(param.substr(mreason.length()));
			args.reason = (!val.empty() ? val : "*");
		}
		else if (param.find(msource) != std::string::npos)
		{
			argreason = false;
			const std::string& val(param.substr(msource.length()));
			args.source = (!val.empty() ? val : "*");
		}
		else if (param.find(msetby) != std::string::npos)
		{
			argreason = false;
			const std::string& val(param.substr(msetby.length()));
			args.source = (!val.empty() ? val : "*");
		}
		else if (param.find(mset) != std::string::npos)
		{
			argreason = false;
			const std::string& val = param.substr(mset.length());
			args.set = (ServerInstance->Duration(val) ? val : "");
		}
		else if (param.find(mduration) != std::string::npos)
		{
			argreason = false;
			const std::string& val = param.substr(mduration.length());
			// 0 is a special case here, to match no expires
			if (val == "0")
				args.duration = val;
			else
				args.duration = (ServerInstance->Duration(val) ? val : "");
		}
		else if (param.find(mexpires) != std::string::npos)
		{
			argreason = false;
			const std::string& val = param.substr(mexpires.length());
			args.expires = (ServerInstance->Duration(val) ? val : "");
		}
		else
		{
			if (argreason)
				args.reason.append(" " + param);
			else
				return false;
		}
	}

	return true;
}

const std::string BuildCriteriaStr(const Criteria& args)
{
	std::string criteria;
	const std::string sep(", ");

	if (args.mask != "*")
		criteria.append("Mask: " + args.mask + sep);
	if (args.reason != "*")
		criteria.append("Reason: " + args.reason + sep);
	if (args.source != "*")
		criteria.append("Source: " + args.source + sep);
	if (!args.set.empty())
		criteria.append("Set: " + args.set + sep);
	if (!args.duration.empty())
		criteria.append("Duration: " + args.duration + sep);
	if (!args.expires.empty())
		criteria.append("Expires: " + args.expires + sep);

	if (criteria.empty())
		criteria.append("No specific criteria");
	// Remove trailing separator
	else if (criteria[criteria.length() - 1] == ' ')
		criteria.erase(criteria.length() - 2, 2);

	return criteria;
}

const std::string BuildTypeStr(const std::string& type)
{
	std::string typestr;

	// Core/Vendor *-Lines are single character, some -extras are two
	// Other X-Lines look better without -Lines
	if (type.length() <= 2)
		typestr = type + "-Line";
	else
		typestr = type;

	return typestr;
}

// Snagged this from Anope: https://github.com/anope/anope/blob/2.0/src/misc.cpp
// Modified to return short text format
const std::string BuildDurationStr(time_t t)
{
	/* We first calculate everything */
	time_t years = t / 31536000;
	time_t days = (t / 86400) % 365;
	time_t hours = (t / 3600) % 24;
	time_t minutes = (t / 60) % 60;
	time_t seconds = (t) % 60;

	if (!years && !days && !hours && !minutes)
		return ConvToStr(seconds) + "s";
	else
	{
		std::string buffer;
		if (years)
			buffer = ConvToStr(years) + "y";
		if (days)
			buffer += ConvToStr(days) + "d";
		if (hours)
			buffer += ConvToStr(hours) + "h";
		if (minutes)
			buffer += ConvToStr(minutes) + "m";
		if (seconds)
			buffer += ConvToStr(seconds) + "s";

		return buffer;
	}
}

class CommandXBase : public Command
{
	void ProcessLines(User* user, const Criteria& args, const std::string& linetype, XLineLookup* xlines, unsigned int& matched, unsigned int& total, const bool count, const bool remove)
	{
		total += xlines->size();
		LookupIter safei;

		for (LookupIter i = xlines->begin(); i != xlines->end(); )
		{
			safei = i;
			safei++;

			XLine* xline = i->second;

			// CIDR and glob mask matching, with negation
			if ((args.mask[0] == '!' && InspIRCd::MatchCIDR(xline->Displayable(), args.mask.substr(1))) ||
			    (args.mask[0] != '!' && !InspIRCd::MatchCIDR(xline->Displayable(), args.mask)))
			{
				i = safei;
				continue;
			}

			// Glob reason matching, with negation
			if ((args.reason[0] == '!' && InspIRCd::Match(xline->reason, args.reason.substr(1))) ||
			    (args.reason[0] != '!' && !InspIRCd::Match(xline->reason, args.reason)))
			{
				i = safei;
				continue;
			}

			// Glob source matching, with negation
			if ((args.source[0] == '!' && InspIRCd::Match(xline->source, args.source.substr(1))) ||
			    (args.source[0] != '!' && !InspIRCd::Match(xline->source, args.source)))
			{
				i = safei;
				continue;
			}

			// Set (time ago): Prefix '-' means less than; no prefix means more than; both match exact (to the second)
			if (!args.set.empty())
			{
				long set = ServerInstance->Time() - ServerInstance->Duration(args.set);

				if ((args.set[0] == '-' && xline->set_time < set) ||
				    (args.set[0] != '-' && xline->set_time > set))
				{
					i = safei;
					continue;
				}
			}

			// Duration: Prefix '+' means longer than; '-' means shorter than; no prefix means exact; '0' matches no expiry
			if (!args.duration.empty())
			{
				long duration = ServerInstance->Duration(args.duration);

				if ((xline->duration == 0 && args.duration != "0") ||
				    (args.duration[0] == '+' && xline->duration <= duration) ||
				    (args.duration[0] == '-' && xline->duration >= duration) ||
				    (args.duration.find_first_not_of("+-") == 0 && xline->duration != duration))
				{
					i = safei;
					continue;
				}
			}

			// Expires (time ahead): Prefix '+' means more than; no prefix means less than; both match exact (to the second)
			if (!args.expires.empty())
			{
				long expires = ServerInstance->Time() + ServerInstance->Duration(args.expires);

				if ((xline->duration == 0) ||
				    (args.expires[0] == '+' && xline->set_time + xline->duration < expires) ||
				    (args.expires[0] != '+' && xline->set_time + xline->duration > expires))
				{
					i = safei;
					continue;
				}
			}

			matched++;

			// Skip the rest when just counting matches
			if (count)
			{
				i = safei;
				continue;
			}

			std::string expires;
			const std::string display = xline->Displayable();
			const std::string duration = (xline->duration == 0 ? "permanent" : BuildDurationStr(xline->duration));
			const std::string reason = xline->reason;
			const std::string settime = ServerInstance->TimeString(xline->set_time);
			if (xline->duration == 0)
				expires = "doesn't expire";
			else
				expires = "expires on " + ServerInstance->TimeString(xline->expiry) + " (in " + BuildDurationStr(xline->expiry - ServerInstance->Time()) + ")";

			if (remove && ServerInstance->XLines->DelLine(display.c_str(), linetype, user))
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s removed %s on %s: %s", user->nick.c_str(),
								    BuildTypeStr(linetype).c_str(), display.c_str(), reason.c_str());
			}
			else if (!remove)
			{
				user->WriteServ("NOTICE %s :%s on %s set by %s on %s, duration '%s', %s: %s", user->nick.c_str(),
						BuildTypeStr(linetype).c_str(), display.c_str(), xline->source.c_str(),
						settime.c_str(), duration.c_str(), expires.c_str(), reason.c_str());
			}

			i = safei;
		}
	}

	bool HandleCmd(User* user, const Criteria& args, Command* cmd)
	{
		bool count = (cmd->name == "XCOUNT" ? true : false);
		bool remove = (cmd->name == "XREMOVE" ? true : false);
		const std::string action = (remove ? "Removing" : "Listing");
		const std::string criteria = BuildCriteriaStr(args);
		unsigned int matched = 0;
		unsigned int total = 0;

		if (args.type == "*")
		{
			if (!count)
				user->WriteServ("NOTICE %s :%s matches from all X-Line types (%s)", user->nick.c_str(), action.c_str(), criteria.c_str());

			std::vector<std::string> xlinetypes = ServerInstance->XLines->GetAllTypes();
			for (std::vector<std::string>::const_iterator x = xlinetypes.begin(); x != xlinetypes.end(); ++x)
			{
				XLineLookup* xlines = ServerInstance->XLines->GetAll(*x);
				if (xlines)
					ProcessLines(user, args, *x, xlines, matched, total, count, remove);
			}

			if (count)
				user->WriteServ("NOTICE %s :%u of %u all X-Lines matched (%s)", user->nick.c_str(), matched, total, criteria.c_str());
			else
				user->WriteServ("NOTICE %s :End of list, %u/%u X-Lines %s", user->nick.c_str(), matched, total, (remove ? "removed" : "matched"));
		}
		else
		{
			std::string linetype = args.type;
			std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);

			XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
			if (!xlines)
			{
				user->WriteServ("NOTICE %s :Invalid X-Line type '%s' (or not yet used X-Line)", user->nick.c_str(), linetype.c_str());
				return false;
			}
			if (xlines->empty())
			{
				user->WriteServ("NOTICE %s :No X-Lines of type '%s' exist", user->nick.c_str(), linetype.c_str());
				return false;
			}

			if (!count)
				user->WriteServ("NOTICE %s :%s matches of X-Line type '%s' (%s)", user->nick.c_str(), action.c_str(), linetype.c_str(), criteria.c_str());

			ProcessLines(user, args, linetype, xlines, matched, total, count, remove);

			if (count)
				user->WriteServ("NOTICE %s :%u of %u X-Lines of type '%s' matched (%s)", user->nick.c_str(), matched, total, linetype.c_str(), criteria.c_str());
			else
				user->WriteServ("NOTICE %s :End of list, %u/%u X-Lines of type '%s' %s", user->nick.c_str(), matched, total, linetype.c_str(), (remove ? "removed" : "matched"));
		}

		return true;
	}

 public:
	CommandXBase(Module* Creator, const std::string& cmdname) : Command(Creator, cmdname, 1)
	{
		syntax = "-type=<type|*> -mask=[!]<> -reason=[!]<> -source=[!]<> -set=[-]<time> -duration=[-+]<time> -expires=[+]<time>";
		flags_needed = 'o';
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		if (parameters[0][0] != '-' || parameters[0].find('=') == std::string::npos)
		{
			user->WriteServ("NOTICE %s :Incorrect argument syntax \"%s\"", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		// Initialize type, mask, reason, and source with match-all
		Criteria args("*", "*", "*", "*");
		if (!ProcessArgs(parameters, args))
		{
			user->WriteServ("NOTICE %s :There was a problem processing the given arguments", user->nick.c_str());
			return CMD_FAILURE;
		}

		// Failure message is sent from HandleCmd()
		if (!HandleCmd(user, args, this))
			return CMD_FAILURE;

		return CMD_SUCCESS;
	}
};

class CommandXCopy : public Command
{
 public:
	CommandXCopy(Module* Creator) : Command(Creator, "XCOPY", 3)
	{
		syntax = "<xline type> <old mask> <new mask> [-duration=<> -reason=<>]";
		flags_needed = 'o';
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		const std::string& xtype = parameters[0];
		const std::string& oldmask = parameters[1];
		const std::string& newmask = parameters[2];
		Criteria args;

		if (parameters.size() > 3)
		{
			std::vector<std::string> optional(parameters.begin() + 3, parameters.end());
			if (!ProcessArgs(optional, args))
			{
				user->WriteServ("NOTICE %s :There was a problem processing the given arguments", user->nick.c_str());
				return CMD_FAILURE;
			}
		}

		// Lookup the X-Line matching what we were given
		std::string linetype = xtype;
		std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);

		XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
		if (!xlines)
		{
			user->WriteServ("NOTICE %s :Invalid X-Line type '%s' (or not yet used X-Line)", user->nick.c_str(), linetype.c_str());
			return CMD_FAILURE;
		}

		XLine* oldxline = NULL;
		for (LookupIter i = xlines->begin(); i != xlines->end(); ++i)
		{
			if (i->second->Displayable() == oldmask)
			{
				oldxline = i->second;
				break;
			}
		}

		if (!oldxline)
		{
			user->WriteServ("NOTICE %s :Could not find \"%s\" in %ss", user->nick.c_str(), oldmask.c_str(), BuildTypeStr(linetype).c_str());
			return CMD_FAILURE;
		}

		// We don't know the proper mask format for this, so we compare it with the old one
		if ((oldmask.find('!') != std::string::npos && newmask.find('!') == std::string::npos) ||
		    (oldmask.find('!') == std::string::npos && newmask.find('!') != std::string::npos) ||
		    (oldmask.find('@') != std::string::npos && newmask.find('@') == std::string::npos) ||
		    (oldmask.find('@') == std::string::npos && newmask.find('@') != std::string::npos))
		{
			user->WriteServ("NOTICE %s :Old and new mask must follow the same format (n!u@h or u@h or h)", user->nick.c_str());
			return CMD_FAILURE;
		}

		// Get the needed X-Line Factory and create the new X-Line
		// Snagged this method from m_xline_db
		XLineFactory* xlf = ServerInstance->XLines->GetFactory(linetype);
		if (!xlf)
		{
			user->WriteServ("NOTICE %s :Great! You just broke the matrix!", user->nick.c_str());
			return CMD_FAILURE;
		}

		unsigned long duration;
		if (!args.duration.empty())
			duration = ServerInstance->Duration(args.duration);
		else
			duration = (oldxline->duration == 0 ? 0 : (oldxline->set_time + oldxline->duration - ServerInstance->Time()));

		const std::string& reason = (!args.reason.empty() ? args.reason : oldxline->reason);
		const std::string expires = (duration == 0 ? "" : (", to expire on " + ServerInstance->TimeString(ServerInstance->Time() + duration)));

		XLine* newxline = xlf->Generate(ServerInstance->Time(), duration, user->nick, reason, newmask);
		if (ServerInstance->XLines->AddLine(newxline, user))
			ServerInstance->SNO->WriteToSnoMask('x', "%s added %s %s on %s%s: %s", user->nick.c_str(),
				(duration == 0 ? "permanent" : "timed"), BuildTypeStr(linetype).c_str(),
				newmask.c_str(), expires.c_str(), reason.c_str());
		else
		{
			user->WriteServ("NOTICE %s :Failed to add %s on \"%s\"", user->nick.c_str(), BuildTypeStr(linetype).c_str(), newmask.c_str());
			delete newxline;
			return CMD_FAILURE;
		}

		return CMD_SUCCESS;
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
		: xcount(this, "XCOUNT")
		, xremove(this, "XREMOVE")
		, xsearch(this, "XSEARCH")
		, xcopy(this)
	{
	}

	void init()
	{
		ServiceProvider* providerlist[] = { &xcount, &xremove, &xsearch, &xcopy };
		ServerInstance->Modules->AddServices(providerlist, sizeof(providerlist)/sizeof(ServiceProvider*));
	}

	virtual Version GetVersion()
	{
		return Version("X-Line management tools", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleXLineTools)
