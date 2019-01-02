/*
 * Based entirely off of the fixes for connect class inheritance in inspircd 3.0
 * as of https://github.com/inspircd/inspircd/tree/3d0d64933da3d37866fadfa042e34a1125315db6
 */

/* $ModAuthor: linuxdaemon */
/* $ModAuthorMail: linuxdaemonirc@gmail.com */
/* $ModDesc: Backport of the fixes for <connect> class inheritance from 3.0 */
/* $ModDepends: core 2.0 */

#include "inspircd.h"

namespace
{
	typedef reference<ConnectClass> ClassReference;
	typedef std::map<std::string, ClassReference> ClassMap;
	typedef std::map<std::string, std::string> ConfigMap;
	typedef std::vector<KeyVal> ConfigItems;
}

class ModuleFixInheritance : public Module
{
	ConfigTag* CreateTag(ConfigTag* tag, const ConfigMap& conf_map)
	{
		ConfigItems* items = NULL;
		ConfigTag* newTag = ConfigTag::create(tag->tag, tag->src_name, tag->src_line, items);
		items->assign(conf_map.begin(), conf_map.end());
		return newTag;
	}

	void InheritClass(const ClassReference& cls, const ClassReference& parent)
	{
		ConfigMap conf_map;
		const ConfigItems& parentkeys = parent->config->getItems();
		for (ConfigItems::const_iterator piter = parentkeys.begin(); piter != parentkeys.end(); ++piter)
		{
			if (piter->first == "name" || piter->first == "parent")
				continue;

			conf_map[piter->first] = piter->second;
		}

		const ConfigItems& childkeys = cls->config->getItems();
		for (ConfigItems::const_iterator childiter = childkeys.begin(); childiter != childkeys.end(); ++childiter)
			conf_map[childiter->first] = childiter->second;

		cls->config = CreateTag(cls->config, conf_map);
	}

 public:
	ModuleFixInheritance()
	{
	}

	void init()
	{
		ServerInstance->Modules->Attach(I_OnRehash, this);
	}

	void OnRehash(User*)
	{
		const ClassVector& ServerClasses = ServerInstance->Config->Classes;

		ClassMap name_map;
		ClassVector::size_type blockcount = ServerClasses.size();
		bool try_again = true;

		for (ClassVector::size_type tries = 0; try_again; ++tries)
		{
			try_again = false;
			for (ClassVector::const_iterator it = ServerClasses.begin(), it_end = ServerClasses.end(); it != it_end; ++it)
			{
				ClassReference cls = *it;
				std::string clsname = cls->GetName();
				if (name_map.find(clsname) != name_map.end())
					// Class already handled
					continue;

				std::string parentName = cls->config->getString("parent");
				if (!parentName.empty())
				{
					// If class is in name_map it either didn't have a parent or was successfully inherited
					ClassMap::const_iterator parentIter = name_map.find(parentName);
					if (parentIter != name_map.end())
					{
						InheritClass(cls, parentIter->second);
					}
					else
					{
						try_again = true;
						// couldn't find parent this time. If it's the last time, we'll never find it.
						if (tries >= blockcount)
						{
							throw ModuleException("Could not find parent connect class \"" + parentName + "\" for connect block at " + cls->config->getTagLocation());
						}

						continue;
					}
				}
				name_map[clsname] = cls;
			}
		}
	}

	Version GetVersion()
	{
		return Version("Fixes connect classes not inheriting their parent modules configs like modes=\"\", etc.");
	}
};

MODULE_INIT(ModuleFixInheritance)
