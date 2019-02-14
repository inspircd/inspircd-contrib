/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Modified (:D) 2017 Dan39 <ddan39@gmail.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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


#include "inspircd.h"
#include "xline.h"

/* $ModDesc: Provides the /STUN command, which stops a user from receiving messages/notices to channels */
/* $ModDepends: core 2.0 */

class Stun : public XLine
{
 public:
	std::string matchtext;

	Stun(time_t s_time, long d, const std::string& src, const std::string& re, const std::string& stunmask)
		: XLine(s_time, d, src, re, "STUN")
		, matchtext(stunmask)
	{
	}

	~Stun()
	{
	}

	bool Matches(User *u)
	{
		// E: overrides stun
		if (u->exempt)
			return false;

		if (InspIRCd::Match(u->GetFullHost(), matchtext) || InspIRCd::Match(u->GetFullRealHost(), matchtext) || InspIRCd::Match(u->nick+"!"+u->ident+"@"+u->GetIPString(), matchtext))
			return true;

		return false;
	}

	bool Matches(const std::string &s)
	{
		if (matchtext == s)
			return true;
		return false;
	}

	void DisplayExpiry()
	{
		ServerInstance->SNO->WriteToSnoMask('x',"Removing expired stun %s (set by %s %ld seconds ago)",
			this->matchtext.c_str(), this->source.c_str(), (long int)(ServerInstance->Time() - this->set_time));
	}

	const char* Displayable()
	{
		return matchtext.c_str();
	}
};

/** An XLineFactory specialized to generate stun pointers
 */
class StunFactory : public XLineFactory
{
 public:
	StunFactory() : XLineFactory("STUN") { }

	/** Generate a stun
 	*/
	XLine* Generate(time_t set_time, long duration, std::string source, std::string reason, std::string xline_specific_mask)
	{
		return new Stun(set_time, duration, source, reason, xline_specific_mask);
	}

	bool AutoApplyToUserList(XLine *x)
	{
		return false;
	}
};

//typedef std::vector<Stun> stunlist;

class CommandStun : public Command
{
 public:
	CommandStun(Module* Creator) : Command(Creator, "STUN", 1, 3)
	{
		flags_needed = 'o'; this->syntax = "<nick!user@hostmask> [<stun-duration>] :<reason>";
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		/* syntax: STUN nick!user@host time :reason goes here */
		/* 'time' is a human-readable timestring, like 2d3h2s. */

		std::string target = parameters[0];
		
		User *find = ServerInstance->FindNick(target);
		if ((find) && (find->registered == REG_ALL))
			target = std::string("*!*@") + find->GetIPString();

		if (parameters.size() == 1)
		{
			if (ServerInstance->XLines->DelLine(target.c_str(), "STUN", user))
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s removed STUN on %s",user->nick.c_str(),target.c_str());
			}
			else
			{
				user->WriteServ("NOTICE %s :*** Stun %s not found in list, try /stats K.",user->nick.c_str(),target.c_str());
				return CMD_FAILURE;
			}
		}
		else
		{
			// Adding - XXX todo make this respect <insane> tag perhaps..
			long duration;
			std::string expr;
			if (parameters.size() > 2)
			{
				duration = ServerInstance->Duration(parameters[1]);
				expr = parameters[2];
			}
			else
			{
				duration = 0;
				expr = parameters[1];
			}

			Stun* r = new Stun(ServerInstance->Time(), duration, user->nick.c_str(), expr.c_str(), target.c_str());
			if (ServerInstance->XLines->AddLine(r, user))
			{
				if (!duration)
				{
					ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent STUN for %s: %s",
						user->nick.c_str(), target.c_str(), expr.c_str());
				}
				else
				{
					time_t c_requires_crap = duration + ServerInstance->Time();
					std::string timestr = ServerInstance->TimeString(c_requires_crap);
					ServerInstance->SNO->WriteToSnoMask('x', "%s added timed STUN for %s to expire on %s: %s",
						user->nick.c_str(), target.c_str(), timestr.c_str(), expr.c_str());
				}
			}
			else
			{
				delete r;
				user->WriteServ("NOTICE %s :*** Stun for %s already exists", user->nick.c_str(), target.c_str());
				return CMD_FAILURE;
			}
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (IS_LOCAL(user))
			return ROUTE_LOCALONLY; // spanningtree will send ADDLINE

