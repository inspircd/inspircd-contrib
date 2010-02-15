/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * Module Author:  Jens Voss ( DukePyrolator@anope.org )
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "transport.h"

/* $ModDesc: auto-opers any user if his SSL fingerprint matches with the fingerprint in an oper block */
/* $ModDepends: core 1.2-1.3 */

class ModuleSSLAutoOper : public Module
{
 public:
	ModuleSSLAutoOper(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnPostConnect };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	virtual ~ModuleSSLAutoOper()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", NULL, API_VERSION);
	}

	void Prioritize()
	{
		Module* sslmodule;
		sslmodule = ServerInstance->Modules->Find("m_ssl_gnutls.so");
		if (!sslmodule)
			sslmodule = ServerInstance->Modules->Find("m_ssl_openssl.so");
		ServerInstance->Modules->SetPriority(this, I_OnPostConnect, PRIORITY_AFTER, &sslmodule);
	}

	bool OneOfMatches(const char* host, const char* ip, const char* hostlist)
	{
		std::stringstream hl(hostlist);
		std::string xhost;
		while (hl >> xhost)
		{
			if (InspIRCd::Match(host, xhost, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(ip, xhost, ascii_case_insensitive_map))
			{
				return true;
			}
		}
		return false;
	}

	virtual void OnPostConnect(User *user)
	{
		ConfigReader cf(ServerInstance);
		std::string TheHost;
		std::string TheIP;
		std::string LoginName;
		std::string OperType;
		std::string HostName;
		std::string FingerPrint;
		ssl_cert* cert = NULL;

		user->GetExt("ssl_cert",cert);

		if (!cert)
			return;

		for (int i = 0; i < cf.Enumerate("oper"); i++)
		{

			LoginName = cf.ReadValue("oper", "name", i);
			OperType = cf.ReadValue("oper", "type", i);
			HostName = cf.ReadValue("oper", "host", i);
			FingerPrint = cf.ReadValue("oper", "fingerprint", i);

			if (FingerPrint.empty())
				continue;

			/* check if the fingerprint matches */
			if (cert->GetFingerprint() == FingerPrint)
			{
				/* now check if the user is using an allowed hostname/IP */
				TheHost = user->ident + "@" + user->host;
				TheIP = user->ident + "@" + user->GetIPString();
				if (!OneOfMatches(TheHost.c_str(), TheIP.c_str(), HostName.c_str()))
				{
					user->WriteNumeric(491, "%s :Invalid oper credentials, your host is not on your oper block!", user->nick.c_str());
					return;
				}

				/* get the right oper type and class from the config */
				char TypeName[MAXBUF];
				char ClassName[MAXBUF];

				for (int j = 0; j < ServerInstance->Config->ConfValueEnum("type"); j++)
				{
					ServerInstance->Config->ConfValue("type", "name", j, TypeName, MAXBUF);
					ServerInstance->Config->ConfValue("type", "class", j, ClassName, MAXBUF);

					if (!OperType.compare(TypeName))
					{
						/* found this oper's opertype */
						if (!ServerInstance->IsNick(TypeName, ServerInstance->Config->Limits.NickMax))
							return;
						char OperHost[MAXBUF];
						ServerInstance->Config->ConfValue("type","host", j, OperHost, MAXBUF);
						if (*OperHost)
							user->ChangeDisplayedHost(OperHost);
						if (*ClassName)
						{
							user->SetClass(ClassName);
							user->CheckClass();
						}
						user->Oper(OperType, LoginName);
						return;
					}
				} // for "type"
			} // if (cert->GetFingerprint() == FingerPrint)
		} // for (int i = 0; i < cf.Enumerate("oper"); i++)
		return;
	} // virtual void OnUserConnect(User *user)

};

MODULE_INIT(ModuleSSLAutoOper)

