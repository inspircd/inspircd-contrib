/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *               m_noctcp_user.cpp
 *
 *  Last updated: October 17th, 2010
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

#include "inspircd.h"

/* $ModDesc: Provides user mode +T to block CTCPs */
/* $ModDepends: core 2.0 */

class NoCTCPuser : public SimpleUserModeHandler
{
public:
	NoCTCPuser(Module* Creator) : SimpleUserModeHandler(Creator, "u_noctcp", 'T') { }
};
class ModuleNoCTCPuser : public Module
{
	NoCTCPuser ncu;
private:
	bool operoverride;
public:

	ModuleNoCTCPuser() : ncu(this)
	{
		OnRehash(NULL);

		ServerInstance->Modules->AddService(ncu);
		Implementation eventlist[] = { I_OnRehash, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual Version GetVersion()
	{
		return Version("Provides user mode +T to block CTCPs");
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
		{
			User* target = static_cast<User*>(dest);
			if (!target->IsModeSet('T'))
				return MOD_RES_PASSTHRU;

			if (operoverride && IS_OPER(user))
				return MOD_RES_PASSTHRU;

			if ((text.length()) && (text[0] == '\1'))
			{
				if (strncmp(text.c_str(),"\1ACTION ", 8))
				{
					user->WriteNumeric(ERR_NOCTCPALLOWED, "%s %s :User does not accept CTCPs",user->nick.c_str(), target->nick.c_str());
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void OnRehash(User* user)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("noctcpuser");
		operoverride = tag->getBool("operoverride");
	}
};

MODULE_INIT(ModuleNoCTCPuser)
