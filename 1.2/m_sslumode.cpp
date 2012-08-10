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
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModAuthor: Shawn Smith */
/* $ModAuthorMail: Shawn@inspircd.org */
/* $ModDepends: core 1.2-1.3 */
/* $ModDesc: Provides support for ssl-only queries and notices. (umode: z) */

static char* dummy;

/** Handle user mode +z
 */

class SSLModeUser : public ModeHandler
{
public:
	SSLModeUser(InspIRCd* Instance) : ModeHandler(Instance, 'z', 0, 0, false, MODETYPE_USER, false) { }
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool)
	{
		if (adding)
		{
			/* Make sure user is on an ssl connection when setting */
			if (!dest->IsModeSet('z') && dest->GetExt("ssl", dummy))
			{
				dest->SetMode('z', true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('z'))
			{
				dest->SetMode('z', false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleSSLModes : public Module
{

	SSLModeUser* sslquery;

 public:
	ModuleSSLModes(InspIRCd* Me)
		: Module(Me)
	{


		sslquery = new SSLModeUser(ServerInstance);

		if (!ServerInstance->Modes->AddMode(sslquery))
			throw ModuleException("Could not add new modes!");

		Implementation eventlist[] = { I_OnUserPreNotice, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}


	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_USER)
		{
			User* t = (User*)dest;
			if (t->IsModeSet('z') && !ServerInstance->ULine(user->server))
			{
				if (!user->GetExt("ssl", dummy))
				{
					user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You are not permitted to send private messages to this user (+z set)", user->nick.c_str(), t->nick.c_str());
					return 1;
				}
			}
			else if (user->IsModeSet('z') && !ServerInstance->ULine(t->server))
			{
				if (!t->GetExt("ssl", dummy))
				{
					user->WriteNumeric(ERR_CANTSENDTOUSER, "%s %s :You must remove usermode 'z' before you are able to send privates messages to a non-ssl user.", user->nick.c_str(), t->nick.c_str());
					return 1;
				}
			}
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		return OnUserPreMessage(user, dest, target_type, text, status, exempt_list);
	}

	virtual ~ModuleSSLModes()
	{
		ServerInstance->Modes->DelMode(sslquery);
		delete sslquery;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};


MODULE_INIT(ModuleSSLModes)

