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

/* $ModDesc: Provides support for unreal-style user mode +T */
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

		if (!ServerInstance->Modes->AddMode(&ncu))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnRehash, I_OnUserPreMessage };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual Version GetVersion()
	{
		return Version("m_noctcpuser.cpp 2 2010-10-17 SnoFox", VF_OPTCOMMON);
	}

	virtual ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
		{
			if (!((User*)dest)->IsModeSet('T'))
				return MOD_RES_PASSTHRU;

			if (operoverride && IS_OPER(user))
				return MOD_RES_PASSTHRU;

				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						user->WriteNumeric(ERR_NOCTCPALLOWED, "%s %s :User does not accept CTCPs",user->nick.c_str(), ((User*)dest)->nick.c_str());
						return MOD_RES_DENY;
					}
				}
			}
		return MOD_RES_PASSTHRU;
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader Conf;
		operoverride = Conf.ReadFlag("noctcpuser","operoverride", "0", 0);
	}
};

MODULE_INIT(ModuleNoCTCPuser)
