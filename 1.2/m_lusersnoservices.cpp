/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "modules.h"

/* $ModDesc: Adds an extra line to LUSERS output to show global user count minus "clients" from U-Lined servers */
/* $ModAuthor: Naram Qashat (CyberBotX) */
/* $ModAuthorMail: cyberbotx@cyberbotx.com */
/* $ModDepends: core 1.2-1.3 */

class LusersWithoutServicesModule : public Module
{
public:
	LusersWithoutServicesModule(InspIRCd *Me) : Module(Me)
	{
		Me->Modules->Attach(I_OnPreCommand, this);
	}

	virtual ~LusersWithoutServicesModule() { }

	virtual int OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return 0;

		if (command == "LUSERS")
		{
			/* This tries to get the LUSERS of the m_spanningtree module output before ours */
			Module *spanningtree = ServerInstance->Modules->Find("m_spanningtree.so");
			if (spanningtree)
				spanningtree->OnPreCommand(command, parameters, user, validated, original_line);
			else
			{
				std::vector<std::string> emptyParameters;
				ServerInstance->CallCommandHandler(command, emptyParameters, user);
			}
			/* Calculate how many clients are not psuedo-clients introduced by the Services package */
			int ClientsNoServices = 0;
			for (user_hash::iterator n = ServerInstance->Users->clientlist->begin(); n != ServerInstance->Users->clientlist->end(); ++n)
			{
				if (!ServerInstance->ULine(n->second->server) && !ServerInstance->ULine(n->second->nick.c_str()))
					++ClientsNoServices;
			}
			user->WriteServ("267 %s :Current Global Users (Excluding Services): %d", user->nick.c_str(), ClientsNoServices);
			return 1;
		}

		return 0;
	}

	virtual void Prioritize()
	{
		Module *um = ServerInstance->Modules->Find("m_spanningtree.so");
		ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_BEFORE, &um);
	}

	virtual Version GetVersion()
	{
		return Version("1.0", 0, API_VERSION);
	}
};

MODULE_INIT(LusersWithoutServicesModule)
