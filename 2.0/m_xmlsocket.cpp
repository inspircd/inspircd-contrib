/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
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


/* $ModConfig: <bind xmlsocket="yes"> */
/* $ModDesc: Provides XML socket support for Flash */
/* $ModAuthor: Attila Molnar */
/* $ModAuthorMail: attilamolnar@hush.com */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleXMLSocket : public Module
{
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnHookIO };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnHookIO(StreamSocket* sock, ListenSocket* lsb)
	{
		if (!sock->GetIOHook() && lsb->bind_tag->getBool("xmlsocket"))
			sock->AddIOHook(this);
	}

	void OnCleanup(int target_type, void* item)
	{
		if (target_type == TYPE_USER)
		{
			LocalUser* user = IS_LOCAL(static_cast<User*>(item));
			if ((user) && (user->eh.GetIOHook() == this))
				ServerInstance->Users->QuitUser(user, "XML socket module unloading");
		}
	}

	int OnStreamSocketRead(StreamSocket* sock, std::string& recvq)
	{
		char* buffer = ServerInstance->GetReadBuffer();
		size_t bufsize = ServerInstance->Config->NetBufferSize;
		int ret = ServerInstance->SE->Recv(sock, buffer, bufsize, 0);
		if (ret > 0)
		{
			for (int i = 0; i < ret; i++)
			{
				char c = buffer[i];
				if (c == 0)
					c = '\n';
				recvq.push_back(c);
			}

			int change = FD_WANT_FAST_READ;
			if ((size_t)ret == bufsize)
				change |= FD_ADD_TRIAL_READ;
			ServerInstance->SE->ChangeEventMask(sock, change);

			return 1;
		}
		else if (ret == 0)
		{
			sock->SetError("Connection closed");
			ServerInstance->SE->ChangeEventMask(sock, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
			return -1;
		}

		// ret < 0
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
		{
			ServerInstance->SE->ChangeEventMask(sock, FD_WANT_FAST_READ | FD_READ_WILL_BLOCK);
			return 0;
		}

		ServerInstance->SE->ChangeEventMask(sock, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
		sock->SetError(SocketEngine::LastError());
		return -1;
	}

	int OnStreamSocketWrite(StreamSocket* sock, std::string& buffer)
	{
		for (std::string::iterator i = buffer.begin(); i != buffer.end(); ++i)
		{
			char& c = *i;
			if ((c == '\r') || (c == '\n'))
				c = 0;
		}

		int ret = ServerInstance->SE->Send(sock, buffer.data(), buffer.size(), 0);
		if (ret > 0)
		{
			if ((size_t)ret == buffer.size())
			{
				ServerInstance->SE->ChangeEventMask(sock, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
				return 1;
			}

			buffer = buffer.substr(ret);
			ServerInstance->SE->ChangeEventMask(sock, FD_WANT_SINGLE_WRITE);
			return 0;
		}
		else if (ret == 0)
		{
			sock->SetError("Connection closed");
			return -1;
		}

		// ret < 0
		if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
		{
			ServerInstance->SE->ChangeEventMask(sock, FD_WANT_FAST_WRITE | FD_WRITE_WILL_BLOCK);
			return 0;
		}

		ServerInstance->SE->ChangeEventMask(sock, FD_WANT_NO_READ | FD_WANT_NO_WRITE);
		sock->SetError(SocketEngine::LastError());
		return -1;
	}

	Version GetVersion()
	{
		return Version("Provides XML socket support for Flash");
	}
};

MODULE_INIT(ModuleXMLSocket)

