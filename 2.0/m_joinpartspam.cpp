/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016-2018 Matt Schatz <genius3000@g3k.solutions>
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
/* $ModDesc: Adds channel mode +x to block a user after x per y joins and parts/quits (join/part spam) */
/* $ModDepends: core 2.0 */
/* $ModConfig: <joinpartspam allowredirect="yes" freeredirect="yes"> */

/* Set 'allowredirect' to "yes" to allow channel redirection. Defaults to no.
 * Set 'freeredirect' to "yes" to skip the channel operator check. Defaults to no.
 * You can remove a block on a user with a /INVITE
 */

/* Helpop Lines for the CHMODES section
 * Find: '<helpop key="chmodes" value="Channel Modes'
 * Place just above the 'z     Blocks non-SSL...' line:
 x <cycles>:<sec>:<block>[:#channel] Blocks a user after the set
                    number of Join and Part/Quit cycles in the set
                    seconds for the given block duration (seconds).
                    An optional redirect channel can be set.
 */

/* Special thanks to Attila for the patience and guidance in
 * fixing up some of my initial, poor methods.
 */

#include "inspircd.h"


enum
{
	RPL_CHANBLOCKED = 545,
	ERR_INVALIDMODEPARAM = 696
};

class joinpartspamsettings
{
	struct Tracking
	{
		time_t reset;
		unsigned int counter;
		Tracking() : reset(0), counter(0) { }
	};

	std::map<std::string, Tracking> cycler;
	std::map<std::string, time_t> blocked;
	time_t lastcleanup;

 public:
	unsigned int cycles;
	unsigned int secs;
	unsigned int block;
	std::string redirect;

	joinpartspamsettings(unsigned int c, unsigned int s, unsigned int b, std::string& r)
		: lastcleanup(ServerInstance->Time())
		, cycles(c)
		, secs(s)
		, block(b)
		, redirect(r)
	{
	}

	// Called by PostJoin to possibly reset a cycler's Tracking and increment the counter
	void addcycle(const std::string& mask)
	{
		/* If mask isn't already tracked, set reset time
		 * If tracked and reset time is up, reset counter and reset time
		 * Also assume another server blocked, with the block timing out or a
		 * user removed it if counter >= cycles, reset counter and reset time
		 */
		Tracking& tracking = cycler[mask];

		if (tracking.reset == 0)
			tracking.reset = ServerInstance->Time() + secs;
		else if (ServerInstance->Time() > tracking.reset || tracking.counter >= cycles)
		{
			tracking.counter = 0;
			tracking.reset = ServerInstance->Time() + secs;
		}

		++tracking.counter;

		this->cleanup();
	}

	/* Called by PreJoin to check if a cycler's counter exceeds the set cycles,
	 * adds them to the blocked list if so.
	 * Will first clear a cycler if their reset time is up.
	 */
	bool zapme(const std::string& mask)
	{
		// Only check reset time and counter if they are already tracked as a cycler
		std::map<std::string, Tracking>::iterator it = cycler.find(mask);
		if (it == cycler.end())
			return false;

		const Tracking& tracking = it->second;

		if (ServerInstance->Time() > tracking.reset)
			cycler.erase(it);
		else if (tracking.counter >= cycles)
		{
			blocked[mask] = ServerInstance->Time() + block;
			cycler.erase(it);
			return true;
		}

		return false;
	}

	// Check if a joining user is blocked, clear them if blocktime is up
	bool isblocked(const std::string& mask)
	{
		std::map<std::string, time_t>::iterator it = blocked.find(mask);
		if (it == blocked.end())
			return false;

		if (ServerInstance->Time() > it->second)
			blocked.erase(it);
		else
			return true;

		return false;
	}

	void removeblock(const std::string& mask)
	{
		std::map<std::string, time_t>::iterator it = blocked.find(mask);
		if (it != blocked.end())
			blocked.erase(it);
	}

	// Clear expired entries of non cyclers and blocked cyclers
	void cleanup()
	{
		// 10 minutes should be a reasonable wait time
		if (ServerInstance->Time() - lastcleanup < 600)
			return;

		lastcleanup = ServerInstance->Time();

		for (std::map<std::string, Tracking>::iterator it = cycler.begin(); it != cycler.end(); )
		{
			const Tracking& tracking = it->second;

			if (ServerInstance->Time() > tracking.reset)
				cycler.erase(it++);
			else
				++it;
		}

		for (std::map<std::string, time_t>::iterator i = blocked.begin(); i != blocked.end(); )
		{
			if (ServerInstance->Time() > i->second)
				blocked.erase(i++);
			else
				++i;
		}
	}
};

