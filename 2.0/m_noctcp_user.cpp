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

	virtual ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		if ((text.empty()) || (text[0] != '\001') || (!strncmp(text.c_str(), "\1ACTION ", 8)) || (text == "\1ACTION\1") || (text == "\1ACTION"))
			return MOD_RES_PASSTHRU;

		switch (target_type)
		{
			case TYPE_CHANNEL:
			{
				if (operoverride && IS_OPER(user))
					return MOD_RES_PASSTHRU;

				Channel* chan = (Channel*)dest;
				const UserMembList* members = chan->GetUsers();
				for (UserMembCIter member = members->begin(); member != members->end(); ++member)
				{
					User* target = member->first;
					if (target->IsModeSet(ncu.GetModeChar()))
						exempt_list.insert(target);
				}
				break;
			}
			case TYPE_USER:
			{
				if (operoverride && IS_OPER(user))
					return MOD_RES_PASSTHRU;

				User* target = static_cast<User*>(dest);
				if (target->IsModeSet(ncu.GetModeChar()))
				{
					user->WriteNumeric(ERR_NOCTCPALLOWED, "%s %s :User does not accept CTCPs (+%c is set)", user->nick.c_str(), target->nick.c_str(), ncu.GetModeChar());
					return MOD_RES_DENY;
				}
				break;
			}
			case TYPE_SERVER:
				break;
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
