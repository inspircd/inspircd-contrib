/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "wildcard.h"

/* $ModDesc: Provides ircd-side fantasy commands. */
/* $ModAuthor: w00t */
/* $ModAuthorMail: w00t@inspircd.org */
/* $ModDepends: core 1.1 */
/* $ModVersion: $Rev: 78 $ */

/** Fantasy command definition
 */
class FantasyCommand : public classbase
{
 public:
	/** The text of the fantasy command */
	irc::string text;
	/** Text to replace with */
	std::string replace_with;
	/** Requires oper? */
	bool operonly;
	/* is case sensitive params */
	bool case_sensitive;
	/** Format that must be matched for use */
	std::string format;
};

class ModuleAlias : public Module
{
 private:
	/** We cant use a map, there may be multiple aliases with the same name */
	std::vector<FantasyCommand> FantasyCommands;
	std::map<std::string, int> FantasyMap;
	std::vector<std::string> pars;

	virtual void ReadAliases()
	{
		ConfigReader MyConf(ServerInstance);

		FantasyCommands.clear();
		FantasyMap.clear();
		for (int i = 0; i < MyConf.Enumerate("fcommand"); i++)
		{
			FantasyCommand a;
			std::string txt;
			txt = MyConf.ReadValue("fcommand", "text", i);
			std::transform(txt.begin(), txt.end(), txt.begin(), ::toupper);
			a.text = txt.c_str();
			a.replace_with = MyConf.ReadValue("fcommand", "replace", i, true);
			a.operonly = MyConf.ReadFlag("fcommand", "operonly", i);
			a.format = MyConf.ReadValue("fcommand", "format", i);
			a.case_sensitive = MyConf.ReadFlag("fcommands", "matchcase", i);
			FantasyCommands.push_back(a);
			FantasyMap[txt] = 1;
		}
	}

 public:
	
