/* **
 * m_chanmode.cpp
 * 
 * Module adds snomasks v and V allows opers to view any and ALL mode changes 
 * that are done by users to channels on the network.
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

/* $ModDesc: Module adds snomasks v and V allows opers to view any and ALL mode changes that are done by users to channels on the network. */
/* $ModDepends: core 1.2-1.3 */

class ModuleChanMode : public Module
{
 public:
	ModuleChanMode(InspIRCd* Me) : Module(Me)
	{
		ServerInstance->SNO->EnableSnomask('v', "CHANMODE");
		ServerInstance->SNO->EnableSnomask('V', "REMOTECHANMODE");
		Implementation eventlist[] = { I_OnMode };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}
	
	virtual ~ModuleChanMode()
	{
		ServerInstance->SNO->DisableSnomask('v');
		ServerInstance->SNO->DisableSnomask('V');
	}
	
	virtual Version GetVersion()
	{
		return Version("m_chanmode ver 1.1 - synmuffin", 0, API_VERSION);
	}
	
	virtual void OnMode(User* user, void* dest, int target_type, const std::deque<std::string> &text, const std::deque<TranslateType> &translate)
	{
		// Mode changes
		std::string output_text;

		// Make the text look nice, I'm 100% sure there is a better way to do this however I don't know it yet :/
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
		
		if (target_type == TYPE_CHANNEL)
		{
			Channel* c = (Channel*)dest;
			
			if (IS_LOCAL(user))
			{
				ServerInstance->SNO->WriteToSnoMask('v', "%s sets mode: %s on channel: %s", user->nick.c_str(), output_text.c_str(), c->name.c_str());
			}
			else
			{
				ServerInstance->SNO->WriteToSnoMask('V', "%s sets mode: %s on channel: %s", user->nick.c_str(), output_text.c_str(), c->name.c_str());
			}
		
		}
	}
		
};

MODULE_INIT(ModuleChanMode)
