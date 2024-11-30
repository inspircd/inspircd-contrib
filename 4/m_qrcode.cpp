/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
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

/// $CompilerFlags: find_compiler_flags("libqrencode")
/// $LinkerFlags: find_linker_flags("libqrencode")

/// $PackageInfo: require_system("arch") qrencode pkgconf
/// $PackageInfo: require_system("darwin") qrencode pkg-config
/// $PackageInfo: require_system("debian~") libqrencode-dev pkg-config
/// $PackageInfo: require_system("rhel~") qrencode-devel pkgconfig

/// $ModAuthor: Sadie Powell <sadie@witchery.services>
/// $ModDesc: Provides support for QR code generation via the /QRCODE command.
/// $ModConfig: <qrcode blockchar=" " darkcolour="black" lightcolour="white">
/// $ModDepends: core 4


#include <qrencode.h>

#include "inspircd.h"
#include "clientprotocolmsg.h"
#include "modules/ssl.h"
#include "numerichelper.h"
#include "stringutils.h"
#include "utility/string.h"

class QRCode final
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

		size_t width = static_cast<size_t>(code->width);
		if (x > width || y > width)
			return false;

		unsigned char* chr = code->data + (x * width) + y;
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

class CommandQRCode final
	: public SplitCommand
{
private:
	ChanModeReference keymode;
	ChanModeReference privatemode;
	ChanModeReference secretmode;

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

	void WriteMessage(LocalUser* user, const std::string& message)
	{
		ClientProtocol::Messages::Privmsg privmsg(ClientProtocol::Messages::Privmsg::nocopy, ServerInstance->FakeClient, user, message);
		user->Send(ServerInstance->GetRFCEvents().privmsg, privmsg);
	}

public:
	std::string blockchar;
	std::string darkcolour;
	std::string lightcolour;

	CommandQRCode(Module* Creator)
		: SplitCommand(Creator, "QRCODE", 0, 1)
		, keymode(Creator, "key")
		, privatemode(Creator, "private")
		, secretmode(Creator, "secret")
	{
	}

	CmdResult HandleLocal(LocalUser* source, const Params& parameters) override
	{
		std::string url;
		if (!parameters.empty())
		{
			if (ServerInstance->Channels.IsChannel(parameters[0]))
			{
				Channel* channel = ServerInstance->Channels.Find(parameters[0]);
				if (!channel || ((channel->IsModeSet(privatemode) || channel->IsModeSet(secretmode)) && !channel->HasUser(source)))
				{
					source->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
					return CmdResult::FAILURE;
				}

				url = Percent::Encode(channel->name);
				if (channel->IsModeSet(keymode))
					url.append(",needkey");
			}
			else
			{
				User* user = ServerInstance->Users.FindNick(parameters[0]);
				if (!user)
				{
					source->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
					return CmdResult::FAILURE;
				}
				url = Percent::Encode(user->nick) + ",isnick";
			}
		}

		if (!source->GetClass()->config->getString("password").empty())
			url.append(",needpass");

		url.insert(0, "/");
		url.insert(0, ConvToStr(source->server_sa.port()));
		url.insert(0, ":");

		const SSLIOHook* const ssliohook = SSLIOHook::IsSSL(&source->eh);
		std::string serverhost;
		if (ssliohook && ssliohook->GetServerName(serverhost))
			url.insert(0, serverhost);
		else
			url.insert(0, source->server_sa.addr());
		url.insert(0, ssliohook ? "ircs://" : "irc://");

		QRCode code(url);
		if (code.GetError())
		{
			source->WriteNotice(INSP_FORMAT("QR generation failed: {}", strerror(code.GetError())));
			return CmdResult::FAILURE;
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

		return CmdResult::SUCCESS;
	}
};

class ModuleQRCode final
	: public Module
{
private:
	CommandQRCode cmd;

	std::string GetColourCode(const std::shared_ptr<ConfigTag>& tag, const char* key, const char* def)
	{
		std::string name = tag->getString(key, def);
		std::transform(name.begin(), name.end(), name.begin(), ::tolower);

		if (insp::equalsci(name, "white"))
			return "0";

		if (insp::equalsci(name, "black"))
			return "1";

		if (insp::equalsci(name, "blue"))
			return "2";

		if (insp::equalsci(name, "green"))
			return "3";

		if (insp::equalsci(name, "red"))
			return "4";

		if (insp::equalsci(name, "brown"))
			return "5";

		if (insp::equalsci(name, "purple"))
			return "6";

		if (insp::equalsci(name, "orange"))
			return "7";

		if (insp::equalsci(name, "yellow"))
			return "8";

		if (insp::equalsci(name, "lightgreen"))
			return "9";

		if (insp::equalsci(name, "cyan"))
			return "10";

		if (insp::equalsci(name, "lightcyan"))
			return "11";

		if (insp::equalsci(name, "lightblue"))
			return "12";

		if (insp::equalsci(name, "pink"))
			return "13";

		if (name == "gray" || name == "grey")
			return "14";

		if (name == "lightgray" || name == "lightgrey")
			return "15";

		if (name.find_first_not_of("0123456789") == std::string::npos)
		{
			const auto value = ConvToNum<unsigned long>(name);
			if (value <= 99)
				return ConvToStr(value);
		}

		throw ModuleException(this, "<" + tag->name + ":" + key + "> is not a valid colour!");
	}

public:
	ModuleQRCode()
		: Module(VF_NONE, "Provides support for QR code generation via the /QRCODE command.")
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus&) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("qrcode");

		// You can use █ if your client sucks at fixed-width formatting.
		std::string blockchar = tag->getString("blockchar", " ");
		if (blockchar.empty())
			throw ModuleException(this, "<qrcode:blockchar> must not be empty!");

		// The colours to use for the dark and light blocks.
		std::string darkcolour = GetColourCode(tag, "darkcolour", "black");
		std::string lightcolour = GetColourCode(tag, "lightcolour", "white");

		// Store the values in the command handler.
		cmd.blockchar.swap(blockchar);
		cmd.darkcolour.swap(darkcolour);
		cmd.lightcolour.swap(lightcolour);
	}
};

MODULE_INIT(ModuleQRCode)