	ModuleAlias(InspIRCd* Me) : Module(Me)
	{
		ReadAliases();
		pars.resize(127);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnRehash] = 1;
	}

	virtual ~ModuleAlias()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}

	std::string GetVar(std::string varname, const std::string &original_line)
	{
		irc::spacesepstream ss(original_line);
		varname.erase(varname.begin());
		int index = *(varname.begin()) - 48;
		varname.erase(varname.begin());
		bool everything_after = (varname == "-");
		std::string word;

		for (int j = 0; j < index; j++)
			ss.GetToken(word);

		if (everything_after)
		{
			std::string more;
			while (ss.GetToken(more))
			{
				word.append(" ");
				word.append(more);
			}
		}

		return word;
	}

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

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type != TYPE_CHANNEL)
		{
			ServerInstance->Log(DEBUG, "fantasy: not a channel msg");
			return 0;
		}

		// fcommands are only for local users. Spanningtree will send them back out as their original cmd.
		if (!IS_LOCAL(user))
		{
			ServerInstance->Log(DEBUG, "fantasy: not local");
			return 0;
		}

		chanrec* c = (chanrec*)dest;
		std::string fcommand;

		// text is like "!moo cows bite me", we want "!moo" first
		irc::spacesepstream ss(text);
		ss.GetToken(fcommand);

		if (fcommand.empty())
		{
			ServerInstance->Log(DEBUG, "fantasy: empty (??)");
			return 0; // wtfbbq
		}

		ServerInstance->Log(DEBUG, "fantasy: looking at fcommand %s", fcommand.c_str());

		// we don't want to touch non-fantasy stuff
		if (*fcommand.c_str() != '!')
		{
			ServerInstance->Log(DEBUG, "fantasy: not a fcommand");
			return 0;
		}

		// nor do we give a shit about the !
		fcommand.erase(fcommand.begin());
		std::transform(fcommand.begin(), fcommand.end(), fcommand.begin(), ::toupper);
		ServerInstance->Log(DEBUG, "fantasy: now got %s", fcommand.c_str());

		/* We dont have any commands looking like this, no point continuing.. */
		if (FantasyMap.find(fcommand) == FantasyMap.end())
			return 0;

		ServerInstance->Log(DEBUG, "fantasy: in the map");

		/* The parameters for the command in their original form, with the command stripped off */
		std::string compare = text.substr(fcommand.length() + 1);
		while (*(compare.c_str()) == ' ')
			compare.erase(compare.begin());

		std::string safe(compare);

		/* Escape out any $ symbols in the user provided text (ugly, but better than crashy) */
		SearchAndReplace(safe, "$", "\r");

		ServerInstance->Log(DEBUG, "fantasy: compare is %s and safe is %s", compare.c_str(), safe.c_str());

		for (unsigned int i = 0; i < FantasyCommands.size(); i++)
		{
			ServerInstance->Log(DEBUG, "fantasy: looking for %s, current item is %s", fcommand.c_str(), FantasyCommands[i].text.c_str());
			if (!strcasecmp(FantasyCommands[i].text.c_str(), fcommand.c_str())) // XXX a stl comparison would be nicer, but I can't get it working right now.
			{
				/* Does it match the pattern? */
				if (!FantasyCommands[i].format.empty())
				{
					if (!match(FantasyCommands[i].case_sensitive, compare.c_str(), FantasyCommands[i].format.c_str()))
					{
						ServerInstance->Log(DEBUG, "fantasy: no match on pattern %s (comparing %s)", FantasyCommands[i].format.c_str(), compare.c_str());
						continue;
					}
				}

				if ((FantasyCommands[i].operonly) && (!IS_OPER(user)))
				{
					ServerInstance->Log(DEBUG, "fantasy: oper only");
					return 0;
				}

				/* Now, search and replace in a copy of the original_line, replacing $1 through $9 and $1- etc */
				std::string::size_type crlf = FantasyCommands[i].replace_with.find('\n');

				if (crlf == std::string::npos)
				{
					ServerInstance->Log(DEBUG, "fantasy: running it");
					DoCommand(FantasyCommands[i].replace_with, user, c, safe);
					return 0;
				}
				else
				{
					irc::sepstream commands(FantasyCommands[i].replace_with, '\n');
					std::string command;
					while (commands.GetToken(command))
					{
						DoCommand(command, user, c, safe);
					}
					return 0;
				}
			}
		}
		return 0;
	}

	void DoCommand(std::string newline, userrec* user, chanrec *c,const std::string &original_line)
	{
		for (int v = 1; v < 10; v++)
		{
			std::string var = "$";
			var.append(ConvToStr(v));
			var.append("-");
			std::string::size_type x = newline.find(var);

			while (x != std::string::npos)
			{
				newline.erase(x, var.length());
				newline.insert(x, GetVar(var, original_line));
				x = newline.find(var);
			}

			var = "$";
			var.append(ConvToStr(v));
			x = newline.find(var);

			while (x != std::string::npos)
			{
				newline.erase(x, var.length());
				newline.insert(x, GetVar(var, original_line));
				x = newline.find(var);
			}
		}

		/* Special variables */
		SearchAndReplace(newline, "$nick", user->nick);
		SearchAndReplace(newline, "$ident", user->ident);
		SearchAndReplace(newline, "$host", user->host);
		SearchAndReplace(newline, "$vhost", user->dhost);
		SearchAndReplace(newline, "$chan", c->name);

		/* Unescape any variable names in the user text before sending */
		SearchAndReplace(newline, "\r", "$");

		irc::tokenstream ss(newline);
		const char* parv[127];
		int x = 0;

		while (ss.GetToken(pars[x]))
		{
			parv[x] = pars[x].c_str();
			x++;
		}

		ServerInstance->Parser->CallHandler(parv[0], &parv[1], x-1, user);
	}
 
	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		ReadAliases();
 	}
};

MODULE_INIT(ModuleAlias)

