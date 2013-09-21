#include	<iostream>
#include	<sstream>
#include	<string>
#include "inspircd.h"
#include "u_listmode.h"

///	\todo	function calls should be replaced with the new versions
#define	LDAP_DEPRECATED	1

#include	<ldap.h>

#define		LDAPSERVERPORT	LDAP_PORT

/* $ModDesc: Provides support for the +U channel mode */
/* $ModDep: ../../include/u_listmode.h */
/* $LinkerFlags: -lldap */

/*
 * Written by glezmen <glezmen@gmail.com>, Jan 2012.
 *
 * Based on m_inviteexception which was originally based on m_exception,
 * which was originally based on m_chanprotect and m_silence
 *
 * The +U channel mode takes an LDAP group name. If a user matches an LDAP
 * group entry on the +U list then they can join the channel, ignoring if +i
 * is set on the channel.
 *
 *	In modules.conf you will need these nodes:
 *		<module name="m_inviteldapexception.so">
 *		<inviteldapexception
 *			server="ldap.example.com"
 *			base="ou=Group,dc=example,dc=com"
 *			attributes="=member,=uniqueMember,memberUid">
 *
 *	The 'attributes' tag lists attributes to check to decide if the user
 *	is member of the group or not. If it begins with '=', the modules
 *	will look for attrib:...=username, otherwise attrib:username
 */

/** Handles channel mode +U
 */
class InviteLdapException : public ListModeBase
{
 public:
	InviteLdapException(Module* Creator) : ListModeBase(Creator, "ldapex", 'U', "End of Channel LDAP Group Invite Exception List", 350, 351, false) { }
};

class ModuleInviteLdapException : public Module
{
	InviteLdapException ie;
	std::string         ldapserver;
public:
	ModuleInviteLdapException() : ie(this)
	{
		if (!ServerInstance->Modes->AddMode(&ie))
			throw ModuleException("Could not add new modes!");

		ie.DoImplements(this);
		Implementation eventlist[] = { I_On005Numeric, I_OnCheckInvite, I_OnCheckKey };
		ServerInstance->Modules->Attach(eventlist, this, 3);
	}


	///	\return	TRUE if the parameter looks like an LDAP group
	bool isGroup(const char* groupname)
	{
		const char *groupstr = "ou=Group";
		const char *pos = strstr(groupname, groupstr);
		if (!pos)
			return false;
		int after = *(pos + strlen(groupstr));
		if (after == 0 || after == ',')
			return pos == groupstr || *(pos-1) == ',';
		return false;
	}

	bool isMember(const char* username, const char* groupname)
	{
		LDAP*          ld;
		LDAPMessage*   search_result;
		LDAPMessage*   current_entry;
		char*          dn;
		int            version;
		int            rc;
		bool           result = false;

		if (ldapserver.empty())
			ldapserver = ServerInstance->Config->ConfValue("inviteldapexception")->getString("server");

		/* Initialize the LDAP library and open a connection to an LDAP server */
		if ( (ld = ldap_init(ldapserver.c_str(), LDAPSERVERPORT)) == NULL )
		{
			std::stringstream s;
			s << "ERROR: Can't initialize connection to LDAP server " << ldapserver << ":" << LDAPSERVERPORT;
			ServerInstance->Logs->Log("m_inviteldapexception", DEFAULT, s.str());
			return false;
		}

		/* For TPF, set the client to an LDAPv3 client. */
		version = LDAP_VERSION3;
		ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &version );

		/* Bind to the server. */
		rc = ldap_simple_bind_s( ld, NULL, NULL );
		if ( rc != LDAP_SUCCESS )
		{
			std::stringstream s;
			s << "ERROR: Can't bind to LDAP server " << ldapserver << ":" << LDAPSERVERPORT;
			ServerInstance->Logs->Log("m_inviteldapexception", DEFAULT, s.str());
			ldap_unbind( ld );
			return false;
		}

		/* Perform the LDAP search */
		std::string groupdn = ServerInstance->Config->ConfValue("inviteldapexception")->getString("base");
		if (*groupname)
		{
			groupdn.insert(0, std::string(groupname) + ",");
			if (!strchr(groupname, '='))
				groupdn.insert(0,"cn=");
		}