class JoinPartSpam : public ModeHandler
{
	SimpleExtItem<joinpartspamsettings>& ext;
	bool& allowredirect;
	bool& freeredirect;

	bool ParseCycles(irc::sepstream& stream, unsigned int& cycles)
	{
		std::string strcycles;
		if (!stream.GetToken(strcycles))
			return false;

		unsigned int result = ConvToInt(strcycles);
		if (result < 2 || result > 20)
			return false;

		cycles = result;
		return true;
	}

	bool ParseSeconds(irc::sepstream& stream, unsigned int& seconds)
	{
		std::string strseconds;
		if (!stream.GetToken(strseconds))
			return false;

		unsigned int result = ConvToInt(strseconds);
		if (result < 1 || result > 43200)
			return false;

		seconds = result;
		return true;
	}

	bool ParseRedirect(irc::sepstream& stream, std::string& redirect, User* source, Channel* chan)
	{
		std::string strredirect;
		// This parameter is optional
		if (!stream.GetToken(strredirect))
			return true;

		if (!allowredirect)
		{
			source->WriteNumeric(ERR_INVALIDMODEPARAM, "%s %s %c %s :Invalid join/part spam mode parameter, the server admin has disabled channel redirection.",
				source->nick.c_str(), chan->name.c_str(), GetModeChar(), strredirect.c_str());
			return false;
		}

		if (!ServerInstance->IsChannel(strredirect.c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			source->WriteNumeric(ERR_INVALIDMODEPARAM, "%s %s %c %s :Invalid join/part spam mode parameter, redirect channel needs to be a valid channel name.",
				source->nick.c_str(), chan->name.c_str(), GetModeChar(), strredirect.c_str());
			return false;
		}

		if (chan->name == strredirect)
		{
			source->WriteNumeric(ERR_INVALIDMODEPARAM, "%s %s %c %s :Invalid join/part spam mode parameter, cannot redirect to myself.",
				source->nick.c_str(), chan->name.c_str(), GetModeChar(), strredirect.c_str());
			return false;
		}

		Channel* c = ServerInstance->FindChan(strredirect);
		if (!c)
		{
			source->WriteNumeric(ERR_INVALIDMODEPARAM, "%s %s %c %s :Invalid join/part spam mode parameter, redirect channel must exist.",
				source->nick.c_str(), chan->name.c_str(), GetModeChar(), strredirect.c_str());
			return false;
		}

		if (!freeredirect && c->GetPrefixValue(source) < HALFOP_VALUE)
		{
			source->WriteNumeric(ERR_INVALIDMODEPARAM, "%s %s %c %s :Invalid join/part spam mode parameter, you need at least halfop in the redirect channel.",
				source->nick.c_str(), chan->name.c_str(), GetModeChar(), strredirect.c_str());
			return false;
		}

		redirect = strredirect;
		return true;
	}

 public:
	JoinPartSpam(Module* Creator, SimpleExtItem<joinpartspamsettings>& e, bool& allow, bool& free)
		: ModeHandler(Creator, "joinpartspam", 'x', PARAM_SETONLY, MODETYPE_CHANNEL)
		, ext(e)
		, allowredirect(allow)
		, freeredirect(free)
	{
	}

	ModeAction OnModeChange(User* source, User*, Channel* chan, std::string& parameter, bool adding)
	{
		if (adding)
		{
			irc::sepstream stream(parameter, ':');
			unsigned int ncycles, nsecs, nblock;
			ncycles = nsecs = nblock = 0;
			std::string redirect;

			if (!ParseCycles(stream, ncycles))
			{
				source->WriteNumeric(ERR_INVALIDMODEPARAM, "%s %s %c %s :Invalid join/part spam mode parameter, 'cycles' needs to be between 2 and 20.",
					source->nick.c_str(), chan->name.c_str(), GetModeChar(), parameter.c_str());
				return MODEACTION_DENY;
			}
			if (!ParseSeconds(stream, nsecs) || !ParseSeconds(stream, nblock))
			{
				source->WriteNumeric(ERR_INVALIDMODEPARAM, "%s %s %c %s :Invalid join/part spam mode parameter, 'duration' and 'block time' need to be between 1 and 43200.",
					source->nick.c_str(), chan->name.c_str(), GetModeChar(), parameter.c_str());
				return MODEACTION_DENY;
			}
			// Error message is sent from ParseRedirect()
			if (!ParseRedirect(stream, redirect, source, chan))
				return MODEACTION_DENY;

			joinpartspamsettings* jpss = ext.get(chan);
			if (jpss && ncycles == jpss->cycles && nsecs == jpss->secs && nblock == jpss->block && redirect == jpss->redirect)
				return MODEACTION_DENY;

			ext.set(chan, new joinpartspamsettings(ncycles, nsecs, nblock, redirect));
			chan->SetModeParam(GetModeChar(), parameter);
			return MODEACTION_ALLOW;
		}

		if (!chan->IsModeSet(GetModeChar()))
			return MODEACTION_DENY;

		ext.unset(chan);
		chan->SetModeParam(GetModeChar(), "");
		return MODEACTION_ALLOW;
	}
};

class ModuleJoinPartSpam : public Module
{
	SimpleExtItem<joinpartspamsettings> ext;
	bool allowredirect;
	bool freeredirect;
	JoinPartSpam jps;

