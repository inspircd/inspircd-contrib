/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Adam <Adam@anope.org>
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

/* $ModAuthor: Adam */
/* $ModAuthorMail: Adam@anope.org */
/* $ModDesc: Incoming connection throttle */
/* $ModDepends: core 2.0 */

class Throttle
{
 public:
	int con_count;
	time_t last_attempt;

	Throttle() : con_count(0), last_attempt(0) { }
};

class ModuleConnThrottle : public Module
{
	typedef nspace::hash_map<std::string, Throttle*> throttle_hash;

	throttle_hash throttles;

	int throttle_num;
	int throttle_time;

 public:

	~ModuleConnThrottle()
	{
		for (throttle_hash::iterator it = throttles.begin(); it != throttles.end(); ++it)
			delete it->second;
	}

	Version GetVersion()
	{
		return Version("Incoming connection throttle", VF_NONE);
	}

	void init()
	{
		Implementation eventlist[] = { I_OnRehash, I_OnAcceptConnection, I_OnGarbageCollect };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist) / sizeof(Implementation));

		OnRehash(NULL);
	}

	void OnRehash(User* user)
	{
		ConfigTag *tag = ServerInstance->Config->ConfValue("connthrottle");

		throttle_num = tag->getInt("num", 1);
		throttle_time = tag->getInt("time", 1);
	}

	ModResult OnAcceptConnection(int fd, ListenSocket* sock, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
	{
		if (ServerInstance->XLines->MatchesLine("E", client->addr()) != NULL)
			return MOD_RES_PASSTHRU;

		Throttle*& throttle = throttles[client->addr()];
		if (throttle != NULL && ServerInstance->Time() - throttle->last_attempt < throttle_time)
		{
			if (throttle->con_count >= throttle_num)
			{
				if (sock->bind_tag->getString("ssl").empty())
				{
					const char err[] = "ERROR :Trying to reconnect too fast.\r\n";
					send(fd, err, sizeof(err) - 1, 0);
				}

				return MOD_RES_DENY;
			}

			++throttle->con_count;
		}
		else
		{
			if (throttle == NULL)
				throttle = new Throttle();

			throttle->con_count = 1;
			throttle->last_attempt = ServerInstance->Time();
		}

		return MOD_RES_PASSTHRU;
	}

	void OnGarbageCollect()
	{
		throttle_hash temp;

		throttles.swap(temp);

		for (throttle_hash::iterator it = temp.begin(); it != temp.end(); ++it)
		{
			const std::string &ip = it->first;
			Throttle *t = it->second;

			if (ServerInstance->Time() - t->last_attempt < throttle_time)
			{
				throttles[ip] = t;
			}
		}
	}
};

MODULE_INIT(ModuleConnThrottle)
