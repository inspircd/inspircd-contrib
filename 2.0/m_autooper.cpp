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
/* $ModDesc: Adds a opertype option to connect blocks so operators can get autologged in via IP */
/* $ModDepends: core 2.0 */
/* $ModConfig: Within connect block: opertype="OPER TYPE HERE" */


#include "inspircd.h"

class ModuleAutoOper : public Module
{
public:
	void init()
	{
		ServerInstance->Modules->Attach(I_OnUserConnect, this);
	}

	void OnUserConnect(LocalUser* user)
	{
		ConfigTag* tag = user->MyClass->config;
		std::string opertype = tag->getString("opertype");

		if (opertype.empty())
			return;

		OperIndex::iterator opit = ServerInstance->Config->oper_blocks.find(" " + opertype);
		if (opit == ServerInstance->Config->oper_blocks.end())
		{
			ServerInstance->Logs->Log("m_autooper", DEFAULT, "m_autooper: Oper type %s in connect block %s not found", opertype.c_str(), user->MyClass->name.c_str());
			return;
		}
		user->Oper(opit->second);
	}

	Version GetVersion()
	{
		return Version("Adds a opertype option to connect blocks so operators can get autologged in via IP");
	}
};

MODULE_INIT(ModuleAutoOper)
