/**
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
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

// requires: nlohmann-json3-dev

/// $ModAuthor: synmuffin (irc.xfnet.org)
/// $ModAuthorMail: jnewing [at] gmail [dot] com
/// $ModConfig: <httpapi token="32-character_long_API_Token">
/// $ModDepends: core 3, m_httpd
/// $ModDesc: Provides a HTTP(s) API that allows users to query to reteive some information on the IRCd and Network.


// clean and simple JSON hpp libaray for C++ (MIT License)
// https://github.com/nlohmann/json
#include <nlohmann/json.hpp>

#include "inspircd.h"
#include "modules/httpd.h"
#include "xline.h"

/**
 * Structs
 * 
 * Layout all our structs, this makes things a little cleaner and we can use a basic template
 * to go from Strcut -> JSON Object and JSON Object -> Struct
 */
namespace _Stats
{ 
    // user data
    struct _UsersInfo {
        unsigned long network;
        unsigned long local;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_UsersInfo,
            network, local);
    };

    struct _ChannelsInfo {
        unsigned long count;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_ChannelsInfo,
            count);
    };

    struct _OpersInfo {
        unsigned long total;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_OpersInfo,
            total);
    };

    struct _ServerListEntry {
        std::string server_name;
        std::string parent_name;
        std::string server_description;
        unsigned long user_count;
        unsigned long latency;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_ServerListEntry,
            server_name, parent_name, server_description, user_count, latency);
    };

    struct _ServerInfo {
        std::string server_name;
        std::string server_description;
        std::string server_version;
        std::string server_network;
        time_t server_uptime;
        time_t server_currenttime;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_ServerInfo,
            server_name, server_description, server_version, server_network, server_uptime, server_currenttime);
    };

    struct _XLine {
        std::string type;
        std::string mask;
        time_t settime;
        unsigned long duration;
        std::string duration_string;
        std::string reason;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_XLine,
            type, mask, settime, duration, duration_string, reason);
    };

    struct _GeneralInfo {
        _ServerInfo server;
        _UsersInfo users;
        _ChannelsInfo channels;
        _OpersInfo opers;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_GeneralInfo,
            server, users, channels, opers);
    };

    struct _Chan {
        unsigned long user_count;
        std::string channel_name;
        std::string channel_topic;
        std::string set_by;
        time_t set_time;
        std::string channel_modes;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_Chan,
            user_count, channel_name, channel_topic, set_by, set_time, channel_modes);
    };

    struct _ErrCode {
        int code;
        std::string description;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_ErrCode,
            code, description);
    };

    struct _BadResponse {
        bool error;
        _ErrCode errcode;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_BadResponse,
            error, errcode);
    };

    struct _Cmd {
        std::string name;
        unsigned long use_count;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_Cmd,
            name, use_count);
    };

    struct _Module {
        std::string name;
        std::string description;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_Module,
            name, description);
    };

    struct _UserAway {
        std::string awaymsg;
        time_t awaytime;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_UserAway,
            awaymsg, awaytime);
    };

    struct _UserLocal {
        int port;
        std::string serveraddr;
        std::string connectclass;
        time_t lastmsg;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_UserLocal,
            port, serveraddr, connectclass, lastmsg);
    };

    struct _User {
        std::string nickname;
        std::string uuid;
        std::string realhost;
        std::string displayhost;
        std::string realname;
        std::string server;
        time_t signon;
        time_t age;
        std::string ipaddress;
        _Stats::_UserAway away;
        std::string oper;
        std::string modes;
        std::string ident;
        _Stats::_UserLocal local;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(_User,
            nickname, uuid, realhost, displayhost, realname, server, signon, age, ipaddress, away, oper, modes, ident, local);
    };

};

/**
 * JSON_API Class
 * 
 * Define our JSON_API Class
 */
class JSON_API
{
    public:
        JSON_API();
        nlohmann::json callAPIFunction(const std::string& apiFunction, HTTPRequest& request);
        void setToken(std::string token);
        bool validateToken(HTTPHeaders *headers);
    
