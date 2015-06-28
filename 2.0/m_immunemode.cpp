/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Miguel Peláez <miguel2706@outlook.com>
 *   Copyright (C) 2014-2015 Hira.io Team <staff@hira.io>
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


/* $ModAuthor: Hira.io Team */
/* $ModAuthorMail: miguel2706@outlook.com */
/* $ModDesc: Module to make an operator immune for a specified time, it's like a uline, but it's hidden */
/* $ModDepends: core 2.0 */

#include <list>
#include "inspircd.h"

struct AutoRemoveMode : public Timer
{
 public:
        std::string userid;
        AutoRemoveMode(long interval)
                : Timer(interval, ServerInstance->Time(), false)
        {
        }
        void Tick(time_t TIME)
        {
		User* user = ServerInstance->FindUUID(userid);
		if(!user)
			return;
		if(user->IsModeSet('t'))
			return;
		user->WriteServ("NOTICE "+user->nick+" :*** Removing your user mode +t to prevent abuses.");
		std::vector<std::string> modes;
		modes.push_back(user->nick);
		modes.push_back("-t");
		ServerInstance->SendGlobalMode(modes, ServerInstance->FakeClient);
        }
};
class ImmuneMode : public SimpleUserModeHandler
{
 public:
	int Timeout;
	std::list<AutoRemoveMode*> TimerList;
	ImmuneMode(Module* Creator) : SimpleUserModeHandler(Creator, "immune", 't')
	{
		oper = true;
	}
	ModeAction OnModeChange(User* source, User* user, Channel* channel, std::string &parameter, bool adding)
        {
		if(!IS_LOCAL(user))
		{
			user->SetMode('t', adding);
			return MODEACTION_ALLOW;
		}
                if (adding == user->IsModeSet('t'))
                        return MODEACTION_DENY;

                if(!adding)
		{
			user->SetMode('t', false);
                        return MODEACTION_ALLOW;
		}
		if(!Timeout)
		{
			user->WriteServ("NOTICE "+user->nick+" :*** This mode will be in place indefinitely," +
			 " remember to take it off to itself to prevent abuse of power or setting the automatic removal.");
			user->SetMode('t',true);
			return MODEACTION_ALLOW;
		}
                AutoRemoveMode *timer = new AutoRemoveMode(Timeout);
                timer->userid = user->uuid;
                ServerInstance->Timers->AddTimer(timer);
		TimerList.push_back(timer);
		user->SetMode('t', true);
                return MODEACTION_ALLOW;
        }
};
class ModuleImmune : public Module
{
 public:
	ImmuneMode im;
	

	ModuleImmune()
		: im(this)
	{
	}
	
	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modules->AddService(im);
		Implementation eventlist[] = { I_OnUserPreKick, I_OnCheckChannelBan, I_OnCheckInvite, I_OnCheckKey, I_OnCheckLimit, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));		
	}
	/* We will protect the user of the following if he has the umode +t */
	ModResult OnUserPreKick(User *source, Membership *memb, const std::string& reason)
	{
		User *user = memb->user;
		//This kid is different!
                if (!IS_LOCAL(user))
                        return MOD_RES_PASSTHRU;
                if (user->IsModeSet('t'))
                        return MOD_RES_DENY;
                else
                        return MOD_RES_ALLOW;
	}
	ModResult OnCheckChannelBan(User * user, Channel * chan)
	{
		return this->Check(user);
	}
	ModResult OnCheckInvite(User * user, Channel * chan)
	{
		return this->Check(user);
	}
	ModResult OnCheckKey(User * user, Channel * chan, const std::string & keygiven)
	{
		return this->Check(user);
	}
	ModResult OnCheckLimit(User * user, Channel * chan)
	{
		return this->Check(user);
	}
	Version GetVersion()
	{
		return Version("Provides the umode +t, it makes an operator immune for a specified time.", VF_COMMON);
	}
	virtual void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("immune");
		im.Timeout = tag->getInt("timeout");
	}
 private:
	/* Perform the verification */
	ModResult Check(User *user)
	{
		if (!IS_LOCAL(user))
                        return MOD_RES_PASSTHRU;
		if (user->IsModeSet('t'))
			return MOD_RES_ALLOW;
		else
			return MOD_RES_PASSTHRU;
	}
	~ModuleImmune()
	{
		for (std::list<AutoRemoveMode*>::iterator it=im.TimerList.begin(); it != im.TimerList.end(); ++it)
		{
			ServerInstance->Timers->DelTimer((AutoRemoveMode*)*it);
		}
	}
};

MODULE_INIT(ModuleImmune)