		return ROUTE_BROADCAST;
	}
};

class ModuleStun : public Module
{
	CommandStun cmd;
	StunFactory f;
	bool affectopers;

	std::string deaf_bypasschars;
	std::string deaf_bypasschars_uline;

 public:
	ModuleStun() : cmd(this)
	{
	}

	void init()
	{
		ServerInstance->XLines->RegisterFactory(&f);
		ServerInstance->Modules->AddService(cmd);

		Implementation eventlist[] = { I_OnStats, I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	virtual ~ModuleStun()
	{
		ServerInstance->XLines->DelAll("STUN");
		ServerInstance->XLines->UnregisterFactory(&f);
	}

	/*
	 * I dont think i need this for OnUserPreNotice and OnUserPreMessage...?
	 *
	void Prioritize()
	{
		Module* alias = ServerInstance->Modules->Find("m_alias.so");
		ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_BEFORE, &alias);
	}
	*/

	virtual ModResult OnStats(char symbol, User* user, string_list& out)
	{
		if (symbol != 'K')
			return MOD_RES_PASSTHRU;

		ServerInstance->XLines->InvokeStats("STUN", 223, user, out);
		return MOD_RES_DENY;
	}

	virtual void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("stun");

		deaf_bypasschars = tag->getString("bypasschars");
		deaf_bypasschars_uline = tag->getString("bypasscharsuline");

		affectopers = tag->getBool("affectopers", false);
	}

	/*
	 * The rest of the code is mostly taken from m_deaf.so
	 * Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
	 * in addition to same copyrights at top
	 * 
	 */
	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			Channel* chan = (Channel*)dest;
			if (chan)
				this->BuildDeafList(MSG_NOTICE, chan, user, status, text, exempt_list);
		}

		return MOD_RES_PASSTHRU;
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
		{
			Channel* chan = (Channel*)dest;
			if (chan)
				this->BuildDeafList(MSG_PRIVMSG, chan, user, status, text, exempt_list);
		}

		return MOD_RES_PASSTHRU;
	}

	virtual void BuildDeafList(MessageType message_type, Channel* chan, User* sender, char status, const std::string &text, CUList &exempt_list)
	{
		const UserMembList *ulist = chan->GetUsers();
		bool is_a_uline;
		bool is_bypasschar, is_bypasschar_avail;
		bool is_bypasschar_uline, is_bypasschar_uline_avail;

		is_bypasschar = is_bypasschar_avail = is_bypasschar_uline = is_bypasschar_uline_avail = 0;
		if (!deaf_bypasschars.empty())
		{
			is_bypasschar_avail = 1;
			if (deaf_bypasschars.find(text[0], 0) != std::string::npos)
				is_bypasschar = 1;
		}
		if (!deaf_bypasschars_uline.empty())
		{
			is_bypasschar_uline_avail = 1;
			if (deaf_bypasschars_uline.find(text[0], 0) != std::string::npos)
				is_bypasschar_uline = 1;
		}

		/*
		 * If we have no bypasschars_uline in config, and this is a bypasschar (regular)
		 * Than it is obviously going to get through +d, no build required
		 */
		if (!is_bypasschar_uline_avail && is_bypasschar)
			return;

		for (UserMembCIter i = ulist->begin(); i != ulist->end(); i++)
		{
			/* Not stunned, don't touch. */
			if (!ServerInstance->XLines->MatchesLine("STUN", i->first))
				continue;

			/* Don't do anything if the user is an operator and affectopers isn't set */
			if (!affectopers && IS_OPER(i->first))
				continue;

			/* matched both U-line only and regular bypasses */
			if (is_bypasschar && is_bypasschar_uline)
				continue; /* deliver message */

			is_a_uline = ServerInstance->ULine(i->first->server);
			/* matched a U-line only bypass */
			if (is_bypasschar_uline && is_a_uline)
				continue; /* deliver message */
			/* matched a regular bypass */
			if (is_bypasschar && !is_a_uline)
				continue; /* deliver message */

			if (status && !strchr(chan->GetAllPrefixChars(i->first), status))
				continue;

			/* don't deliver message! */
			exempt_list.insert(i->first);
		}
	}

	virtual Version GetVersion()
	{
		return Version("Provides the /STUN command, which stops a user from receiving messages/notices to channels",VF_VENDOR|VF_COMMON);
	}
};

MODULE_INIT(ModuleStun)