    private:
        std::string _token;
        std::map<std::string, std::function<nlohmann::json(HTTPRequest&)>> apiFunctionMap;
        static nlohmann::json api_general(HTTPRequest& request);
        static nlohmann::json api_server(HTTPRequest& request);
        static nlohmann::json api_users(HTTPRequest& request);
        static nlohmann::json api_xlines(HTTPRequest& request);
        static nlohmann::json api_server_list(HTTPRequest& request);
        static nlohmann::json api_commands(HTTPRequest& request);
        static nlohmann::json api_channels(HTTPRequest& request);
        static nlohmann::json api_modules(HTTPRequest& request);
        static nlohmann::json api_motd(HTTPRequest& request);
};


/**
 * Construct a new JSON_API object
 */
JSON_API::JSON_API()
{
    // api function routing
    // honestly I'm not sure if this is the best way to do this I'm just to shit to know any better... *shrug*
    apiFunctionMap["/api/general"]      = [](HTTPRequest& request) { return api_general(request); };
    apiFunctionMap["/api/server"]       = [](HTTPRequest& request) { return api_server(request); };
    apiFunctionMap["/api/users"]        = [](HTTPRequest& request) { return api_users(request); };
    apiFunctionMap["/api/xlines"]       = [](HTTPRequest& request) { return api_xlines(request); };
    apiFunctionMap["/api/server-list"]  = [](HTTPRequest& request) { return api_server_list(request); };
    apiFunctionMap["/api/commands"]     = [](HTTPRequest& request) { return api_commands(request); };
    apiFunctionMap["/api/channels"]     = [](HTTPRequest& request) { return api_channels(request); };
    apiFunctionMap["/api/modules"]      = [](HTTPRequest& request) { return api_modules(request); };
    apiFunctionMap["/api/motd"]         = [](HTTPRequest& request) { return api_motd(request); };
}

/**
 * callAPIFunction():
 * 
 * The callAPIFunctino does just what it says. Calls the corresponding
 * function matching the /api/ path. Must return an nlohmann::json
 * object.
 * 
 * @param apiFunction 
 * @param request 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::callAPIFunction(const std::string& apiFunction, HTTPRequest& request)
{
    if (apiFunctionMap.find(apiFunction) != apiFunctionMap.end())
    {
        return apiFunctionMap[apiFunction](request);
    }

    return _Stats::_BadResponse {
            true,
            { 404, "Not Found"}
        };
}

/**
 * setToken():
 * 
 * Set the token string
 * 
 * @param token 
 */
void JSON_API::setToken(std::string token)
{
    _token = token;
}

/**
 * validateToken():
 * 
 * Validates the token set in the HTTP Request Header.
 * 
 * @param headers 
 * @return true 
 * @return false 
 */
bool JSON_API::validateToken(HTTPHeaders *headers)
{
    if (headers->IsSet("Token") && headers->GetHeader("Token") == _token)
    {
        return true;
    }

    return false;
}
    

