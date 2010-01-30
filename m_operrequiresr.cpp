/* $ModDesc: Make /oper require that a user be identified as a certain user. */
/* $ModAuthor: w00t */
/* $ModAuthorMail: w00t@inspircd.org */
/* $ModDepends: core 1.2 */

#include "inspircd.h"

class ModuleOperReg : public Module
{
	ConfigReader *Conf;
	public:

		virtual int OnPreCommand(std::string &command, std::vector<std::string>& parameters, User* user, bool validated, const std::string &original_line)
		{
			if (!validated)
				return 0;

			if (command != "OPER")
				return 0;

			std::string *account, currentaccount=user->nick.c_str();
			user->GetExt("accountname", account);

			if (account)
				currentaccount=account->c_str();

			else if (!user->IsModeSet('r') && !account)
			{
				user->WriteServ("491 %s :Invalid oper credentials",user->nick.c_str());
				return 1;
			}

			for (int j = 0; j < Conf->Enumerate("oper"); j++)
			{
				std::string opername = Conf->ReadValue("oper", "name", j);
				if (opername==parameters[0])
				{
					std::string registerednick = Conf->ReadValue("oper", "registerednick", "", j);

					if (registerednick.empty() || InspIRCd::Match(registerednick,currentaccount))
						break;
					else
					{
						user->WriteServ("491 %s :Invalid oper credentials",user->nick.c_str());
						return 1;
					}
				}
			}

			return 0;
		}

		ModuleOperReg(InspIRCd* Me) : Module(Me)
		{
			Implementation eventlist[] = { I_OnPreCommand, I_OnRehash};
			ServerInstance->Modules->Attach(eventlist, this, 2);
			Conf = new ConfigReader(ServerInstance);
		}

		virtual void OnRehash(User* user, const std::string &parameter)
		{
			delete Conf;
			Conf = new ConfigReader(ServerInstance);
		}

		virtual Version GetVersion()
		{
			return Version("$Id$",VF_COMMON,API_VERSION);
		}
};

MODULE_INIT(ModuleOperReg)

