/* **
 * m_usermode.cpp
 * 
 * Module adds snomasks u and U allows opers to view any and ALL mode changes 
 * that are done by users on the network.
 *
 * Changes:
 * --------------------------
 * 1.1: - Fixed UUID display
 *			- Made the output look all purdy with some nice formatting
 *
 * author: j. newing (synmuffin)
 * email: jnewing@gmail.com
 *
 * homeless :( I'm currently looking for a network that where I can get back into developing InspIRCd modules and working on
 * my Orion Services web front end. 
 *
 */
#include "inspircd.h"

/* $ModDesc: Module adds snomasks u and U allows opers to view any and ALL mode changes that are done by users on the network. */
/* $ModDepends: core 1.2-1.3 */

class ModuleUserMode : public Module
{
 public:
	ModuleUserMode(InspIRCd* Me) : Module(Me)
	{
		ServerInstance->SNO->EnableSnomask('u', "USREMODE");
		ServerInstance->SNO->EnableSnomask('U', "REMOTEUSREMODE");
		Implementation eventlist[] = { I_OnMode };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}
	
	virtual ~ModuleUserMode()
	{
		ServerInstance->SNO->DisableSnomask('u');
		ServerInstance->SNO->DisableSnomask('U');
	}
	
	virtual Version GetVersion()
	{
		return Version("m_usermode ver 1.1 - synmuffin", 0, API_VERSION);
	}
	
	virtual void OnMode(User* user, void* dest, int target_type, const std::deque<std::string> &text, const std::deque<TranslateType> &translate)
	{
		// Mode chances
		std::string output_text;
    
    std::deque<std::string>::const_iterator i;
		for (i = text.begin(); i != text.end(); i++)
		{
			if (*i == text.back())
				output_text.append(*i);
			else if (*i == text.front())
				output_text.append(*i + " ");
			else
				output_text.append(*i + ", ");
		}

		if (target_type == TYPE_USER)
		{
			User* u = (User*)dest;
			
			if (IS_LOCAL(user))
			{
				ServerInstance->SNO->WriteToSnoMask('u', "%s set mode: %s to: %s", user->nick.c_str(), output_text.c_str(), u->nick.c_str());
			}
			else
			{
				ServerInstance->SNO->WriteToSnoMask('U', "%s set mode: %s to: %s", user->nick.c_str(), output_text.c_str(), u->nick.c_str());
			}
		}
	}
};

MODULE_INIT(ModuleUserMode)
