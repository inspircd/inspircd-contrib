/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
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

/* $ModAuthor: Daniel Vassdal */
/* $ModAuthorMail: shutter@canternet.org */
/* $ModDesc: Enable TCP_DEFER_ACCEPT on sockets */
/* $ModDepends: core 2.0 */
/* $ModConfig: <bind defer="0"> */

#include "inspircd.h"
#include <netinet/tcp.h>

#if !defined TCP_DEFER_ACCEPT && !defined SO_ACCEPTFILTER
#error "This system does not support TCP_DEFER_ACCEPT - you can\'t use this module"
#endif

class ModuleDeferAccept : public Module
{
 private:
	bool removing;

 public:
	ModuleDeferAccept() : removing(false)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modules->Attach(I_OnRehash, this);
	}

	void OnRehash(User* user)
	{
		for (std::vector<ListenSocket*>::const_iterator it = ServerInstance->ports.begin(); it != ServerInstance->ports.end(); ++it)
		{
			int timeout = 0;
			if (!removing)
				timeout = (*it)->bind_tag->getInt("defer", 0);

			int fd = (*it)->GetFd();
#ifdef TCP_DEFER_ACCEPT
			setsockopt(fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &timeout, sizeof(timeout));
#elif defined SO_ACCEPTFILTER
			struct accept_filter_arg afa;
			memset(&afa, 0, sizeof(afa));
			strcpy(afa.af_name, "dataready");
			setsockopt(fd, SOL_SOCKET, SO_ACCEPTFILTER, (!timeout ? NULL : &afa), sizeof(afa));
#endif
		}
	}

	~ModuleDeferAccept()
	{
		removing = true;
		OnRehash(NULL);
	}

	Version GetVersion()
	{
		return Version("Enable TCP Defer Accept", VF_NONE);
	}

};

MODULE_INIT(ModuleDeferAccept)