 public:
	ModuleJoinPartSpam()
		: ext("joinpartspam", this)
		, allowredirect(false)
		, freeredirect(false)
		, jps(this, ext, allowredirect, freeredirect)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServiceProvider* servicelist[] = { &ext, &jps };
		ServerInstance->Modules->AddServices(servicelist, sizeof(servicelist)/sizeof(ServiceProvider*));
		Implementation eventlist[] = { I_OnRehash, I_OnUserPreJoin, I_OnUserJoin, I_OnUserInvite };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void Prioritize()
	{
		// Let bans, etc. stop the join first
		ServerInstance->Modules->SetPriority(this, I_OnUserPreJoin, PRIORITY_LAST);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("joinpartspam");
		allowredirect = tag->getBool("allowredirect");
		freeredirect = tag->getBool("freeredirect");
	}

	// Stop the join and clear the user's counter if they've hit the limit
	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string& privs, const std::string& keygiven)
	{
		if (!chan || !chan->IsModeSet(jps.GetModeChar()))
			return MOD_RES_PASSTHRU;
		if (IS_OPER(user))
			return MOD_RES_PASSTHRU;

		joinpartspamsettings* jpss = ext.get(chan);
		if (!jpss)
			return MOD_RES_PASSTHRU;

		const std::string& mask(user->MakeHost());

		if (jpss->isblocked(mask))
		{
			user->WriteNumeric(RPL_CHANBLOCKED, "%s %s :Channel join/part spam triggered (limit is %u cycles in %u secs). Please try again later.",
				user->nick.c_str(), chan->name.c_str(), jpss->cycles, jpss->secs);
			return MOD_RES_DENY;
		}
		else if (jpss->zapme(mask))
		{
			// The user is now in the blocked list, deny the join, and if
			// redirect is wanted and allowed, we join the user to that channel.
			user->WriteNumeric(RPL_CHANBLOCKED, "%s %s :Channel join/part spam triggered (limit is %u cycles in %u secs). Please try again in %u seconds.",
				user->nick.c_str(), chan->name.c_str(), jpss->cycles, jpss->secs, jpss->block);

			if (allowredirect && !jpss->redirect.empty())
				Channel::JoinUser(user, jpss->redirect.c_str(), false, "", false, ServerInstance->Time());
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	// Only count successful joins
	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
	{
		if (sync)
			return;
		if (created || !memb->chan->IsModeSet(jps.GetModeChar()))
			return;
		if (IS_OPER(memb->user))
			return;

		joinpartspamsettings* jpss = ext.get(memb->chan);
		if (jpss)
		{
			const std::string& mask(memb->user->MakeHost());
			jpss->addcycle(mask);
		}
	}

	// Remove a block on a user on a successful invite
	void OnUserInvite(User*, User* user, Channel* chan, time_t)
	{
		if (!chan->IsModeSet(jps.GetModeChar()))
			return;

		joinpartspamsettings* jpss = ext.get(chan);
		if (!jpss)
			return;

		const std::string& mask(user->MakeHost());
		jpss->removeblock(mask);
	}

	Version GetVersion()
	{
		return Version("Provides channel mode +" + ConvToStr(jps.GetModeChar()) + " for blocking Join/Part spammers.", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleJoinPartSpam)