/**
 * api_general():
 * 
 * Returns a JSON object with some basic general server,
 * network, user and oper information.
 * 
 * HTTP Method: GET
 * URI: http(s)://<host>:<port>/api/general
 * Headers: 
 *      Content-Type: application/json
 *      Token: <token-set-in-config> (**Required**)
 * 
 * @param params 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::api_general(HTTPRequest& request)
{
    if (request.GetType() != "GET")
    {
        return _Stats::_BadResponse {
            true,
            { 405, "Method Not Allowed"}
        };
    }

    return _Stats::_GeneralInfo {
        {
            ServerInstance->Config->ServerName,
            ServerInstance->Config->ServerDesc,
            ServerInstance->GetVersionString(true),
            ServerInstance->Config->Network,
            ServerInstance->startup_time,
            ServerInstance->Time(),
        },
        { 
            ServerInstance->Users->GetUsers().size(),
            ServerInstance->Users->GetLocalUsers().size()
        },
        { ServerInstance->GetChans().size() },
        { ServerInstance->Users->all_opers.size() },
    };
}

/**
 * api_server():
 * 
 * Returns a JSON object containing general information on the
 * server, network, and its uptime
 * 
 * HTTP Method: GET
 * URI: http(s)://<host>:<port>/api/server
 * Headers: 
 *      Content-Type: application/json
 *      Token: <token-set-in-config> (**Required**)
 * 
 * @param params 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::api_server(HTTPRequest& request)
{
    if (request.GetType() != "GET")
    {
        return _Stats::_BadResponse {
            true,
            { 405, "Method Not Allowed"}
        };
    }

    return _Stats::_ServerInfo {
        ServerInstance->Config->ServerName,
        ServerInstance->Config->ServerDesc,
        ServerInstance->GetVersionString(true),
        ServerInstance->Config->Network,
        ServerInstance->startup_time,
        ServerInstance->Time(),
    };
}
        
/**
 * api_users():
 * 
 * Returns the current user information. (Filterable)
 * 
 * HTTP Method: GET | POST
 * URI: http(s)://<host>:<port>/api/users
 * 
 * Note: You can use multiple filters and and if you do they 
 *       will be treated as a logical 'AND'.
 * 
 * Headers: 
 *      Content-Type: application/json
 *      Token: <token-set-in-config> (**Required**)
 * Body: (JSON Object)
 *      {
 *          filters: {          
 *              ident: "some_ident",
 *              nickname: "syn*",
 *              server: "irc.xfnet.org",
 *              oper: "SomeOperClass"
 *          }
 *      }
 * 
 * @param params 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::api_users(HTTPRequest& request)
{
    std::vector<_Stats::_User> user_list;
    nlohmann::json params;

    if (request.GetType() != "POST" && request.GetType() != "GET")
    {
        return _Stats::_BadResponse {
            true,
            { 405, "Method Not Allowed"}
        };
    }

    // if we have POST request we need filter data
    if (request.GetType() == "POST")
    {
        std::string post_data = request.GetPostData();

        if (post_data.empty())
        {
            return _Stats::_BadResponse {
                true,
                { 400, "Bad Request" }
            };
        }

        // parse our params
        params = nlohmann::json::parse(post_data);
    }

    const user_hash& users = ServerInstance->Users->GetUsers();
    for (const auto& [_, user] : users)
    {
        // User* u = i->second;

        if (user->registered != REG_ALL)
            continue;

        _Stats::_UserAway user_away;
        _Stats::_UserLocal user_local;

        if (user->IsAway())
        {
            user_away = _Stats::_UserAway {
                user->awaymsg,
                user->awaytime,
            };
        }

        LocalUser* lu = IS_LOCAL(user);
		if (lu)
        {
            user_local = _Stats::_UserLocal {
                lu->server_sa.port(),
                lu->server_sa.str(),
                lu->GetClass()->GetName(),
                lu->idle_lastmsg,
            };
        }

        user_list.push_back(_Stats::_User {
            user->nick,
            user->uuid,
            user->GetRealHost(),
            user->GetDisplayedHost(),
            user->GetRealName(),
            user->server->GetName(),
            user->signon,
            user->age,
            user->GetIPString(),
            user_away,
            (user->IsOper()) ? user->oper->name : "",
            user->GetModeLetters().substr(1),
            user->ident,
            user_local
        });
    }

    // filter(s)
    if (params.contains("filter"))
    {
        // filter: test
        for (auto& kv : params["filter"].items())
        {
            if (params["filter"][kv.key()].is_string())
            {
                auto remove_if_not_filter = [&kv](const _Stats::_User& _user)
                {
                    if (kv.key() == "ident")    return !InspIRCd::Match(_user.ident, kv.value());
                    if (kv.key() == "nickname") return !InspIRCd::Match(_user.nickname, kv.value());
                    if (kv.key() == "server")   return !InspIRCd::Match(_user.server, kv.value());
                    if (kv.key() == "oper")     return !InspIRCd::Match(_user.oper, kv.value());
                    
                    return false;
                };

                user_list.erase(std::remove_if(user_list.begin(), user_list.end(), remove_if_not_filter), user_list.end());
            }
            else
            {
                return _Stats::_BadResponse {
                    true,
                    { 400, "Bad Request" }
                };
            }
        }
    }

    return user_list;
}

/**
 * api_xlines():
 * 
 * Returns a listing of xlines, with or withour filtering via params
 * 
 * Note: You can use multipul filters and and if you do they 
 *       will be treated as a logical 'AND'.
 * 
 * HTTP Method: GET | POST
 * URI: http(s)://<host>:<port>/api/xlines
 * Headers: 
 *      Content-Type: application/json
 *      Token: <token-set-in-config> (**Required**)
 * Body: (JSON Object - Optional)
 *      {
 *          filters: {          
 *              type: ["Z", "Q"],
 *              mask: "something*"
 *          }
 *      }
 * 
 * @param params 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::api_xlines(HTTPRequest& request)
{
    std::vector<_Stats::_XLine> xlines;
    nlohmann::json params;

    if (request.GetType() != "POST" && request.GetType() != "GET")
    {
        return _Stats::_BadResponse {
            true,
            { 405, "Method Not Allowed"}
        };
    }

    // if we are using POST we expect filters.
    if (request.GetType() == "POST")
    {
        std::string post_data = request.GetPostData();

        if (post_data.empty())
        {
            return _Stats::_BadResponse {
                true,
                { 400, "Bad Request" }
            };
        }

        // parse our params
        params = nlohmann::json::parse(post_data);
    }

    std::vector<std::string> types = ServerInstance->XLines->GetAllTypes();
    for (const auto& type : types)
    {
        XLineLookup* lookup = ServerInstance->XLines->GetAll(type);
        if (lookup)
        {
            for (const auto& xlinePair : *lookup)
            {
                XLine* line = xlinePair.second;
                xlines.push_back({ type.c_str(), line->Displayable(), line->set_time, line->duration, InspIRCd::DurationString(line->duration), line->reason });
            }
        }
    }

    // filter(s)
    if (params.contains("filter"))
    {
        // filter: type
        // we expect and arry but if the user gives us just 1 entry we'll make the array
        if (params["filter"].contains("type"))
        {
            std::vector<std::string> typeFilter;

            if (params["filter"]["type"].is_array())
            {
                typeFilter = params["filter"]["type"];
            }
            else
            {
                typeFilter = { params["filter"]["type"] };
            }

            // filter
            auto isType = [typeFilter](const _Stats::_XLine& line)
            { 
                return std::find(typeFilter.begin(), typeFilter.end(), line.type) == typeFilter.end();
            };

            xlines.erase(std::remove_if(xlines.begin(), xlines.end(), isType), xlines.end());
        }

        // filter: mask
        // we excpet a user string as a mask, we also assume glob matching... not sure if thats ok or not. :S
        if (params["filter"].contains("mask") && params["filter"]["mask"].is_string())
        {
            std::string maskFilter = params["filter"]["mask"];
            auto isMask = [maskFilter](const _Stats::_XLine& line)
            {
                return !InspIRCd::Match(line.mask, maskFilter);
            };

            xlines.erase(std::remove_if(xlines.begin(), xlines.end(), isMask), xlines.end());
        }
    }

    return xlines;
}

/**
 * api_server_list():
 * 
 * Returns a list of all servers connected to this network.
 * 
 * HTTP Method: GET
 * URI: http(s)://<host>:<port>/api/server-list
 * Headers: 
 *      Content-Type: application/json
 *      Token: <token-set-in-config> (**Required**)
 * 
 * @param request 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::api_server_list(HTTPRequest& request)
{
    std::vector<_Stats::_ServerListEntry> server_list; 

    if (request.GetType() != "GET")
    {
        return _Stats::_BadResponse {
            true,
            { 405, "Method Not Allowed"}
        };
    }

    ProtocolInterface::ServerList sl;
    ServerInstance->PI->GetServerList(sl);

    for (ProtocolInterface::ServerList::const_iterator b = sl.begin(); b != sl.end(); ++b)
    {
        server_list.push_back(_Stats::_ServerListEntry {
            b->servername,
            b->parentname,
            b->description,
            b->usercount,
            b->latencyms,
        });
    }

    return server_list;
}

/**
 * api_commands():
 * 
 * Returns a list of commands.
 * 
 * HTTP Method: GET
 * URI: http(s)://<host>:<port>/api/commands
 * Headers: 
 *      Content-Type: application/json
 *      Token: <token-set-in-config> (**Required**)
 * 
 * @param request 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::api_commands(HTTPRequest& request)
{
    std::vector<_Stats::_Cmd> commands_list; 

    if (request.GetType() != "GET")
    {
        return _Stats::_BadResponse {
            true,
            { 405, "Method Not Allowed"}
        };
    }

    const CommandParser::CommandMap& commands = ServerInstance->Parser.GetCommands();
    for (CommandParser::CommandMap::const_iterator i = commands.begin(); i != commands.end(); ++i)
    {
        commands_list.push_back(_Stats::_Cmd {
            i->second->name,
            i->second->use_count,
        });
    }

    return commands_list;
}

/**
 * api_channels():
 * 
 * Returns a list to channels on the server. You can also filter this list by name
 * you can match using glob patterns.
 * 
 * HTTP Method: GET | POST
 * URI: http(s)://<host>:<port>/api/channels
 * Headers: 
 *      Content-Type: application/json
 *      Token: <token-set-in-config> (**Required**)
 * Body: (JSON Object)
 *      {
 *          filters: {          
 *              name: "#name",
 *          }
 *      }
 * 
 * @param request 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::api_channels(HTTPRequest& request)
{
    std::vector<_Stats::_Chan> channel_list;
    nlohmann::json params;

    if (request.GetType() != "POST" && request.GetType() != "GET")
    {
        return _Stats::_BadResponse {
            true,
            { 405, "Method Not Allowed"}
        };
    }

    // if we are using POST we expect filters.
    if (request.GetType() == "POST")
    {
        std::string post_data = request.GetPostData();

        if (post_data.empty())
        {
            return _Stats::_BadResponse {
                true,
                { 400, "Bad Request" }
            };
        }

        // parse our params
        params = nlohmann::json::parse(post_data);
    }

    const chan_hash& chans = ServerInstance->GetChans();
    for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i)
    {
        Channel* c = i->second;
        channel_list.push_back(_Stats::_Chan {
            c->GetUsers().size(),
            c->name,
            c->topic,
            c->setby,
            c->topicset,
            c->ChanModes(true),
        });
    }

    // filter(s)
    if (params.contains("filter"))
    {
        // filter: name
        // we excpet a user string as a name or glob pattern, we also assume glob matching... not sure if thats ok or not. :S
        if (params["filter"].contains("name") && params["filter"]["name"].is_string())
        {
            std::string nameFilter = params["filter"]["name"];
            auto isName = [nameFilter](const _Stats::_Chan& ichan)
            {
                return !InspIRCd::Match(ichan.channel_name, nameFilter);
            };

            channel_list.erase(std::remove_if(channel_list.begin(), channel_list.end(), isName), channel_list.end());
        }
    }

    return channel_list;
}

/**
 * api_modules():
 * 
 * Returns a list of loaded modules on this server.
 * 
 * HTTP Method: GET
 * URI: http(s)://<host>:<port>/api/modules
 * Headers: 
 *      Content-Type: application/json
 *      Token: <token-set-in-config> (**Required**)
 * 
 * @param request 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::api_modules(HTTPRequest& request)
{
    if (request.GetType() != "GET")
    {
        return _Stats::_BadResponse {
            true,
            { 405, "Method Not Allowed"}
        };
    }

    std::vector<_Stats::_Module> module_list;
    const ModuleManager::ModuleMap& mods = ServerInstance->Modules->GetModules();

    for (ModuleManager::ModuleMap::const_iterator i = mods.begin(); i != mods.end(); ++i)
    {
        Version v = i->second->GetVersion();

        module_list.push_back(_Stats::_Module {
            i->first,
            v.description,
        });
    }

    return module_list;
}

/**
 * api_motd():
 * 
 * Returns the motd for the server.
 * 
 * HTTP Method: GET
 * URI: http(s)://<host>:<port>/api/motd
 * Headers: 
 *      Content-Type: application/json
 *      Token: <token-set-in-config> (**Required**)
 * 
 * @param request 
 * @return nlohmann::json 
 */
