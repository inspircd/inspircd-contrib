/* This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

/* $ModAuthor: Sebastian Nielsen */
/* $ModAuthorMail: sebastian@sebbe.eu */
/* $ModDesc: Provides snomasks p and P to which notices about PMs are sent */
/* $ModDepends: core 2.0 */
/* $ModConfig: Optional: <userpm ignoreuline="no" ignoreoper="no" expandnicks="0/1/2">, 0 = nick only, 1 = full + cloak, 2 = full */

class ModuleUserPM : public Module
{
 private:
 	bool IgnoreUline;
	bool IgnoreOper;
	int ExpandNicks;
 public:
	void init()
	{
		ServerInstance->SNO->EnableSnomask('p', "USERPM");
		Implementation eventlist[] = { I_OnUserMessage, I_OnUserNotice, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	Version GetVersion()
	{
		return Version("Provides snomasks p and P to which notices about PMs are sent");
	}

	void OnRehash(User *user)
	{
		ConfigTag* conftag = ServerInstance->Config->ConfValue("userpm");
		this->IgnoreUline = conftag->getBool("ignoreuline", false);
		this->IgnoreOper = conftag->getBool("ignoreoper", false);
		this->ExpandNicks = conftag->getInt("expandnicks", 0);
	}

	void OnUserMessage(User *user, void *dest, int targettype, const std::string &pm, char status, const CUList &except)
	{
		if ( (targettype != TYPE_USER) || !IS_LOCAL(user) )
		{
			return;
		}
		User *duser = (User *)dest;
		if (this->IgnoreUline && ( ServerInstance->ULine(user->server) || ServerInstance->ULine(duser->server) ))
		{
			return;
		}
		if (this->IgnoreOper && ( IS_OPER(user) || IS_OPER(duser) ))
		{
			return;
		}
		if (this->ExpandNicks == 0)
		{
			ServerInstance->SNO->WriteGlobalSno('p', "<%s>,<%s>: %s", user->nick.c_str(), duser->nick.c_str(), pm.c_str());
		}
		else
		{
			if (this->ExpandNicks == 1)
			{
				ServerInstance->SNO->WriteGlobalSno('p', "<%s!%s@%s>,<%s!%s@%s>: %s", user->nick.c_str(), user->ident.c_str(), user->dhost.c$
			}
			else
			{
				ServerInstance->SNO->WriteGlobalSno('p', "<%s!%s@%s>,<%s!%s@%s>: %s", user->nick.c_str(), user->ident.c_str(), user->host.c_$
			}
		}
	}

	void OnUserNotice(User *user, void *dest, int targettype, const std::string &pm, char status, const CUList &except)
	{
		OnUserMessage(user,dest,targettype,pm,status,except);
	}
};

MODULE_INIT(ModuleUserPM)
