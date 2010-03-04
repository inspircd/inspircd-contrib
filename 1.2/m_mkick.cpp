/**
 * m_mkick.cpp
 * 
 * Basic module that adds the MKICK command, allows ops on a a channel to mass kick all non-ops.
 * Cut, and Dry. Note: I've not tested this at all with ~ and other fancy things as we only
 *                     use @ and + here at work. So I have no idea what the outcome would be.
 *
 * Changes:
 * --------------------------
 *
 * author: j. newing (synmuffin)
 * email: jnewing@gmail.com
 *
 * homeless :( I'm currently looking for a network where I can get back into developing InspIRCd modules and working on
 * my Orion Services web front end. 
 *
 */

/* $ModDesc: Provides the /MKICK command, allows ops on a a channel to mass kick all non-ops. */
/* $ModDepends: core 1.2-1.3 */
#include "inspircd.h"

class CommandMkick : public Command {
	
	public:
		CommandMkick (InspIRCd* Instance) : Command(Instance, "MKICK", 0, 1, 2, false)
		{
			this->source = "m_kick.so";
			syntax = "<#channel> [reason]";
		}
		
		CmdResult Handle (const std::vector<std::string>& parameters, User* user)
		{
			Channel* channel = ServerInstance->FindChan(parameters[0]);
			std::string reason = "";
				
			if (channel)
			{
				if (parameters.size() > 1)
					reason = parameters[1];
				
				CUList* ulist = channel->GetUsers();
				
				for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
				{
					if (IS_LOCAL(i->first))
					{
						if (i->first->nick != user->nick && channel->GetPrefixValue(i->first) < OP_VALUE)
							channel->KickUser(user, i->first, reason.c_str());
					}
				}
				
				return CMD_SUCCESS;
				
			}
			else
			{
				user->WriteServ("NOTICE %s :*** Invalid channel", user->nick.c_str());
				return CMD_FAILURE;
			}
			
			return CMD_SUCCESS;
		}

};
		
class ModuleMkick : public Module {
	CommandMkick* mycommand;
	
	public:
		ModuleMkick(InspIRCd* Me) : Module(Me)
		{
			mycommand = new CommandMkick(ServerInstance);
			ServerInstance->AddCommand(mycommand);
		}
		
		virtual ~ModuleMkick(){ }
		
		virtual Version GetVersion()
		{
			return Version("m_mkick ver 1.0 - synmuffin", 0, API_VERSION);
		}
		
};
		
MODULE_INIT(ModuleMkick)

