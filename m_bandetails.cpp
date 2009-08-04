#include "inspircd.h"

/* $ModDesc: Synchronises setter and time set of channel bans, so details are not lost. */
/* $ModAuthor: Aleksey */
/* $ModAuthorMail: w00t@inspircd.org */
/* $ModDepends: core 1.2 */
 
class ModuleBanDetails : public Module
{
	public:

		virtual void OnSyncChannel (Channel *chan, Module *proto, void *opaque)
		{
			for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
			{
				std::string meta(i->data);
				meta.append(" ").append(i->set_by).append(" ").append(ConvToStr(i->set_time));
				proto->ProtoSendMetaData(opaque, TYPE_CHANNEL, chan, "m_bandetails", meta);
			}
		}

		virtual void OnDecodeMetaData (int target_type, void *target, const std::string &extname, const std::string &extdata)
		{
			if ((target_type == TYPE_CHANNEL) && (extname == "m_bandetails"))
			{
				Channel* chan = (Channel*)target;
				std::string banmask,sb,st;

				irc::spacesepstream list(extdata);
				list.GetToken(banmask);
				list.GetToken(sb);
				list.GetToken(st);

				for (BanList::iterator i = chan->bans.begin(); i != chan->bans.end(); i++)
				{
					if (!strcasecmp(i->data.c_str(), banmask.c_str()))
					{
						i->set_time=ConvToInt(st);
						i->set_by=sb;
						break;
					}
				}
			}
		}

		ModuleBanDetails(InspIRCd* Me) : Module(Me)
		{
			Implementation eventlist[] = { I_OnSyncChannel,I_OnDecodeMetaData};
			ServerInstance->Modules->Attach(eventlist, this, 2);
		}

		virtual Version GetVersion()
		{
			return Version("$Id: m_bandetails.cpp 0 2008-11-07 18:02:38SAMT phoenix $",VF_COMMON,API_VERSION);
		}

};

MODULE_INIT(ModuleBanDetails)