		rc = ldap_search_ext_s( ld, groupdn.c_str(), LDAP_SCOPE_SUBTREE, "(|(objectClass=groupOfNames)(objectClass=posixGroup))", 0, 0, NULL, NULL, NULL, 
				0, &search_result );

		/* Try to use groupname as full qualified */
		if ( rc != LDAP_SUCCESS )
		{
			rc = ldap_search_ext_s( ld, groupname, LDAP_SCOPE_SUBTREE, "(|(objectClass=groupOfNames)(objectClass=posixGroup))", 0, 0, NULL, NULL, NULL, 
					0, &search_result );
			if ( rc != LDAP_SUCCESS )
			{
				std::cerr << "ERROR: LDAP search failed" << std::endl;
				ldap_unbind( ld );
				return false;
			}
		}

		std::string attribs = ServerInstance->Config->ConfValue("inviteldapexception")->getString("attributes");
		int ulen = strlen(username);
		for (current_entry = ldap_first_entry(ld, search_result); current_entry != NULL; current_entry = 
				ldap_next_entry(ld, current_entry))
		{
			BerElement* ber;
			char* found;
			for (char *attr = ldap_first_attribute(ld, current_entry, &ber); !result && attr; attr = ldap_next_attribute(ld, current_entry, ber))
			{
				//if (!strcmp(attr, "member") || !strcmp(attr,"uniqueMember") || !strcmp(attr,"memberUid"))
				while (!result && !attribs.empty())
				{
					size_t comma = attribs.find(',');
					std::string s; // current attribute
					if (comma == attribs.npos)
					{	// no more comma
						s = attribs;
						attribs = "";
					}
					else
					{
						s = attribs.substr(0, comma);
						attribs = attribs.substr(comma+1);
					}
					if ((s[0] == '=' && s.substr(1) == attr) || (s[0] != '=' && s == attr))
					{
						char ** vals;
						vals = ldap_get_values(ld, current_entry, attr);
						int size = ldap_count_values(vals);
						for (int i=0; !result && i<size; ++i)
						{
							if (isGroup(vals[i]))
							{	// this is a group
								if (isMember(username,vals[i]))
									result = true;
							}
							else // this is a user
								if (!*username || ((found = strstr(vals[i], username)) != NULL))
								{	///	\todo	nice check for username here too, so it doesn't match "usernameblabla"
									if (*username && s[0] != '=' && s == attr)
										result = true;
									else
										if (*username && s[0] == '=' && *(found-1) == '=' && (*(found+ulen) == 0 || *(found+ulen) == ','))
											result = true;
								}
						}
						if (vals)
							ldap_value_free(vals);
					}

				}
				ldap_memfree(attr);
			}
			ber_free(ber, 0);
		}

		ldap_msgfree(search_result);

		/* Disconnect from the server. */
		ldap_unbind( ld );

		return result;
	}

	void On005Numeric(std::string &output)
	{
		output.append(" LDAPEX=U");
	}

	ModResult OnCheckInvite(User* user, Channel* chan)
	{
		if(chan != NULL)
		{
			modelist* list = ie.extItem.get(chan);
			if (list)
			{
				for (modelist::iterator it = list->begin(); it != list->end(); it++)
				{
					ServerInstance->Logs->Log("CONNECT", NONE, "checking LDAP membership: for user: %s, group: %s", user->nick.c_str(), it->mask.c_str());
					if (isMember(user->nick.c_str(), it->mask.c_str()))
					{
						return MOD_RES_ALLOW;
					}
				}
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckKey(User* user, Channel* chan, const std::string& key)
	{
		if (ServerInstance->Config->ConfValue("inviteldapexception")->getBool("bypasskey", true))
			return OnCheckInvite(user, chan);
		return MOD_RES_PASSTHRU;
	}

	void OnCleanup(int target_type, void* item)
	{
		ie.DoCleanup(target_type, item);
	}

	void OnSyncChannel(Channel* chan, Module* proto, void* opaque)
	{
		ie.DoSyncChannel(chan, proto, opaque);
	}

	void OnRehash(User* user)
	{
		ie.DoRehash();

		ldapserver = ServerInstance->Config->ConfValue("inviteldapexception")->getString("server");
	}

	Version GetVersion()
	{
		return Version("Provides support for the +U channel mode", VF_VENDOR);
	}
};

MODULE_INIT(ModuleInviteLdapException)
