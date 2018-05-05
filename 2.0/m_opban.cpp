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
/* $ModDesc: Implements extban +b o: Prevents these persons from aquiring a privileged position */
/* $ModDepends: core 2.0 */
/* $ModConfig: Optional: <opban requiredrank="50000"> sets required rank to set/unset +b o, defaults to q */

#include "inspircd.h"
class ModuleOPBan : public Module
{
 private:
	unsigned int RequiredRank;
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnRawMode, I_On005Numeric, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}
	Version GetVersion()
	{
		return Version("Implements extban +b o: Prevents these persons from aquiring a privileged position",VF_OPTCOMMON);
	}

	void OnRehash(User* user)
	{
		this->RequiredRank = 50000;
		ConfigTag* conftag = ServerInstance->Config->ConfValue("opban");
		if (conftag)
			this->RequiredRank = conftag->getInt("requiredrank", 50000);
	}

	ModResult OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt)
	{
		if (!chan || !IS_LOCAL(user) || IS_OPER(user) || ServerInstance->ULine(user->server))
			return MOD_RES_PASSTHRU;

		Membership* transmitter = chan->GetUser(user);
		if ((mode == 'b') && (param.length() > 2) && (param[0] == 'o') && (param[1] == ':') && (transmitter->getRank() < this->RequiredRank))
		{
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s : You do not have sufficient privileges to set or unset extban o", user->nick.c_str(), chan->name.c_str());
			return MOD_RES_DENY;
		}
		
		if (!adding || param.empty())
			return MOD_RES_PASSTHRU;
			
		User *u = ServerInstance->FindNick(param);
		ModeHandler *mh = ServerInstance->Modes->FindMode(mode, MODETYPE_CHANNEL);
		if (!u || !mh)
			return MOD_RES_PASSTHRU;
				
		if ((chan->GetExtBanStatus(u, 'o') == MOD_RES_DENY) && !IS_OPER(u) && (mh->GetPrefixRank() > 0))
		{
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s : %s is banned from having a privileged position", user->nick.c_str(), chan->name.c_str(), u->nick.c_str());
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('o');
	}
};
MODULE_INIT(ModuleOPBan)

