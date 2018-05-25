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
/* $ModDesc: Allows locking down channel modes to specific prefix ranks */
/* $ModDepends: core 2.0 */
/* $ModConfig: <moderestrict mode="b" rank="30000"> = halfops cannot ban/unban, <moderestrict mode="t" rank="50000"> = MLOCK functionality */

#include "inspircd.h"
class ModuleModeRestrict : public Module
{
 private:
	std::map<char, unsigned int> requiredrank;
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnRawMode, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	Version GetVersion()
	{
		return Version("Allows locking down channel modes to specific prefix ranks");
	}

	void OnRehash(User* user)
	{
		requiredrank.clear();
		ConfigTagList tags = ServerInstance->Config->ConfTags("moderestrict");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string mode = tag->getString("mode");
			unsigned int rank = tag->getInt("rank");
			if (mode.length() == 1)
				requiredrank[mode[0]] = rank;
		}
	}

	ModResult OnRawMode(User* user, Channel* chan, const char mode, const std::string &param, bool adding, int pcnt)
	{
		if (!chan || !IS_LOCAL(user) || IS_OPER(user) || ServerInstance->ULine(user->server))
			return MOD_RES_PASSTHRU;

		Membership* transmitter = chan->GetUser(user);
		if (!transmitter)
			return MOD_RES_PASSTHRU;

		std::map<char, unsigned int>::iterator mc;
		mc = requiredrank.find(mode);
		if (mc == requiredrank.end())
			return MOD_RES_PASSTHRU;

		if (transmitter->getRank() < mc->second)
		{
			std::string modestring = ConvToStr(mode);
			user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s : You do not have sufficient privileges to set or unset mode %s", user->nick.c_str(), chan->name.c_str(), modestring.c_str());
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};
MODULE_INIT(ModuleModeRestrict)
