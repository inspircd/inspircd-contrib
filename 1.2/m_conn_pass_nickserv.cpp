#include "inspircd.h"

/* $ModDesc: Forwards NickServ credentials from PASS */
/* $ModAuthor: satmd */
/* $ModAuthorMail: http://lain.at/blog/ */
/* $ModDepends: core 1.2 */

void SearchAndReplace(std::string& newline, const std::string &find, const std::string &replace)
{
	std::string::size_type x = newline.find(find);
	while (x != std::string::npos)
	{
		newline.erase(x, find.length());
		newline.insert(x, replace);
		x = newline.find(find);
	}
}


class ModuleConnPassNickserv : public Module
{
	private:
		std::string nickrequired, unavailablemsg, forwardmsg, forwardcmd;

	public:
		ModuleConnPassNickserv(InspIRCd* Me) : Module(Me)
		{
			OnRehash(NULL,"");
			Implementation eventlist[] = { I_OnPostConnect, I_OnRehash };
			ServerInstance->Modules->Attach(eventlist, this, 2);
		}

		virtual ~ModuleConnPassNickserv()
		{
		}

		virtual Version GetVersion()
		{
			return Version("$Id$", 0, API_VERSION);
		}

		virtual void OnRehash(User* user, const std::string &param)
		{
			ConfigReader Conf(ServerInstance);
			nickrequired = Conf.ReadValue("passnickserv", "nick", "NickServ", 0);
			unavailablemsg = Conf.ReadValue("passnickserv", "nonickmsg", "401 $nick $nickrequired :is currently unavailable. Please try again later.", 0);
			forwardmsg = Conf.ReadValue("passnickserv", "forwardmsg", "NOTICE $nick :*** Forwarding PASS to $nickrequired", 0);
			forwardcmd = Conf.ReadValue("passnickserv", "cmd", "PRIVMSG $nickrequired :IDENTIFY $pass", 0);
		}

		void FormatStr(std::string& newline, const std::string &nick, const std::string &nickrequired, const std::string &pass)
		{
			SearchAndReplace(newline,"$nickrequired",nickrequired);
			SearchAndReplace(newline,"$nick",nick);
			SearchAndReplace(newline,"$pass",pass);
		}

		virtual void OnPostConnect(User* user)
		{
			if (!IS_LOCAL(user))
				return;

			if (user->password.empty())
				return;

			if (!nickrequired.empty())
			{
				/* Check if nick exists and its server is ulined */
				User* u = ServerInstance->FindNick(nickrequired.c_str());
				if (!u || !ServerInstance->ULine(u->server))
				{
					std::string tmp(unavailablemsg);
					FormatStr(tmp,user->nick,nickrequired,user->password);
					user->WriteServ(tmp);
					return;
				}
			}

			std::string tmp(forwardmsg);
			FormatStr(tmp,user->nick,nickrequired,user->password);
			user->WriteServ(tmp);

			tmp.assign(forwardcmd);
			FormatStr(tmp,user->nick,nickrequired,user->password);
			ServerInstance->Parser->ProcessBuffer(tmp,user);
		}
};

MODULE_INIT(ModuleConnPassNickserv)

