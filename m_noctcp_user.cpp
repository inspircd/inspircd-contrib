/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *               m_noctcp_user.cpp
 *
 *  Created on: ???
 *      Author: Josh - http://snofox.net/
 *      Other software @ http://sleepyirc.net/irc/
 *  This software is available under the GNU GPLv2 license
 *  	Enjoy!
 *
 *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *  *
 *
 * Configuration line:
 * 	<noctcpuser operoverride="yes/no">
 *  	Should Opers override the block?
 *  	Defaults to "no"
 *
*/
/* ***Newb notice!***
 * This was a slight rewrite of m_noctcp.cpp
 * 'Twas my first module. Leave me alone.
 * It works like a charm. ;)
 */
#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style user mode +T */
/* $ModAuthor: SnoFox */
/* $ModAuthorMail: josh@sleepyirc.net */
/* $ModDepends: core 1.2 */

class NoCTCPuser : public SimpleUserModeHandler
{
public:
	NoCTCPuser(InspIRCd* Instance) : SimpleUserModeHandler(Instance, 'T') { }
};
class ModuleNoCTCPuser : public Module
{

	NoCTCPuser* ncu;
private:
	bool operoverride;
public:

	ModuleNoCTCPuser(InspIRCd* Me)
		: Module(Me)
	{
		OnRehash(NULL);

		ncu = new NoCTCPuser(ServerInstance);
		if (!ServerInstance->Modes->AddMode(ncu))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnRehash, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleNoCTCPuser()
	{
		ServerInstance->Modes->DelMode(ncu);
		delete ncu;
	}

	virtual Version GetVersion()
	{
		return Version("$Id: m_noctcpuser.cpp 1 2009-05-1 10:53:22Z SnoFox $", VF_COMMON, API_VERSION);
	}

	virtual int OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
		{
			if (!((User*)dest)->IsModeSet('T'))
				return 0;

			if (operoverride && IS_OPER(user))
				return 0;

				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						user->WriteNumeric(ERR_NOCTCPALLOWED, "%s %s :User does not accept CTCPs",user->nick.c_str(), ((User*)dest)->nick.c_str());
						return 1;
					}
				}
			}
		return 0;
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);
		operoverride = Conf.ReadFlag("noctcpuser","operoverride", "0", 0);
	}
};

MODULE_INIT(ModuleNoCTCPuser)
