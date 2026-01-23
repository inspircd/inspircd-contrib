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
/// $ModDepends: core 4
/// $ModDesc: X-line management with XCOPY, XCOUNT, XREMOVE, and XSEARCH

#include "inspircd.h"
#include "xline.h"
#include "timeutils.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    enum MatchType
    {
        MATCH_ONLY,
        MATCH_NONE,
        MATCH_ANY
    };

    struct Criteria
    {
        MatchType config;
        std::string type;
        std::string mask;
        std::string reason;
        std::string source;
        std::string set;
        std::string duration;
        std::string expires;

        Criteria()
            : config(MATCH_ANY)
        {
        }

        Criteria(const std::string& t, const std::string& m, const std::string& r, const std::string& s)
            : config(MATCH_ANY)
            , type(t)
            , mask(m)
            , reason(r)
            , source(s)
        {
        }
    };

    bool HasCommandPermission(LocalUser* user, std::string type)
    {
        if (type.length() <= 2)
            type.append("LINE");
        return user->HasCommandPermission(type);
    }

    // Parser simple compatible con InspIRCd 4.8.0
    bool ParseSimpleDuration(const std::string& s, unsigned long& out)
    {
        if (s.empty())
            return false;

        char suffix = s.back();
        unsigned long multiplier = 1;
        std::string number = s;

        switch (suffix)
        {
            case 's': multiplier = 1; number = s.substr(0, s.size() - 1); break;
            case 'm': multiplier = 60; number = s.substr(0, s.size() - 1); break;
            case 'h': multiplier = 3600; number = s.substr(0, s.size() - 1); break;
            case 'd': multiplier = 86400; number = s.substr(0, s.size() - 1); break;
            case 'w': multiplier = 604800; number = s.substr(0, s.size() - 1); break;
            default:
                multiplier = 1;
                number = s;
                break;
        }

        for (char c : number)
        {
            if (!isdigit(c))
                return false;
        }

        unsigned long base = strtoul(number.c_str(), nullptr, 10);
        out = base * multiplier;
        return true;
    }

    bool ProcessArgs(const CommandBase::Params& params, Criteria& args)
    {
        if (params.empty())
            return false;

        bool argreason = false;

        const std::string mconfig("-config=");
        const std::string mtype("-type=");
        const std::string mmask("-mask=");
        const std::string mreason("-reason=");
        const std::string msource("-source=");
        const std::string msetby("-setby=");
        const std::string mset("-set=");
        const std::string mduration("-duration=");
        const std::string mexpires("-expires=");

        for (const auto& param : params)
        {
            if (irc::find(param, mconfig) != std::string::npos)
            {
                argreason = false;
                const std::string val(param.substr(mconfig.length()));
                if (irc::equals(val, "yes") || irc::equals(val, "true"))
                    args.config = MATCH_ONLY;
                else if (irc::equals(val, "no") || irc::equals(val, "false"))
                    args.config = MATCH_NONE;
            }
            else if (irc::find(param, mtype) != std::string::npos)
            {
                argreason = false;
                const std::string val(param.substr(mtype.length()));
                args.type = (!val.empty() ? val : "*");
            }
            else if (irc::find(param, mmask) != std::string::npos)
            {
                argreason = false;
                const std::string val(param.substr(mmask.length()));
                args.mask = (!val.empty() ? val : "*");
            }
            else if (irc::find(param, mreason) != std::string::npos)
            {
                argreason = true;
                const std::string val(param.substr(mreason.length()));
                args.reason = (!val.empty() ? val : "*");
            }
            else if (irc::find(param, msource) != std::string::npos)
            {
                argreason = false;
                const std::string val(param.substr(msource.length()));
                args.source = (!val.empty() ? val : "*");
            }
            else if (irc::find(param, msetby) != std::string::npos)
            {
                argreason = false;
                const std::string val(param.substr(msetby.length()));
                args.source = (!val.empty() ? val : "*");
            }
            else if (irc::find(param, mset) != std::string::npos)
            {
                argreason = false;
                const std::string val(param.substr(mset.length()));

                if (val.empty())
                {
                    args.set.clear();
                }
                else
                {
                    unsigned long dummy = 0;
                    std::string raw = (val[0] == '-' ? val.substr(1) : val);

                    if (ParseSimpleDuration(raw, dummy))
                        args.set = val;
                    else
                        args.set.clear();
                }
            }
            else if (irc::find(param, mduration) != std::string::npos)
            {
                argreason = false;
                const std::string val(param.substr(mduration.length()));

                if (val == "0")
                {
                    args.duration = val;
                }
                else
                {
                    unsigned long dummy = 0;
                    std::string raw = ((val[0] == '+' || val[0] == '-') ? val.substr(1) : val);

                    if (ParseSimpleDuration(raw, dummy))
                        args.duration = val;
                    else
                        args.duration.clear();
                }
            }
            else if (irc::find(param, mexpires) != std::string::npos)
            {
                argreason = false;
                const std::string val(param.substr(mexpires.length()));

                if (val.empty())
                {
                    args.expires.clear();
                }
                else
                {
                    unsigned long dummy = 0;
                    std::string raw = (val[0] == '+' ? val.substr(1) : val);

                    if (ParseSimpleDuration(raw, dummy))
                        args.expires = val;
                    else
                        args.expires.clear();
                }
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

    std::string BuildCriteriaStr(const Criteria& args)
    {
        std::string criteria;
        const std::string sep(", ");

        if (args.mask != "*")
            criteria.append("Mask: " + args.mask + sep);
        if (args.reason != "*")
            criteria.append("Reason: " + args.reason + sep);
        if (args.source != "*")
            criteria.append("Source: " + args.source + sep);
        if (args.config != MATCH_ANY)
            criteria.append("From Config: " + std::string(args.config == MATCH_ONLY ? "yes" : "no") + sep);
        if (!args.set.empty())
            criteria.append("Set: " + args.set + sep);
        if (!args.duration.empty())
            criteria.append("Duration: " + args.duration + sep);
        if (!args.expires.empty())
            criteria.append("Expires: " + args.expires + sep);

        if (criteria.empty())
            criteria.append("No specific criteria");
        else if (criteria[criteria.length() - 1] == ' ')
            criteria.erase(criteria.length() - 2, 2);

        return criteria;
    }

    std::string BuildTypeStr(const std::string& type)
    {
        if (type.length() <= 2)
            return type + "-line";
        return type;
    }
}

class CommandXBase : public SplitCommand
{
    void ProcessLines(LocalUser* user, const Criteria& args, const std::string& linetype,
                      XLineLookup* xlines, unsigned int& matched, unsigned int& total,
                      const bool count, const bool remove)
    {
        total += xlines->size();

        LookupIter safei;
        for (LookupIter i = xlines->begin(); i != xlines->end(); )
        {
            safei = i;
            ++safei;

            XLine* xline = i->second;

            // Config-only / non-config filters
            if ((args.config == MATCH_ONLY && (!xline->from_config && xline->source != "<Config>"))
             || (args.config == MATCH_NONE && (xline->from_config || xline->source == "<Config>")))
            {
                i = safei;
                continue;
            }

            // Mask
            bool negate = !args.mask.empty() && args.mask[0] == '!';
            const std::string mask = (negate ? args.mask.substr(1) : args.mask);
            bool match = InspIRCd::MatchCIDR(xline->Displayable(), mask)
                      || InspIRCd::MatchCIDR(mask, xline->Displayable());
            if ((negate && match) || (!negate && !match))
            {
                i = safei;
                continue;
            }

            // Reason
            negate = !args.reason.empty() && args.reason[0] == '!';
            const std::string reasonmask = (negate ? args.reason.substr(1) : args.reason);
            match = InspIRCd::Match(xline->reason, reasonmask);
            if ((negate && match) || (!negate && !match))
            {
                i = safei;
                continue;
            }

            // Source
            negate = !args.source.empty() && args.source[0] == '!';
            const std::string sourcemask = (negate ? args.source.substr(1) : args.source);
            match = InspIRCd::Match(xline->source, sourcemask);
            if ((negate && match) || (!negate && !match))
            {
                i = safei;
                continue;
            }

            // Set time filter
            if (!args.set.empty())
            {
                bool prefixed = args.set[0] == '-';
                unsigned long dur = 0;
                std::string raw = (prefixed ? args.set.substr(1) : args.set);

                if (!ParseSimpleDuration(raw, dur))
                {
                    i = safei;
                    continue;
                }

                long set = static_cast<long>(ServerInstance->Time()) - static_cast<long>(dur);
                if ((prefixed && xline->set_time < set) || (!prefixed && xline->set_time > set))
                {
                    i = safei;
                    continue;
                }
            }

            // Duration filter
            if (!args.duration.empty())
            {
                bool prefixed = args.duration[0] == '+' || args.duration[0] == '-';
                unsigned long duration = 0;
                std::string raw = (prefixed ? args.duration.substr(1) : args.duration);

                if (!ParseSimpleDuration(raw, duration))
                {
                    i = safei;
                    continue;
                }

                if ((xline->duration == 0 && args.duration != "0")
                 || (args.duration[0] == '+' && xline->duration <= duration)
                 || (args.duration[0] == '-' && xline->duration >= duration)
                 || (!prefixed && xline->duration != duration))
                {
                    i = safei;
                    continue;
                }
            }

            // Expires filter
            if (!args.expires.empty())
            {
                bool prefixed = args.expires[0] == '+';
                unsigned long expires_dur = 0;
                std::string raw = (prefixed ? args.expires.substr(1) : args.expires);

                if (!ParseSimpleDuration(raw, expires_dur))
                {
                    i = safei;
                    continue;
                }

                unsigned long expires = ServerInstance->Time() + static_cast<long>(expires_dur);
                if ((xline->duration == 0)
                 || (prefixed && xline->set_time + xline->duration < expires)
                 || (!prefixed && xline->set_time + xline->duration > expires))
                {
                    i = safei;
                    continue;
                }
            }

            ++matched;

            if (count)
            {
                i = safei;
                continue;
            }

            const std::string display = xline->Displayable();
            const std::string duration = (xline->duration == 0 ? "permanent" : Duration::ToString(xline->duration));
            const std::string reason = xline->reason;
            const std::string settime = Time::ToString(xline->set_time);

            std::string expires;
            if (xline->duration == 0)
                expires = "doesn't expire";
            else
                expires = INSP_FORMAT("expires in {} (on {})",
                    Duration::ToString(xline->expiry - ServerInstance->Time()),
                    Time::ToString(xline->expiry));

            if (remove)
            {
                std::string out;
                if (ServerInstance->XLines->DelLine(display.c_str(), linetype, out, user))
                {
                    ServerInstance->SNO.WriteToSnoMask('x',
                        INSP_FORMAT("{} removed {} on {}: {}",
                            user->nick,
                            BuildTypeStr(linetype),
                            display,
                            reason));
                }
            }
            else
            {
                user->WriteNotice(INSP_FORMAT(
                    "{} on {} set by {} on {}, duration '{}', {}: {}",
                    BuildTypeStr(linetype),
                    display,
                    xline->source,
                    settime,
                    duration,
                    expires,
                    reason));
            }

            i = safei;
        }
    }

    bool HandleCmd(LocalUser* user, const Criteria& args, Command* cmd)
    {
        const bool count = (cmd->name == "XCOUNT");
        const bool remove = (cmd->name == "XREMOVE");

        const std::string action = (remove ? "Removing" : "Listing");
        const std::string criteria = BuildCriteriaStr(args);

        unsigned int matched = 0;
        unsigned int total = 0;

        if (args.type == "*")
        {
            if (!count)
            {
                user->WriteNotice(INSP_FORMAT(
                    "{} matches from all X-line types ({})",
                    action,
                    criteria));
            }

            std::vector<std::string> xlinetypes = ServerInstance->XLines->GetAllTypes();
            for (const auto& x : xlinetypes)
            {
                if (remove && !HasCommandPermission(user, x))
                {
                    user->WriteNotice(INSP_FORMAT(
                        "Skipping type '{}' as your oper type does not have access to remove these.",
                        x));
                    continue;
                }

                XLineLookup* xlines = ServerInstance->XLines->GetAll(x);
                if (xlines)
                    ProcessLines(user, args, x, xlines, matched, total, count, remove);
            }

            if (count)
            {
                user->WriteNotice(INSP_FORMAT(
                    "{} of {} X-lines matched ({})",
                    matched,
                    total,
                    criteria));
            }
            else
            {
                user->WriteNotice(INSP_FORMAT(
                    "End of list, {}/{} X-lines {}",
                    matched,
                    total,
                    (remove ? "removed" : "matched")));
            }
        }
        else
        {
            std::string linetype = args.type;
            std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);

            if (remove && !HasCommandPermission(user, linetype))
            {
                user->WriteNumeric(ERR_NOPRIVILEGES,
                    "Permission Denied - your oper type does not have access to remove X-lines of this type");
                return false;
            }

            XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
            if (!xlines)
            {
                user->WriteNotice(INSP_FORMAT(
                    "Invalid X-line type '{}' (or not yet used X-line)",
                    linetype));
                return false;
            }

            if (xlines->empty())
            {
                user->WriteNotice(INSP_FORMAT(
                    "No X-lines of type '{}' exist",
                    linetype));
                return false;
            }

            if (!count)
            {
                user->WriteNotice(INSP_FORMAT(
                    "{} matches of X-line type '{}' ({})",
                    action,
                    linetype,
                    criteria));
            }

		    ProcessLines(user, args, linetype, xlines, matched, total, count, remove);

            if (count)
            {
                user->WriteNotice(INSP_FORMAT(
                    "{} of {} X-lines of type '{}' matched ({})",
                    matched,
                    total,
                    linetype,
                    criteria));
            }
            else
            {
                user->WriteNotice(INSP_FORMAT(
                    "End of list, {}/{} X-lines of type '{}' {}",
                    matched,
                    total,
                    linetype,
                    (remove ? "removed" : "matched")));
            }
        }

        return true;
    }

public:
    CommandXBase(Module* Creator, const std::string& cmdname)
        : SplitCommand(Creator, cmdname, 1)
    {
        syntax = {
            "-type=<type|*> -mask=[!]<mask> -reason=[!]<reason> "
            "-source=[!]<source> -set=[-]<time> -duration=[-+]<time> "
            "-expires=[+]<time> -config=<yes|no>"
        };
    }

    CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
    {
        if (!user->IsOper())
        {
            user->WriteNumeric(ERR_NOPRIVILEGES,
                "Permission Denied - this command is for operators only");
            return CmdResult::FAILURE;
        }

        if (parameters.empty() || parameters[0].empty()
            || parameters[0][0] != '-' || parameters[0].find('=') == std::string::npos)
        {
            user->WriteNotice(INSP_FORMAT(
                "Incorrect argument syntax \"{}\"",
                (parameters.empty() ? std::string() : parameters[0])));
            return CmdResult::FAILURE;
        }

        Criteria args("*", "*", "*", "*");
        if (!ProcessArgs(parameters, args))
        {
            user->WriteNotice("There was a problem processing the given arguments");
            return CmdResult::FAILURE;
        }

        if (!HandleCmd(user, args, this))
            return CmdResult::FAILURE;

        return CmdResult::SUCCESS;
    }
};

class CommandXCopy : public SplitCommand
{
public:
    CommandXCopy(Module* Creator)
        : SplitCommand(Creator, "XCOPY", 3)
    {
        syntax = {
            "<X-line type> <old mask> <new mask> "
            "[-duration=<time> -reason=<reason>]"
        };
    }

    CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
    {
        if (!user->IsOper())
        {
            user->WriteNumeric(ERR_NOPRIVILEGES,
                "Permission Denied - this command is for operators only");
            return CmdResult::FAILURE;
        }

        if (parameters.size() < 3)
        {
            user->WriteNotice("Usage: /XCOPY <X-line type> <old mask> <new mask> "
                "[-duration=<time> -reason=<reason>]");
            return CmdResult::FAILURE;
        }

        const std::string& xtype   = parameters[0];
        const std::string& oldmask = parameters[1];
        const std::string& newmask = parameters[2];

        Criteria args;
        if (parameters.size() > 3)
        {
            CommandBase::Params optional(parameters.begin() + 3, parameters.end());
            if (!ProcessArgs(optional, args))
            {
                user->WriteNotice("There was a problem processing the given arguments");
                return CmdResult::FAILURE;
            }
        }

        std::string linetype = xtype;
        std::transform(linetype.begin(), linetype.end(), linetype.begin(), ::toupper);

        if (!HasCommandPermission(user, linetype))
        {
            user->WriteNumeric(ERR_NOPRIVILEGES,
                "Permission Denied - your oper type does not have access to copy an X-line of this type");
            return CmdResult::FAILURE;
        }

        XLineLookup* xlines = ServerInstance->XLines->GetAll(linetype);
        if (!xlines)
        {
            user->WriteNotice(INSP_FORMAT(
                "Invalid X-line type '{}' (or not yet used X-line)",
                linetype));
            return CmdResult::FAILURE;
        }

        XLine* oldxline = nullptr;
        for (LookupIter i = xlines->begin(); i != xlines->end(); ++i)
        {
            if (irc::equals(i->second->Displayable(), oldmask))
            {
                oldxline = i->second;
                break;
            }
        }

        if (!oldxline)
        {
            user->WriteNotice(INSP_FORMAT(
                "Could not find \"{}\" in {}s",
                oldmask,
                BuildTypeStr(linetype)));
            return CmdResult::FAILURE;
        }

        if ((oldmask.find('!') != std::string::npos && newmask.find('!') == std::string::npos)
         || (oldmask.find('!') == std::string::npos && newmask.find('!') != std::string::npos)
         || (oldmask.find('@') != std::string::npos && newmask.find('@') == std::string::npos)
         || (oldmask.find('@') == std::string::npos && newmask.find('@') != std::string::npos))
        {
            user->WriteNotice("Old and new mask must follow the same format (n!u@h or u@h or h)");
            return CmdResult::FAILURE;
        }

        XLineFactory* xlf = ServerInstance->XLines->GetFactory(linetype);
        if (!xlf)
        {
            user->WriteNotice("Great! You just broke the matrix!");
            return CmdResult::FAILURE;
        }

        unsigned long duration = 0;
        if (!args.duration.empty())
        {
            bool prefixed = args.duration[0] == '+' || args.duration[0] == '-';
            std::string raw = (prefixed ? args.duration.substr(1) : args.duration);

            if (!ParseSimpleDuration(raw, duration))
            {
                user->WriteNotice("Invalid duration string");
                return CmdResult::FAILURE;
            }
        }
        else
        {
            duration = (oldxline->duration == 0
                ? 0
                : (oldxline->set_time + oldxline->duration - ServerInstance->Time()));
        }

        const std::string& reason = (!args.reason.empty() ? args.reason : oldxline->reason);

        std::string expires;
        if (duration == 0)
        {
            expires.clear();
        }
        else
        {
            expires = INSP_FORMAT(
                ", expires in {} (on {})",
                Duration::ToString(duration),
                Time::ToString(ServerInstance->Time() + duration));
        }

        XLine* newxline = xlf->Generate(ServerInstance->Time(), duration, user->nick, reason, newmask);
        if (ServerInstance->XLines->AddLine(newxline, user))
        {
            ServerInstance->SNO.WriteToSnoMask('x',
                INSP_FORMAT("{} added {} {} for {}{}: {}",
                    user->nick,
                    (duration == 0 ? "permanent" : "timed"),
                    BuildTypeStr(linetype),
                    newmask,
                    expires,
                    reason));
        }
        else
        {
            user->WriteNotice(INSP_FORMAT(
                "Failed to add {} on \"{}\"",
                BuildTypeStr(linetype),
                newmask));
            delete newxline;
            return CmdResult::FAILURE;
        }

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
