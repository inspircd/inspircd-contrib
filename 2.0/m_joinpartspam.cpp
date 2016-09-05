/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 genius3000 <genius3000@g3k.solutions>
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

#include "inspircd.h"

/* $ModDesc: Adds channel mode +V to ban a user after x per y joins and parts/quits (join/part spam) */
/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDepends: core 2.0 */

/* Special thanks to Attila for the patience and guidance in
 * fixing up some of my initial, poor methods.
 */

class joinpartspamsettings
{
 public:
	unsigned int cycles;
	unsigned int secs;
	time_t lastcleanup;

	struct Tracking
	{
		time_t reset;
		unsigned int counter;
		Tracking() : reset(0), counter(0) { }
	};
	std::map<std::string, Tracking> cycler;

	joinpartspamsettings(unsigned int c, unsigned int s)
		: cycles(c), secs(s), lastcleanup(ServerInstance->Time()) { }

	/* Called by PostJoin to possibly reset a cycler's Tracking and increment the counter */
	void addcycle(const std::string& mask)
	{
		/* If mask isn't already tracked, set reset time
		 * If tracked and reset time is up, reset counter and reset time
		 * Also assume another server denied join and set ban, with a user removing
		 * the ban if counter >= cycles, reset counter and reset time
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
	 * Will first clear a cycler if their reset time is up
	 */
	bool zapme(const std::string& mask)
	{
		/* Only check reset time and counter if they are already tracked as a cycler */
		std::map<std::string, Tracking>::iterator it = cycler.find(mask);
		if (it == cycler.end())
			return false;

		const Tracking& tracking = it->second;

		if (ServerInstance->Time() > tracking.reset)
			cycler.erase(it);
		else if (tracking.counter >= cycles)
		{
			cycler.erase(it);
			return true;
		}

		return false;
	}

	/* Clear expired entries of non cyclers */
	void cleanup()
	{
		/* 10 minutes should be a reasonable wait time */
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
	}
};

/* Handles channel mode +V */
class JoinPartSpam : public ModeHandler
{
 public:
	SimpleExtItem<joinpartspamsettings> ext;
	JoinPartSpam(Module* Creator) : ModeHandler(Creator, "joinpartspam", 'V', PARAM_SETONLY, MODETYPE_CHANNEL)
		, ext("joinpartspam", Creator)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* chan, std::string& parameter, bool adding)
	{
		if (adding)
		{
			std::string::size_type colon = parameter.find(':');
			if (colon == std::string::npos || parameter.find('-') != std::string::npos)
			{
				source->WriteNumeric(608, "%s %s :Invalid join/part spam parameter", source->nick.c_str(), chan->name.c_str());
				return MODEACTION_DENY;
			}

			unsigned int ncycles = ConvToInt(parameter.substr(0, colon));
			unsigned int nsecs = ConvToInt(parameter.substr(colon+1));

			if (ncycles < 2 || nsecs < 1)
			{
				source->WriteNumeric(608, "%s %s :Invalid join/part spam parameter", source->nick.c_str(), chan->name.c_str());
				return MODEACTION_DENY;
			}

			joinpartspamsettings* jpss = ext.get(chan);
			if (jpss && ncycles == jpss->cycles && nsecs == jpss->secs)
				return MODEACTION_DENY;

			ext.set(chan, new joinpartspamsettings(ncycles, nsecs));
			parameter = ConvToStr(ncycles) + ":" + ConvToStr(nsecs);
			chan->SetModeParam(GetModeChar(), parameter);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (!chan->IsModeSet(GetModeChar()))
				return MODEACTION_DENY;

			ext.unset(chan);
			chan->SetModeParam(GetModeChar(), "");
			return MODEACTION_ALLOW;
		}
	}
};

class ModuleJoinPartSpam : public Module
{
	JoinPartSpam jps;

 public:
	ModuleJoinPartSpam()
		: jps(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(jps);
		ServerInstance->Modules->AddService(jps.ext);
		Implementation eventlist[] = { I_OnUserPreJoin, I_OnUserJoin };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void Prioritize()
	{
		/* Let bans, etc. stop the join first */
		ServerInstance->Modules->SetPriority(this, I_OnUserPreJoin, PRIORITY_LAST);
		ServerInstance->Modules->SetPriority(this, I_OnUserJoin, PRIORITY_LAST);
	}

	/* Stop the join and clear the user's counter if they've hit the limit */
	ModResult OnUserPreJoin(User* user, Channel* chan, const char* cname, std::string& privs, const std::string& keygiven)
	{
		if (!chan || !chan->IsModeSet(jps.GetModeChar()))
			return MOD_RES_PASSTHRU;
		if (IS_OPER(user))
			return MOD_RES_PASSTHRU;

		joinpartspamsettings* jpss = jps.ext.get(chan);
		if (jpss)
		{
			const std::string& mask(user->MakeHost());
			if (jpss->zapme(mask))
			{
				std::vector<std::string> parameters;
				parameters.push_back(chan->name);
				parameters.push_back("+b");
				parameters.push_back(user->MakeWildHost());
				ServerInstance->SendGlobalMode(parameters, ServerInstance->FakeClient);

				user->WriteNumeric(474, "%s %s :Channel join/part spam triggered (limit is %u cycles in %u secs)", user->nick.c_str(), chan->name.c_str(), jpss->cycles, jpss->secs);
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	/* Only count successful joins */
	void OnUserJoin(Membership* memb, bool sync, bool created, CUList& except)
	{
		if (sync)
			return;
		if (created || !memb->chan->IsModeSet(jps.GetModeChar()))
			return;
		if (IS_OPER(memb->user))
			return;

		joinpartspamsettings* jpss = jps.ext.get(memb->chan);
		if (jpss)
		{
			const std::string& mask(memb->user->MakeHost());
			jpss->addcycle(mask);
		}
	}

	Version GetVersion()
	{
		return Version("Provides channel mode +" + ConvToStr(jps.GetModeChar()) + " for banning Join/Part spammers.");
	}
};

MODULE_INIT(ModuleJoinPartSpam)
