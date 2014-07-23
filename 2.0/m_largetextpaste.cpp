/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Anthony Magee <macawm@gmail.com>
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

/* $ModConfig: <largetextpaste service="http://service.com" cmd="/path/to/command" sniplen="60" cutofflen="300"> */
/* $ModDesc: Allows server to catch large PRIVMSG text and send them off to a pastebin-like service and replace the text with a shortened version including said url to text entirety. Defaults to using 'pastebinit' command */
/* $ModAuthor: macawm */
/* $ModAuthorMail: macawm@gmail.com */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class ModuleLargeTextPaste : public Module
{
    private:
        // A URL to point to a different service if the default is not acceptable
        std::string serviceUrl;

        // The command line utilities path (full path not needed, but is recommended)
        // Defaults to 'pastebinit'
        std::string cmdLineCommand;

        // The amount of the original text to post in the modified PRIVMSG (unit: char)
        // Defaults to 60 chars
        unsigned int snipLen;

        // The length of text that triggers the pastebin shortening (unit: char)
        // Defaults to 300 chars
        unsigned int cutoffLen;

        // Flag to indicate that the command binary was found
        bool commandAvailable;

        void _readConfig()
        {
            ConfigTag* conf = ServerInstance->Config->ConfValue("largetextpaste");
            serviceUrl = conf->getString("service", "");
            cmdLineCommand = conf->getString("cmd", "pastebinit");
            snipLen = conf->getInt("sniplen", 60);
            cutoffLen = conf->getInt("cutofflen", 300);

            // log exception if executable is not present, and set the availibility flag to skip execution later on
            if (!_commandAvailable(cmdLineCommand))
            {
                commandAvailable = false;
                ServerInstance->Logs->Log("MODULE", DEFAULT, "Unable to find executable command: %s, for m_largetextpaste",
                    cmdLineCommand.c_str());
            }

            else {
                commandAvailable = true;
            }
        }

        bool _commandAvailable(std::string cmd)
        {
            // the which command seems to be the best option for *nix
            // we could use the stat(2) call, but the user may not provide a full path to the binary
            std::string testCommand = "which " + cmd;

            // storage for the output file's contents
            std::string result;

            // output file in read mode
            FILE* testCmdFile = popen(testCommand.c_str(), "r");

            if (testCmdFile)
            {
                char resultBuffer[100];
                char* line = fgets(resultBuffer, sizeof(resultBuffer), testCmdFile);
                if (line)
                {
                    result = std::string(line);
                }
            }

            pclose(testCmdFile);

            if (!result.empty())
            {
                // a simple comparison to determine if the binary's name was in the result of the which command
                return result.find(cmd) != std::string::npos;
            }
            else
            {
                return false;
            }
        }

        std::string _prepareExecutableMessage(std::string nick, std::string text)
        {
            // this method could be built upon further to support more of the pastebin service's options
            // other options
            // -a author
            // -b service url
            // -f format (for syntax highlighting)
            // -m permatag
            // -t paste title
            // -u username (if login required)
            // -p password (if login required)


            // we don't really want the text in a temp file so a piped echo is good enough
            std::string executable = std::string("echo \"" + text + "\" | ");

            // pastebin command
            executable += cmdLineCommand;

            // set the author
            executable += " -a " + nick;

            // if the default service is not desired, then set the alternative
            // could check that alternate is supported, but it can wait
            if (serviceUrl.length() > 0)
            {
                executable += " -b " + serviceUrl;
            }

            return executable;
        }

    public:

        void init()
        {
            _readConfig();

            Implementation eventlist[] = { I_OnUserPreMessage, I_OnRehash };
            ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
        }



        ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list)
        {
            if (commandAvailable && (target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
            {
                //if the message is over the limit shorten it
                if (text.length() > cutoffLen)
                {
                    // first store a copy of the text
                    std::string orig = std::string(text);

                    // trim it
                    text.resize(snipLen);
                    text += "...";

                    // construct executable string and try to capture output into a file
                    std::string executableMessage = _prepareExecutableMessage(user->nick, orig);
                    FILE* cmdFile = popen(executableMessage.c_str(), "r");

                    // if we got some output check its validity and append it to the end of the text
                    if (cmdFile)
                    {
                        char resultBuffer[100];
                        char* line = fgets(resultBuffer, sizeof(resultBuffer), cmdFile);
                        std::string url = std::string(line);

                        // compile final output text
                        text += " (more " + url.erase(url.find_last_not_of("\n\r") + 1) + " )";
                    }

                    // clean up open file
                    pclose(cmdFile);
                }
            }

            return MOD_RES_PASSTHRU;
        }

        void OnRehash(User* user) {
            _readConfig();
        }

        Version GetVersion()
        {
            return Version("Module sends messages longer than set number of characters to a pastebin like\
                service and modifies the message with a link", VF_VENDOR);
        }
};

MODULE_INIT(ModuleLargeTextPaste)
