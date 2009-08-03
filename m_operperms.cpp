/* m_operperms - Written by Om <omster@gmail.com> - April 2005 */
/* Updated by Om <omster@gmail.com> - Feburary 2006 */

#include <string>
#include <set>
#include "inspircd.h"
#include "modules.h"

/* $ModDesc: Provides an /operperms command, allowing opers to view what commands they and other opers have access to. */
/* $ModAuthor: Om */
/* $ModAuthorMail: om@inspircd.org */
/* $ModDepends: core 1.1 */
/* $ModVersion: $Rev: 78 $ */

typedef std::set<std::string> strset;

inline strset& split(strset &out, const std::string &in)
{
	unsigned int a = 0;
	unsigned int b = 0;
	
	while(b <= in.size())
	{
		if(in[b] == ' ' || in[b] == '\t' || b == in.size())
		{
			out.insert(in.substr(a, b-a));
			a = b+1;		
		}
		
		b++;
	}
	
	return out;
}

class ModuleOperCmdsBase
{
 public:
	ConfigReader* Conf;
	InspIRCd* Srv;
	int perline;
};

class cmd_operperms : public command_t
{
	ModuleOperCmdsBase* base;
	
 public:
	cmd_operperms(ModuleOperCmdsBase* b)
	: command_t(b->Srv, "OPERPERMS", 'o', 1), base(b)
	{
		this->source = "m_operperms.so";
	}
	
	CmdResult Handle(const char** parameters, int pcnt, userrec* user)
	{
		// Find a userrec for the parameter.
		userrec* target = base->Srv->FindNick(parameters[0]);
		// Lists of classes and commands for the oper
		strset classlist;
		strset commandlist;
	
		if(!target)
		{
			// If the user was invalid tell the person who used the command that it was invalid...and stop executing the command.
			user->WriteServ("401 %s %s :No such nick/channel", user->nick, parameters[0]);
			return CMD_FAILURE;
		}
	
		if(!*target->oper)
		{
			// If target->oper is null then the user isn't an oper.
			user->WriteServ("NOTICE %s :%s is not an oper", user->nick, target->nick);
			return CMD_FAILURE;
		}
	
		base->Srv->WriteOpers("*** %s used /OPERPERMS to view permissions of %s", user->nick, target->nick);
	
		for(int i = 0; i < base->Conf->Enumerate("type"); i++)
		{
			// For each oper type in the configuration file.
		
			if(base->Conf->ReadValue("type", "name", i) == target->oper)
			{
				// If the <type> tag in the config corresponds to the oper type of the target, eg, we've found the user's oper type
			
				// Reset the error.
				base->Conf->GetError();
				long level = base->Conf->ReadInteger("type", "level", i, false);
				if(base->Conf->GetError() == 0)
					user->WriteServ("NOTICE %s :%s's oper level is %ld", user->nick, target->nick, level);
				else
					user->WriteServ("NOTICE %s :%s does not have an oper level", user->nick, target->nick);
							
				// Read the classes the target oper has.
				split(classlist, base->Conf->ReadValue("type", "classes", i));
				
				for(int j = 0; j < base->Conf->Enumerate("class"); j++)
				{
					// Loop through all classes in the config..				
					if(classlist.count(base->Conf->ReadValue("class", "name", j)))
					{
						// And if our user has that oper class				
						std::string classcmds = base->Conf->ReadValue("class", "commands", j);
						
						if(classcmds.size())
							split(commandlist, classcmds);
					}
				}
				
				int num = 0;
				std::string line;
				
				for(strset::iterator iter = commandlist.begin(); iter != commandlist.end(); iter++)
				{
					// For each one of the commands the oper has access to.
					if(num < base->perline)
					{
						if(base->Srv->Parser->cmdlist.count(*iter))
							line.append(" " + *iter);
						else
							line.append(" [" + *iter + "]");
											
						num++;
					}
					else
					{
						user->WriteServ("NOTICE %s :%s", user->nick, line.c_str());
						line = " " + *iter;
						num = 1;
					}					
				}

				for(nspace::hash_map<std::string,command_t*>::iterator iter = base->Srv->Parser->cmdlist.begin(); iter != base->Srv->Parser->cmdlist.end(); iter++)
				{
					// For each one of the commands the oper doesn't have access to.
					if((iter->second->flags_needed == 'o') && !commandlist.count(iter->first))
					{						
						if(num < base->perline)
						{
							line.append(" (" + iter->first + ")");
							num++;
						}
						else
						{
							user->WriteServ("NOTICE %s :%s", user->nick, line.c_str());
							line = " " + iter->first;
							num = 1;
						}
					}
				}

				// All done, and we don't want to carry on reading <type> tags, so return.
				user->WriteServ("NOTICE %s :End of %s's allowed command list", user->nick, target->nick);
				return CMD_SUCCESS;
			}
		}
		
		// We shouldn't ever get here, as the handler should return after it finds the <type> tag.
		base->Srv->Log(DEBUG, "m_operperms.so: Oper type not found in config for a user who has +o, something is messed up...");
		return CMD_FAILURE;
	}
};

class ModuleOperCmds : public Module, public ModuleOperCmdsBase
{
	cmd_operperms* cmd;
	
public:
	ModuleOperCmds(InspIRCd* serv)
	: Module::Module(serv)
	{
		Srv = serv;
		Conf = new ConfigReader(Srv);
		cmd = new cmd_operperms(this);
		
		OnRehash(NULL, "");
		
		Srv->AddCommand(cmd);
	}
	
	void Implements(char* List)
	{
		List[I_OnRehash] = 1;
	}
	
	virtual ~ModuleOperCmds()
	{
		delete Conf;
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 2, 0, API_VERSION);
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		delete Conf;
		Conf = new ConfigReader(Srv);
		
		perline = Conf->ReadInteger("operperms", "perline", 0, false);
		
		if(perline <= 0 || perline > 10)
			perline = 3;			
	}  	
};

MODULE_INIT(ModuleOperCmds)
