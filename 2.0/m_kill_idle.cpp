/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemonirc@gmail.com>
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

/* $ModAuthor: linuxdaemon */
/* $ModAuthorMail: linuxdaemonirc@gmail.com */
/* $ModDepends: core 2.0 */
/* $ModDesc: Disconnect idle users matching configured conditions */
/* $ModConfig: <idleprofile name="example" nochans="true" idletime="7200" ignoreloggedin="no" reason="Disconnected for inactivity" away="only"> */

// Idle profiles are selected via the <connect:idleprofile> setting

// Opers with the "users/no-idle-kill" privilege will be ignored

// <idleprofile:away> allows the module to:
//   "only": only disconnect idle users who are also marked /away
//   "ignore": only disconnect idle users who are NOT marked /away
//   "none": disconnect idle users regardless of their away status

// <idleprofile:nochans> controls whether the module will only kill off users who are in no channels

#include "inspircd.h"
#include "account.h"

inline std::string* GetUserAccount(User* user)
{
	AccountExtItem* ext = GetAccountExtItem();
	if (!ext)
		return NULL;

	return ext->get(user);
}

inline unsigned long GetIdle(LocalUser* lu)
{
	return ServerInstance->Time() - lu->idle_lastmsg;
}

struct IdleProfile
{
	std::string name;

	/** Reason for idle disconnect as shown to all users */
	std::string reason;

	/** Minimum time since last message */
	unsigned long mintime;

	enum AwayCondition
	{
		AWAY_IGNORE,
		AWAY_ONLY,
		AWAY_NONE
	} away;

	/** Does the user need to be in no channels to match */
	bool nochans;

	/** Should we ignore users who are logged in to an account */
	bool ignoreloggedin;

	IdleProfile()
		: mintime(7200)
		, away(AWAY_NONE)
		, nochans(true)
		, ignoreloggedin(false)
	{
	}

	bool Matches(LocalUser* lu) const
	{
		if (lu->registered != REG_ALL)
			return false;

		if (lu->HasPrivPermission("users/no-idle-kill"))
			return false;

		if (ignoreloggedin && GetUserAccount(lu))
			return false;

		if (nochans && !lu->chans.empty())
			return false;

		switch (away)
		{
			case AWAY_ONLY:
				if (lu->awaymsg.empty())
					return false;
				break;
			case AWAY_IGNORE:
				if (!lu->awaymsg.empty())
					return false;
				break;
			case AWAY_NONE:
			default:
				break;
		}

		if (GetIdle(lu) < mintime)
			return false;

		return true;
	}
};

typedef std::map<std::string, IdleProfile> ProfileMap;

class ModuleKillIdle
	: public Module
{
	ProfileMap profiles;

	IdleProfile* GetProfile(LocalUser* user)
	{
		ConnectClass* cls = user->GetClass();
		if (!cls)
			return NULL;

		std::string name = cls->config->getString("idleprofile");
		if (name.empty())
			return NULL;

		ProfileMap::iterator it = profiles.find(name);
		if (it == profiles.end())
			return NULL;

		return &it->second;
	}

 public:
	void init()
	{
		Implementation eventlist[] = { I_OnRehash, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	void OnRehash(User*)
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags("idleprofile");
		ProfileMap newprofiles;
		for (ConfigIter it = tags.first; it != tags.second; ++it)
		{
			ConfigTag* tag = it->second;

			std::string name = tag->getString("name");
			if (name.empty())
				throw ModuleException("Empty <idleprofile:name> at " + tag->getTagLocation());

			if (newprofiles.find(name) != newprofiles.end())
				throw ModuleException("Duplicate <idleprofile:name> found at " + tag->getTagLocation());

			IdleProfile& profile = newprofiles[name];
			profile.name = name;
			profile.nochans = tag->getBool("nochans", profile.nochans);
			std::string timestr = tag->getString("idletime");
			if (!timestr.empty())
			{
				unsigned long duration = ServerInstance->Duration(timestr);
				// profile.mintime was already set to its default
				if (duration >= 60)
					profile.mintime = duration;
			}
			profile.ignoreloggedin = tag->getBool("ignoreloggedin", profile.ignoreloggedin);
			profile.reason = tag->getString("reason", "Disconnected for inactivity");
			irc::string away = assign(tag->getString("away"));

			if (away == "only")
				profile.away = IdleProfile::AWAY_ONLY;
			else if (away == "ignore")
				profile.away = IdleProfile::AWAY_IGNORE;
			else
				profile.away = IdleProfile::AWAY_NONE;
		}
		profiles.swap(newprofiles);
	}

	void OnBackgroundTimer(time_t)
	{
		LocalUserList& users = ServerInstance->Users->local_users;
		for (LocalUserList::const_iterator it = users.begin(); it != users.end();)
		{
			// The iterator may be invalidated by QuitUser()
			LocalUser* u = *it;
			++it;
			IdleProfile* profile = GetProfile(u);
			if (profile && profile->Matches(u))
				ServerInstance->Users->QuitUser(u, profile->reason);
		}
	}

	Version GetVersion()
	{
		return Version("Disconnect idle users matching configured conditions");
	}
};

MODULE_INIT(ModuleKillIdle)