nlohmann::json JSON_API::api_motd(HTTPRequest& request)
{
    std::vector<std::string> motd_msg;

    if (request.GetType() != "GET")
    {
        return _Stats::_BadResponse {
            true,
            { 405, "Method Not Allowed"}
        };
    }

    ConfigTag* conf = ServerInstance->Config->ConfValue("files");

    try
    {
        FileReader reader(conf->getString("motd", "motd", 1));

        const file_cache& lines = reader.GetVector();

        motd_msg.reserve(lines.size());
        for (file_cache::const_iterator it = lines.begin(); it != lines.end(); ++it)
        {
            const std::string& line = *it;
            motd_msg.push_back(line.empty() ? " " : line);
        }
    }
    catch (CoreException&)
    {
        // nada...
    }

    return motd_msg;
}

/**
 * Module
 */
class ModuleHttpAPI : public Module, public HTTPRequestEventListener
{
	HTTPdAPI API;
    JSON_API jsonapi;

    public:
	    ModuleHttpAPI()
		    : HTTPRequestEventListener(this)
		    , API(this)
            , jsonapi()
	    {
	    }

	    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	    {
            ConfigTag* conf = ServerInstance->Config->ConfValue("httpapi");

            std::string token = conf->getString("token", "");

            // inform the user they need to set a token
            if (token.empty() && token.length() < 32)
            {
                throw ModuleException("Your token empty or too short. It should be at least 32 characters in length.");
            }

            // set the api token
            jsonapi.setToken(token);
        }

