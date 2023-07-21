/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2023 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModConfig: <bind path="message.sock" type="message">
/// $ModDesc: Allows sending messages to all local users from a socket.
/// $ModDepends: core 3


#include "inspircd.h"

class MessageSocket;

namespace
{
	insp::intrusive_list<MessageSocket> sockets;
}

class MessageSocket CXX11_FINAL
	: public BufferedSocket
	, public insp::intrusive_list_node<MessageSocket>
{
private:
	bool culling;

public:
	MessageSocket(int nfd)
		: BufferedSocket(nfd)
		, culling(false)
	{
	}

	~MessageSocket()
	{
		sockets.erase(this);
	}


	void DoCull()
	{
		if (!culling)
			return;

		culling = true;
		Close();
		ServerInstance->GlobalCulls.AddItem(this);
	}

	void OnError(BufferedSocketError) CXX11_OVERRIDE
	{
		DoCull();
	}

	void OnDataReady() CXX11_OVERRIDE
	{
		// Annoyingly we can't use sepstream here as it will consume unfinished lines.
		size_t eolpos;
		while ((eolpos = recvq.find('\n')) != std::string::npos)
		{
			// Extract the message.
			std::string message(recvq, 0, eolpos);
			recvq.erase(0, eolpos + 1);

			// Handle accidental Windows newlines.
			if (message.back() == '\r')
				message.pop_back();

			const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
			for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
			{
				LocalUser* curr = *i;
				if (!curr->quitting && curr->registered == REG_ALL)
					curr->WriteNotice("*** " + message);
			}

		}
	}
};

class ModuleMessageSocket CXX11_FINAL
	: public Module
{
public:
	ModResult OnAcceptConnection(int nfd, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server) CXX11_OVERRIDE
	{
		if (!stdalgo::string::equalsci(from->bind_tag->getString("type"), "message"))
			return MOD_RES_PASSTHRU;

		sockets.push_front(new MessageSocket(nfd));
		return MOD_RES_ALLOW;
	}

	CullResult cull() CXX11_OVERRIDE
	{
		for (insp::intrusive_list<MessageSocket>::const_iterator i = sockets.begin(); i != sockets.end(); ++i)
		{
			MessageSocket* sock = *i;
			sock->DoCull();
		}
		return Module::cull();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows sending messages to all localusers from a socket.");
	}
};

MODULE_INIT(ModuleMessageSocket)

