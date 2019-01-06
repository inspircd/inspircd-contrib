/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Peter Powell <petpow@saberuk.com>
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

/* $CompileFlags: pkgconfincludes("libqrencode","/qrencode.h","") */
/* $LinkerFlags: pkgconflibs("libqrencode","/libqrencode.so","-lqrencode") */

/* $ModAuthor: Peter "SaberUK" Powell */
/* $ModAuthorMail: petpow@saberuk.com */
/* $ModDesc: Provides support for QR code generation via the /QRCODE command. */
/* $ModConfig: <qrcode blockchar=" " darkcolour="black" lightcolour="white"> */
/* $ModDepends: core 2.0 */


#include "inspircd.h"
#include "ssl.h"

#include <qrencode.h>

class QRCode
{
 private:
	QRcode* code;
	int error;

 public:
	QRCode(const std::string& url)
	{
		errno = error = 0;
		code = QRcode_encodeString(url.c_str(), 0, QR_ECLEVEL_M, QR_MODE_8, 0);
		if (!code)
			error = errno;
	}

	int GetError() const
	{
		return error;
	}

	bool GetPixel(size_t x, size_t y) const
	{
		if (!code)
			return false;

		if (x > code->width || y > code->width)
			return false;

		unsigned char* chr = code->data + (x * code->width) + y;
		return *chr & 0x1;
	}

	size_t GetSize() const
	{
		return code ? code->width : 0;
	}

	~QRCode()
	{
		QRcode_free(code);
	}
};

class CommandQRCode : public SplitCommand
{
private:
	std::string FormatPixel(bool dark)
	{
		std::string buffer;
		buffer.append("\x3");
		buffer.append(dark ? darkcolour : lightcolour);
		buffer.append(",");
		buffer.append(dark ? darkcolour : lightcolour);
		buffer.append(blockchar);
		buffer.append(blockchar);
		return buffer;
	}

	std::string URLEncode(const std::string& data)
	{
		static const char* hextable = "0123456789ABCDEF";

		std::string buffer;
		for (std::string::const_iterator iter = data.begin(); iter != data.end(); ++iter)
		{
			if (isalnum(*iter))
			{
				buffer.push_back(*iter);
				continue;
			}

			buffer.push_back('%');
			buffer.push_back(hextable[*iter >> 4]);
			buffer.push_back(hextable[*iter & 15]);
		}
		return buffer;
	}

	void WriteMessage(LocalUser* user, const std::string& message)
	{
		user->WriteServ("PRIVMSG %s :%s", user->nick.c_str(), message.c_str());
	}

 public:
	std::string blockchar;
	std::string darkcolour;
	std::string lightcolour;

	CommandQRCode(Module* Creator)
		: SplitCommand(Creator, "QRCODE", 0, 1)
	{
		allow_empty_last_param = false;
	}

	CmdResult HandleLocal(const std::vector<std::string>& parameters, LocalUser* source)
	{
		std::string url;
		if (!parameters.empty())
		{
			if (ServerInstance->IsChannel(parameters[0].c_str(), ServerInstance->Config->Limits.ChanMax))
			{
				Channel* channel = ServerInstance->FindChan(parameters[0]);
				if (!channel || ((channel->IsModeSet('p') || channel->IsModeSet('s')) && !channel->HasUser(source)))
				{
					source->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :No such channel", source->nick.c_str(), parameters[0].c_str());
					return CMD_FAILURE;
				}

				url = URLEncode(channel->name);
				if (channel->IsModeSet('k'))
					url.append(",needkey");
			}
			else
			{
				User* user = ServerInstance->FindNickOnly(parameters[0]);
				if (!user)
				{
					source->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick", source->nick.c_str(), parameters[0].c_str());
					return CMD_FAILURE;
				}
				url = URLEncode(user->nick) + ",isnick";
			}
		}

		if (!source->GetClass()->config->getString("password").empty())
			url.append(",needpass");
		
		url.insert(0, "/");
		url.insert(0, source->server_sa.str());

		SocketCertificateRequest request(&source->eh, ServerInstance->Modules->Find("m_qrcode.so"));
		url.insert(0, request.cert ? "ircs://" : "irc://");

		QRCode code(url);
		if (code.GetError())
		{
			source->WriteServ("NOTICE %s :QR generation failed: %s", source->nick.c_str(), strerror(code.GetError()));
			return CMD_FAILURE;
		}

		// Give a friendly message to tell the user what to do with this code.
		if (parameters.empty())
			WriteMessage(source, "Use this QR code to connect to " + ServerInstance->Config->Network + ":");
		else
			WriteMessage(source, "Use this QR code to connect to " + ServerInstance->Config->Network + " and chat with " + parameters[0] + ":");


		// Format the QR code and send it to the user.
		std::string border;
		for (size_t c = 0; c < code.GetSize() + 2; ++c)
			border.append(FormatPixel(false)); 

		WriteMessage(source, border);
		for (size_t y = 0; y < code.GetSize(); ++y)
		{
			std::string row(FormatPixel(false));
			for (size_t x = 0; x < code.GetSize(); ++x)
				row.append(FormatPixel(code.GetPixel(x, y)));
			row.append(FormatPixel(false));

			WriteMessage(source, row);
		}
		WriteMessage(source, border);

		return CMD_SUCCESS;
	}
};

class ModuleQRCode : public Module
{
 private:
	CommandQRCode cmd;

	std::string GetColourCode(ConfigTag* tag, const char* key, const char* def)
	{
		std::string name = tag->getString(key, def);
		std::transform(name.begin(), name.end(), name.begin(), ::tolower);

		if (name == "white")
			return "0";

		if (name == "black")
			return "1";

		if (name == "blue")
			return "2";

		if (name == "green")
			return "3";

		if (name == "red")
			return "4";

		if (name == "brown")
			return "5";

		if (name == "purple")
			return "6";

		if (name == "orange")
			return "7";

		if (name == "yellow")
			return "8";
		
		if (name == "lightgreen")
			return "9";

		if (name == "cyan")
			return "10";

		if (name == "lightcyan")
			return "11";

		if (name == "lightblue")
			return "12";

		if (name == "pink")
			return "13";

		if (name == "gray" || name == "grey")
			return "14";

		if (name == "lightgray" || name == "lightgrey")
			return "15";

		if (name.find_first_not_of("0123456789") == std::string::npos)
		{
			const long value = ConvToInt(name);
			if (value >= 0 && value <= 99)
				return ConvToStr(value);
		}

		throw ModuleException("<" + tag->tag + ":" + key + "> is not a valid colour!");
	}

 public:
	ModuleQRCode()
		: cmd(this)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->Modules->Attach(I_OnRehash, this);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("qrcode");

		// You can use █ if your client sucks at fixed-width formatting.
		std::string blockchar = tag->getString("blockchar", " ");
		if (blockchar.empty())
			throw ModuleException("<qrcode:blockchar> must not be empty!");

		// The colours to use for the dark and light blocks.
		std::string darkcolour = GetColourCode(tag, "darkcolour", "black");
		std::string lightcolour = GetColourCode(tag, "lightcolour", "white");

		// Store the values in the command handler.
		cmd.blockchar.swap(blockchar);
		cmd.darkcolour.swap(darkcolour);
		cmd.lightcolour.swap(lightcolour);
	}

	Version GetVersion()
	{
		return Version("Provides support for QR code generation via the /QRCODE command");
	}
};

MODULE_INIT(ModuleQRCode)
