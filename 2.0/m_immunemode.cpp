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
/* $ModDesc: Module to make an operator immune for a specified time, it's like a uline */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
int Timeout = 0;
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
		user->WriteServ("NOTICE "+user->nick+" :*** Removing your user mode +t to prevent abuses.");
		std::vector<std::string> modes;
		modes.push_back(user->nick);
		modes.push_back("-t");
		ServerInstance->SendGlobalMode(modes, ServerInstance->FakeClient);
        }
};
/** User mode +t - Immune from kicks
 */
class ImmuneMode : public SimpleUserModeHandler
{
 public:
	ImmuneMode(Module* Creator) : SimpleUserModeHandler(Creator, "immune", 't')
	{
		oper = true;
	}
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
        {
		if(!adding)
			return MODEACTION_ALLOW;
		if(!IS_LOCAL(dest))
			return MODEACTION_ALLOW;
		if((!Timeout)||(Timeout == 0))
		{
			dest->WriteServ("NOTICE "+dest->nick+" :*** This mode will be in place indefinitely, remember to take it off to itself to prevent abuse of power or setting the automatic removal.");
			return MODEACTION_ALLOW;
		}
                AutoRemoveMode *timer = new AutoRemoveMode(Timeout);
                timer->userid = dest->uuid;
                ServerInstance->Timers->AddTimer(timer);
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

	
	ModResult OnUserPreKick(User *source, Membership *memb, const std::string& reason)
	{
		if (!IS_LOCAL(memb->user))
                        return MOD_RES_PASSTHRU;
		if (memb->user->IsModeSet('t'))
			return MOD_RES_DENY;
		else
			return MOD_RES_PASSTHRU;
	}
	ModResult OnCheckChannelBan(User * user, Channel * chan)
	{
		if (!IS_LOCAL(user))
                        return MOD_RES_PASSTHRU;
		if (user->IsModeSet('t'))
			return MOD_RES_ALLOW;
		else
			return MOD_RES_PASSTHRU;
	}
	ModResult OnCheckInvite(User * user, Channel * chan)
	{
		if (!IS_LOCAL(user))
                        return MOD_RES_PASSTHRU;
		if (user->IsModeSet('t'))
			return MOD_RES_ALLOW;
		else
			return MOD_RES_PASSTHRU;
	}
	ModResult OnCheckKey(User * user, Channel * chan, const std::string & keygiven)
	{
		if (!IS_LOCAL(user))
                        return MOD_RES_PASSTHRU;
		if (user->IsModeSet('t'))
			return MOD_RES_ALLOW;
		else
			return MOD_RES_PASSTHRU;
	}
	ModResult OnCheckLimit(User * user, Channel * chan)
	{
		if (!IS_LOCAL(user))
                        return MOD_RES_PASSTHRU;
		if (user->IsModeSet('t'))
			return MOD_RES_ALLOW;
		else
			return MOD_RES_PASSTHRU;
	}
	Version GetVersion()
	{
		return Version("Provides the umode +t, that prevents the user is kicked from any channel. Can only be set by a network operator.", VF_VENDOR);
	}
	virtual void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("immune");
		int timeout = tag->getInt("timeout");
		Timeout = timeout;
	}
};

MODULE_INIT(ModuleImmune)
