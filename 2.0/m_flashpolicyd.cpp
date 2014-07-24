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

/*
 * How to use this module:
 * Load the module and assign a port to it, e.g. like this
 * <bind address="" port="8430" type="flashpolicyd">
 * Unless you want to specify a policy file manually,
 * or change the timeout, you're done now.
 */

/* $ModDesc: Flash Policy Daemon. Allows Flash IRC clients to connect. */
/* $ModAuthor: Daniel Vassdal */
/* $ModDepends: core 2.0 */
/* $ModConfig: <flashpolicyd timeout="5" file="/path/to/file"> */

#include "inspircd.h"

class FlashPDSocket;

namespace
{
	std::set<FlashPDSocket*> sockets;
	std::string policy_reply;
	std::string expected_request;
}

class FlashPDSocket : public BufferedSocket
{
 public:
	time_t created;

	FlashPDSocket(int newfd)
		: BufferedSocket(newfd)
		, created(ServerInstance->Time())
	{
	}

	~FlashPDSocket()
	{
		sockets.erase(this);
	}

	void OnError(BufferedSocketError)
	{
		AddToCull();
	}

	void OnDataReady()
	{
		if (recvq == expected_request)
			WriteData(policy_reply);
		AddToCull();
	}

	void AddToCull()
	{
		if (created == 0)
			return;

		created = 0;
		Close();
		ServerInstance->GlobalCulls.AddItem(this);
	}
};

class ModuleFlashPD : public Module
{
	time_t timeout;

 public:
	void init()
	{
		expected_request = std::string("<policy-file-request/>\0", 23);
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnRehash, I_OnAcceptConnection, I_OnBackgroundTimer };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnBackgroundTimer(time_t curtime)
	{
		for (std::set<FlashPDSocket*>::const_iterator i = sockets.begin(); i != sockets.end(); ++i)
		{
			FlashPDSocket* sock = *i;
			if ((sock->created + timeout > curtime) && (sock->created != 0))
				sock->AddToCull();
		}
	}

	ModResult OnAcceptConnection(int nfd, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
	{
		if (from->bind_tag->getString("type") != "flashpolicyd")
			return MOD_RES_PASSTHRU;

		if (policy_reply.empty())
			return MOD_RES_DENY;

		sockets.insert(new FlashPDSocket(nfd));
		return MOD_RES_ALLOW;
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("flashpolicyd");
		timeout = tag->getInt("timeout", 5);
		std::string file = tag->getString("file");

		if (timeout == 0)
			timeout = 1;

		if (!file.empty())
		{
			try
			{
				FileReader reader(file);
				policy_reply = reader.Contents();
			}
			catch (CoreException&)
			{
				const std::string error_message = "A file was specified for FlashPD, but it could not be loaded.";
				ServerInstance->Logs->Log("m_flashpd", DEFAULT, error_message);
				ServerInstance->SNO->WriteGlobalSno('a', error_message);
				policy_reply.clear();
			}
			return;
		}

		//	A file was not specified. Set the default setting.
		//	We allow access to all client ports by default
		std::string to_ports;
		for (std::vector<ListenSocket*>::const_iterator i = ServerInstance->ports.begin(); i != ServerInstance->ports.end(); ++i)
		{
				ListenSocket* ls = *i;
				if (ls->bind_tag->getString("type", "clients") != "clients" || ls->bind_tag->getString("ssl", "plaintext") != "plaintext")
					continue;

				to_ports += (ConvToStr(ls->bind_port) + ",");
		}

		if (to_ports.empty())
		{
			policy_reply.clear();
			return;
		}

		to_ports.erase(to_ports.size() - 1);

		policy_reply =
"<?xml version=\"1.0\"?>\
<!DOCTYPE cross-domain-policy SYSTEM \"/xml/dtds/cross-domain-policy.dtd\">\
<cross-domain-policy>\
<site-control permitted-cross-domain-policies=\"master-only\"/>\
<allow-access-from domain=\"*\" to-ports=\"" + to_ports + "\" />\
</cross-domain-policy>";
	}

	CullResult cull()
	{
		for (std::set<FlashPDSocket*>::const_iterator i = sockets.begin(); i != sockets.end(); ++i)
		{
			FlashPDSocket* sock = *i;
			sock->AddToCull();
		}
		return Module::cull();
	}

	virtual Version GetVersion()
	{
		return Version("Flash Policy Daemon. Allows Flash IRC clients to connect", VF_NONE);
	}
};

MODULE_INIT(ModuleFlashPD)
