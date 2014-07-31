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

/* $ModConfig: <largetextpaste sniplen="60" cutofflen="300"> */
/* $ModDesc: Allows server to catch large PRIVMSG text and send them off to a pastebin.com service and replace the text with a shortened version including said url to text entirety. */
/* $ModAuthor: macawm */
/* $ModAuthorMail: macawm@gmail.com */
/* $ModDepends: core 2.0 */

/* $LinkerFlags: -lcurl */

#ifdef WINDOWS
#pragma comment(lib, "libcurl_static.lib")
#endif

#include "inspircd.h"

#include <curl/curl.h>

class ModuleLargeTextPaste : public Module
{
    private:
        // pastebin API developer key
        std::string devKey;

        // A URL to point to a different service if the default is not acceptable
        std::string serviceUrl;

        // The amount of the original text to post in the modified PRIVMSG (unit: char)
        // Defaults to 60 chars
        unsigned int snipLen;

        // The length of text that triggers the pastebin shortening (unit: char)
        // Defaults to 300 chars
        unsigned int cutoffLen;


        size_t static write_data(char *ptr, size_t size, size_t nmemb, void *userdata)
        {
            // we are not accounting for large chunked writes, so there is no buffer for the data

            // simple cast
            char** response_ptr = (char**) userdata;

            // copy the string, it better return a string
            *response_ptr = new char[size * nmemb];

            // probably need a sanity check here too. could be null if allocation fails
            if (*response_ptr)
            {
                strncpy(*response_ptr, ptr, size * nmemb);
            }

            // return the number of bytes we read
            return size * nmemb;
        }

    public:

        void init()
        {
            serviceUrl = "http://pastebin.com/api/api_post.php";

            OnRehash(NULL);

            Implementation eventlist[] = { I_OnUserPreMessage, I_OnRehash };
            ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
        }

        ModuleLargeTextPaste() : Module()
        {
            curl_global_init(CURL_GLOBAL_ALL);
        }

        ~ModuleLargeTextPaste()
        {
            // must cleanup
            curl_global_cleanup();
        }

        ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list)
        {
            //if the message is over the limit shorten it
            if (target_type == TYPE_CHANNEL && IS_LOCAL(user) && text.length() > cutoffLen)
            {
                // required cURL setup
                CURL* curl = curl_easy_init();
                if (curl)
                {

                    // set post fields for pastebin API
                    std::string pasteName  = user->nick + " wrote";
                    std::string postFields = "api_option=paste";
                    postFields += "&api_dev_key=" + devKey;
                    postFields += "&api_paste_code=" + std::string(curl_easy_escape(curl, text.c_str(), text.size()));
                    postFields += "&api_paste_name=" + std::string(curl_easy_escape(curl, pasteName.c_str(), pasteName.size()));
                    postFields += "&api_paste_private=1"; // unlisted
                    postFields += "&api_paste_expire_date=N"; // never expires

                    // set url and post fields
                    curl_easy_setopt(curl, CURLOPT_URL, serviceUrl.c_str());
                    curl_easy_setopt(curl, CURLOPT_POST, 1);
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());

                    // provide a place to store POST response
                    char* response = NULL;

                    // set callback for POST write back
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

                    // set data reference for write back
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

                    // send the request
                    CURLcode result = curl_easy_perform(curl);
                    if (result == CURLE_OK)
                    {
                        // if the pastebin request worked, then trim the text and add URL
                        text.resize(snipLen);
                        text.append("... (more ").append(response).append(" )");
                        if (response) // just a sanity check
                        {
                            delete response;
                        }

                    }

                    else
                    {
                        // otherwise just leave the text alone and post an error
                        ServerInstance->Logs->Log("m_largetextpaste", DEFAULT, "cURL operation failed: %s", curl_easy_strerror(result));
                    }

                    // must cleanup
                    curl_easy_cleanup(curl);
                }
            }

            return MOD_RES_PASSTHRU;
        }

        void OnRehash(User* user) {
            ConfigTag* conf = ServerInstance->Config->ConfValue("largetextpaste");
            snipLen = conf->getInt("sniplen", 60);
            cutoffLen = conf->getInt("cutofflen", 300);
            devKey = conf->getString("apikey");
        }

        Version GetVersion()
        {
            return Version("Module sends messages longer than set number of characters to a pastebin.com \
                service and modifies the message with a link", VF_NONE);
        }
};

MODULE_INIT(ModuleLargeTextPaste)
