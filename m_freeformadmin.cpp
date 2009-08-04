#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Allows a freeform reply to the /ADMIN command */
/* $ModAuthor: jackmcbarn */
/* $ModAuthorMail: w00t@inspircd.org */
/* $ModDepends: core 1.1 */

class ModuleFreeformAdmin : public Module
{

	std::string admintext;
 public:
	ModuleFreeformAdmin(InspIRCd* Me) : Module(Me)
	{
		this->OnRehash(NULL,"");
	}
	
	virtual ~ModuleFreeformAdmin()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, 0, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnPostCommand] = 1;
	}

	virtual void OnPostCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, CmdResult result, const std::string &original_line)
	{
		if (command != "ADMIN") return;
		std::string adminstr = "304 " + std::string(user->nick) + " :ADMIN ";
		irc::sepstream stream(admintext, '\n');
		std::string token = "*";
		while (stream.GetToken(token))
			user->WriteServ("%s%s", adminstr.c_str(), token.c_str());
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader myConfigReader = ConfigReader(ServerInstance);
		admintext = myConfigReader.ReadValue("admin", "text", "Misconfigured", 0, true);
	}
	
};

MODULE_INIT(ModuleFreeformAdmin)
