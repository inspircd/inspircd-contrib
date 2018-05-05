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
/* $ModDesc: Adds a badnicks option to connect block, allowing you to specify prohibited nicks */
/* $ModDepends: core 2.0 */
/* $ModConfig: Within connect block: badnicks="badnick *serv* *rat bot*" */


#include "inspircd.h"

class ModuleBadnicks : public Module
{
public:
	void init()
	{
		ServerInstance->Modules->Attach(I_OnUserPreNick, this);
	}

	ModResult OnUserPreNick(User* olduser, const std::string& newnick)
	{
		LocalUser* user = IS_LOCAL(olduser);
		if (!user)
			return MOD_RES_PASSTHRU;

		ConfigTag* tag = user->MyClass->config;
		std::string badnicks = tag->getString("badnicks");
		if (badnicks.empty())
			return MOD_RES_PASSTHRU;

		irc::spacesepstream StreamReader(badnicks);
		std::string badnick;
		while (StreamReader.GetToken(badnick))
		{
			if (InspIRCd::Match(newnick, badnick))
			{
				user->WriteNumeric(432, "%s %s :This nick is prohibited for your connect class", (this->registered & REG_NICK ? this->nick.c_str() : "*"), newnick.c_str());
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
	
	Version GetVersion()
	{
		return Version("Adds a badnicks option to connect block, allowing you to specify prohibited nicks");
	}
};

MODULE_INIT(ModuleBadnicks)