        ModResult HandleRequest(HTTPRequest* http)
        {
            // are they calling the api
            if (http->GetPath().compare(0, 4, "/api"))
            {
                return MOD_RES_PASSTHRU;
            }

            // validate the token
            if (!jsonapi.validateToken(http->headers))
            {
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "HTTP API request for: %s denied. Invalid credentials.", http->GetPath().c_str());
                return MOD_RES_DENY;
            }

            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Handling API HTTP request for %s", http->GetPath().c_str());

            // pass off the request
            nlohmann::json resp_data = jsonapi.callAPIFunction(http->GetPath(), *http);

            if (resp_data.contains("error") && resp_data["error"])
            {
                return ErrorResult(http, resp_data);
            }
            
            // build json data        
            std::stringstream data;
            data << resp_data;

            // m_httpd resoponse
            HTTPDocumentResponse response(this, *http, &data, 200);
            response.headers.SetHeader("X-Powered-By", MODNAME);
            response.headers.SetHeader("Content-Type", "application/json");
            API->SendResponse(response);

            return MOD_RES_DENY;
        }

        ModResult ErrorResult(HTTPRequest* http, const nlohmann::json response_data)
        {
            // build json data        
            std::stringstream data;
            data << response_data["errcode"];

            // m_httpd resoponse
            HTTPDocumentResponse response(this, *http, &data, response_data["errcode"]["code"]);
            response.headers.SetHeader("X-Powered-By", MODNAME);
            response.headers.SetHeader("Content-Type", "application/json");
            API->SendResponse(response);

            return MOD_RES_DENY;
        }

        ModResult OnHTTPRequest(HTTPRequest& req) CXX11_OVERRIDE
        {
            return HandleRequest(&req);
        }

        Version GetVersion() CXX11_OVERRIDE
        {
            return Version("Provides JSON-serialised API for fetching data about the server, channels, network and users over HTTP(s).");
        }
};

MODULE_INIT(ModuleHttpAPI);
