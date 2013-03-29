/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Peter Powell <petpow@saberuk.com>
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

#include "inspircd.h"

/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModDesc: Allows the customisation of penalty levels. */
/* $ModDepends: core 2.0-2.1 */
/* $ModConfig: <penalty name="INVITE" value="60"> */

class ModuleCustomPenalty : public Module
{
	public:
		
		void init()
		{
			ServerInstance->Modules->Attach(I_OnLoadModule, this);
			SetPenalties();
		}
		
		void OnLoadModule(Module* mod)
		{
			SetPenalties();
		}
		
		void SetPenalties()
		{
			ConfigTagList tags = ServerInstance->Config->ConfTags("penalty");
			for (ConfigIter i = tags.first; i != tags.second; ++i)
			{
				ConfigTag* tag = i->second;
				
				std::string name = tag->getString("name");
				int penalty = (int)tag->getInt("value", 1);
				
				Command* command = ServerInstance->Parser->GetHandler(name);
				if (command == NULL)
				{
					ServerInstance->Logs->Log("m_custompenalty", DEFAULT, "Warning: unable to find command: " + name);
					continue;	
				}
				if (penalty < 0)
				{
					ServerInstance->Logs->Log("m_custompenalty", DEFAULT, "Warning: unable to set a negative penalty for " + name);
					continue;	
				}
				
				ServerInstance->Logs->Log("m_custompenalty", DEBUG, "Setting the penalty for %s to %d", name.c_str(), penalty);
				command->Penalty = penalty;
			}
		}
		
		Version GetVersion()
		{
			return Version("Allows the customisation of penalty levels.");
		}
};

MODULE_INIT(ModuleCustomPenalty)
