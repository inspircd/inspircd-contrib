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
/* $ModDesc: Prevents a lower rank from removing a listmode item set by a higher rank */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

class Banprotector
{
 public:
	std::map<char, std::map<std::string, unsigned int>> banrank;
	Banprotector() {}

	void addrank(const char& modeparam, const std::string& banparam, const unsigned int& rank)
	{
		std::map<char, std::map<std::string, unsigned int>>::iterator obindex = banrank.find(modeparam);
		if (obindex != banrank.end()) {
			std::map<std::string, unsigned int> innerban = obindex->second;
			std::map<std::string, unsigned int>::iterator ibindex = innerban.find(banparam);
			if (ibindex != innerban.end())
				return;
		}
		banrank[modeparam][banparam] = rank;
	}

	bool checkrank(const char& modeparam, const std::string& banparam, const unsigned int& rank)
	{
		std::map<char, std::map<std::string, unsigned int>>::iterator obindex = banrank.find(modeparam);
		if (obindex != banrank.end()) {
			std::map<std::string, unsigned int> innerban = obindex->second;
			std::map<std::string, unsigned int>::iterator ibindex = innerban.find(banparam);
			if (ibindex == innerban.end())
				return true;
			if (ibindex->second > rank)
				return false;
		}
		return true;
	}

	void delrank(const char& modeparam, const std::string& banparam)
	{
		std::map<char, std::map<std::string, unsigned int>>::iterator obindex = banrank.find(modeparam);;
		if (obindex != banrank.end()) {
			std::map<std::string, unsigned int> innerban = obindex->second;
			std::map<std::string, unsigned int>::iterator removeindex = innerban.find(banparam);
			if (removeindex != innerban.end())
			{
				innerban.erase(removeindex);
				banrank[modeparam] = innerban;
			}
		}
	}
};

class ModuleBanprotect : public Module
{
 private:
	SimpleExtItem<Banprotector> ext;
 public:
	ModuleBanprotect()
		: ext("Banprotector", this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(ext);
		ServerInstance->Modules->Attach(I_OnRawMode, this);
	}

	Version GetVersion()
	{
		return Version("Prevents a lower rank from removing a listmode item set by a higher rank");
	}

	ModResult OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt)
	{
		if (!chan)
			return MOD_RES_PASSTHRU;

		ModeHandler *mh = ServerInstance->Modes->FindMode(mode, MODETYPE_CHANNEL);
		if (!mh)
			return MOD_RES_PASSTHRU;

		if (!mh->IsListMode() || mh->GetPrefixRank() > 0)
			return MOD_RES_PASSTHRU;

		Banprotector* banp = ext.get(chan);
		if (!banp)
		{
			ext.set(chan, new Banprotector());
			banp = ext.get(chan);
		}
		
		Membership* transmitter = chan->GetUser(user);
		if (!transmitter)
		{
			if (!adding)
				banp->delrank(mode, param);
			return MOD_RES_PASSTHRU;
		}

		if (adding)
		{
			banp->addrank(mode, param, transmitter->getRank());
			return MOD_RES_PASSTHRU;
		}
		
		if (!banp->checkrank(mode, param, transmitter->getRank()) && IS_LOCAL(user) && !IS_OPER(user) && !ServerInstance->ULine(user->server))
		{
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You need a privilege equal or higher than the person who set the entry, to remove it.", user->nick.c_str(), chan->name.c_str());
			return MOD_RES_DENY;
		}
		banp->delrank(mode, param);
		return MOD_RES_PASSTHRU;
	}
};
MODULE_INIT(ModuleBanprotect)
