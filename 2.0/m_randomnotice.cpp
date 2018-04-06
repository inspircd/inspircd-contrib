/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 genius30000 <genius3000@g3k.solutions>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2003, 2006 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2005 Craig McLure <craig@chatspike.net>
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

/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDesc: Send a random notice (quote) from a file to all users at a set interval. */
/* $ModDepends: core 2.0 */
/* $ModConfig: <randomnotice file="conf/randomnotices.txt" interval="30m"> */
/* "file" needs to be a text file with each 'notice' on a new line
 * "interval" is a time-string (1y7d8h6m3s format)
 * options of: "prefix" and "suffix" are also available
 */

#include "inspircd.h"


class ModuleRandomNotice : public Module
{
	FileReader* notices = NULL;
	std::string prefix;
	std::string suffix;
	time_t lastsent;
	long interval;

	/* insp20's Timer class doesn't allow changing the interval of an added Timer, so we just check
	 * the current time to our 'last sent' time plus the wait interval.
	 * This is similar to how spanningtree's AutoConnect works, to allow Rehash changes to interval.
	 */
	void SendNotice(time_t curtime)
	{
		if ((lastsent + interval > curtime) || !notices)
			return;

		unsigned int fsize = notices->FileSize();
		if (!fsize)
			return;

		const std::string notice = notices->GetLine(ServerInstance->GenRandomInt(fsize));
		if (notice.empty())
			return;

		for (LocalUserList::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); ++i)
		{
			LocalUser* lu = *i;

			if (lu->registered == REG_ALL)
				lu->WriteServ("NOTICE %s :%s%s%s", lu->nick.c_str(), prefix.c_str(), notice.c_str(), suffix.c_str());
		}
		lastsent = curtime;
	}

 public:
	ModuleRandomNotice()
		: lastsent(0)
		, interval(1800)
	{
	}

	void init()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnBackgroundTimer, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("randomnotice");
		std::string n_file = tag->getString("file", "conf/randomnotices.txt");
		prefix = tag->getString("prefix");
		suffix = tag->getString("suffix");
		interval = ServerInstance->Duration(tag->getString("interval", "30m"));

		// Some sane(ish) limits to interval (1m to 1y :D)
		if (interval < 60)
			interval = 60;
		else if (interval > 31536000)
			interval = 31536000;

		notices = new FileReader(n_file);
		if (!notices->Exists())
		{
			throw ModuleException("Random Notices file not found!! Please check your config - module will not function.");
		}
	}

	void OnBackgroundTimer(time_t curtime)
	{
		SendNotice(curtime);
	}

	~ModuleRandomNotice()
	{
		delete notices;
	}

	Version GetVersion()
	{
		return Version("Send a random notice (quote) to all users at a set interval.");
	}
};

MODULE_INIT(ModuleRandomNotice)
