/*
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* $ModAuthor: Sebastian Nielsen */
/* $ModAuthorMail: sebastian@sebbe.eu */
/* $ModDesc: Implements extban +b o: Prevents these persons from aquiring a privilegied position */
/* $ModDepends: core 2.0 */

#include "inspircd.h"
class ModuleOPBan : public Module {
public:
  void init()
  {
    Implementation eventlist[] = { I_OnRawMode, I_On005Numeric };
    ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
  }
  
  Version GetVersion()
  {
    return Version("Implements extban +b o: Prevents these persons from aquiring a privilegied position",VF_OPTCOMMON);
  }

  ModResult OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt)
  {
    if (chan && IS_LOCAL(user) && !IS_OPER(user) && !ServerInstance->ULine(user->server)) {
      Membership* transmitter = chan->GetUser(user);
      if ((mode == 'b') && (param.length() > 2) && (param[0] == 'o') && (param[1] == ':') && (transmitter->modes.find('q') == std::string::npos))
      {
				user->WriteNumeric(482, "%s %s : You must have channel owner access or oper to be able to set or unset extban o", user->nick.c_str(), chan->name.c_str());
				return MOD_RES_DENY;
			}
			if (adding && !param.empty())
			{
				User *u = ServerInstance->FindNick(param);
				if (u)
				{
					if ((chan->GetExtBanStatus(u, 'o') == MOD_RES_DENY) && !IS_OPER(u) && (mode == 'a' || mode == 'o' || mode == 'h' || mode == 'v'))
					{
						user->WriteNumeric(482, "%s %s : %s is banned from having a privilegied position", user->nick.c_str(), chan->name.c_str(), u->nick.c_str());
						return MOD_RES_DENY;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}
	
	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('o');
	}
};
MODULE_INIT(ModuleOPBan)
