/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* $ModAuthor: genius3000 */
/* $ModAuthorMail: genius3000@g3k.solutions */
/* $ModDesc: Provides support for showing text files to users */
/* $ModDepends: core 2.0 */
/* $ModConfig: <showfile name="DAILYNEWS" file="conf/dailynews.txt"> */

/* Configuration explained further:
 * name   - The name of the command which displays this file.
 * file   - The text file to be shown (formatting codes allowed). This will
 *          default to the same as 'name' if not specified.
 * method - How should the file be shown? Options are:
 *          * numeric: Send contents using a numeric, this is the default.
 *          * notice:  Send contents as a series of notices.
 *          * msg:     Send contents as a series of private messages.
 * When using the method of "numeric", you can also set the following:
 * introtext    - Introductory line, "Showing <name>" by default.
 * intronumeric - Numeric used for the introductory line, 308 by default.
 * numeric      - Numeric used for sending the text itself, 232 by default.
 * endtext      - Ending line, "End of <name>" by default.
 * endnumeric   - Numeric used for the ending line, 309 by default.
 */

#include "inspircd.h"


class CommandShowFile : public Command
{
	enum Method
	{
		SF_MSG,
		SF_NOTICE,
		SF_NUMERIC
	};

	std::string introtext;
	std::string endtext;
	unsigned int intronumeric;
	unsigned int textnumeric;
	unsigned int endnumeric;
	file_cache contents;
	Method method;

 public:
	CommandShowFile(Module* Creator, const std::string& cmdname)
		: Command(Creator, cmdname)
	{
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User* user)
	{
		if (method == SF_NUMERIC)
		{
			if (!introtext.empty())
				user->WriteNumeric(intronumeric, "%s :%s", user->nick.c_str(), introtext.c_str());

			for (file_cache::const_iterator i = contents.begin(); i != contents.end(); ++i)
				user->WriteNumeric(textnumeric, "%s :- %s", user->nick.c_str(), i->c_str());

			user->WriteNumeric(endnumeric, "%s :%s", user->nick.c_str(), endtext.c_str());
		}
		else
		{
			const std::string msgcmd = (method == SF_MSG ? "PRIVMSG" : "NOTICE");
			for (file_cache::const_iterator i = contents.begin(); i != contents.end(); ++i)
				user->WriteServ("%s %s :%s", msgcmd.c_str(), user->nick.c_str(), i->c_str());
		}
		return CMD_SUCCESS;
	}

	void UpdateSettings(ConfigTag* tag, const file_cache& filecontents)
	{
		introtext = tag->getString("introtext", "Showing " + name);
		endtext = tag->getString("endtext", "End of " + name);
		intronumeric = tag->getInt("intronumeric", RPL_RULESTART);
		textnumeric = tag->getInt("numeric", RPL_RULES);
		endnumeric = tag->getInt("endnumeric", RPL_RULESEND);
		std::string smethod = tag->getString("method");

		if (intronumeric < 0 || intronumeric > 999)
			intronumeric = RPL_RULESTART;

		if (textnumeric < 0 || textnumeric > 999)
			textnumeric = RPL_RULES;

		if (endnumeric < 0 || endnumeric > 999)
			endnumeric = RPL_RULESEND;

		method = SF_NUMERIC;
		if (smethod == "msg")
			method = SF_MSG;
		else if (smethod == "notice")
			method = SF_NOTICE;

		contents = filecontents;
		InspIRCd::ProcessColors(contents);
	}
};

class ModuleShowFile : public Module
{
	std::vector<CommandShowFile*> cmds;

	void ReadTag(ConfigTag* tag, std::vector<CommandShowFile*>& newcmds)
	{
		std::string cmdname = tag->getString("name");
		if (cmdname.empty())
			throw ModuleException("Empty value for 'name'");

		std::transform(cmdname.begin(), cmdname.end(), cmdname.begin(), ::toupper);

		const std::string file = tag->getString("file", cmdname);
		if (file.empty())
			throw ModuleException("Empty value for 'file'");
		FileReader reader(file);

		CommandShowFile* sfcmd;
		Command* handler = ServerInstance->Parser->GetHandler(cmdname);
		if (handler)
		{
			// Command exists, check if it is ours
			if (handler->creator != this)
				throw ModuleException("Command " + cmdname + " already exists");

			// This is our command, make sure we don't have the same entry twice
			sfcmd = static_cast<CommandShowFile*>(handler);
			if (std::find(newcmds.begin(), newcmds.end(), sfcmd) != newcmds.end())
				throw ModuleException("Command " + cmdname + " is already used in a <showfile> tag");
		}
		else
		{
			// Command doesn't exist, create it
			sfcmd = new CommandShowFile(this, cmdname);
			ServerInstance->Modules->AddService(*sfcmd);
		}

		file_cache contents;
		for (unsigned int line = 0; line < (unsigned)reader.FileSize(); ++line)
			contents.push_back(reader.GetLine(line));

		sfcmd->UpdateSettings(tag, contents);
		newcmds.push_back(sfcmd);
	}

	static void DelAll(const std::vector<CommandShowFile*>& list)
	{
		for (std::vector<CommandShowFile*>::const_iterator i = list.begin(); i != list.end(); ++i)
			delete *i;
	}

 public:
	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modules->Attach(I_OnRehash, this);
	}

	void OnRehash(User*)
	{
		std::vector<CommandShowFile*> newcmds;

		ConfigTagList tags = ServerInstance->Config->ConfTags("showfile");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			try
			{
				ReadTag(tag, newcmds);
			}
			catch (CoreException& ex)
			{
				ServerInstance->Logs->Log("MODULE", DEFAULT, "Error: %s at %s", ex.GetReason(), tag->getTagLocation().c_str());
			}
		}

		// Remove all commands that were removed from the config
		std::vector<CommandShowFile*> removed(cmds.size());
		std::sort(newcmds.begin(), newcmds.end());
		std::set_difference(cmds.begin(), cmds.end(), newcmds.begin(), newcmds.end(), removed.begin());

		DelAll(removed);
		cmds.swap(newcmds);
	}

	~ModuleShowFile()
	{
		DelAll(cmds);
	}

	Version GetVersion()
	{
		return Version("Provides support for showing text files to users");
	}
};

MODULE_INIT(ModuleShowFile)
