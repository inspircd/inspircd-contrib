/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2016-2017, 2019-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2016-2017 Attila Molnar <attilamolnar@hush.com>
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


/// $ModAuthor: InspIRCd Developers
/// $ModConfig: <sslprofile name="Clients" provider="mbedtls" cafile="" certfile="cert.pem" crlfile="" dhfile="dhparams.pem" hash="sha256" keyfile="key.pem" mindhbits="2048" outrecsize="2048" requestclientcert="yes">
/// $ModDepends: core 4
/// $ModDesc: Allows TLS encrypted connections using the mbedTLS library.
/// $ModLink: https://docs.inspircd.org/4/moved-modules/#ssl_mbedtls

/// $LinkerFlags: -lmbedtls

/// $PackageInfo: require_system("arch") mbedtls
/// $PackageInfo: require_system("darwin") mbedtls
/// $PackageInfo: require_system("debian~") libmbedtls-dev


#include "inspircd.h"
#include "modules/ssl.h"
#include "stringutils.h"
#include "timeutils.h"
#include "utility/string.h"

#ifdef _WIN32
# define timegm _mkgmtime
#endif

// Work around mbedTLS using C99 features that are not part of C++.
#ifdef __clang__
# pragma clang diagnostic ignored "-Wc99-extensions"
#endif

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/dhm.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/md.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <mbedtls/version.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_crl.h>

#ifdef INSPIRCD_MBEDTLS_LIBRARY_DEBUG
#include <mbedtls/debug.h>
#endif

static Module* thismod;

namespace mbedTLS
{
	class Exception final
		: public ModuleException
	{
	public:
		Exception(const std::string& msg)
			: ModuleException(thismod, msg)
		{
		}
	};

	std::string ErrorToString(int errcode)
	{
		char buf[256];
		mbedtls_strerror(errcode, buf, sizeof(buf));
		return buf;
	}

	void ThrowOnError(int errcode, const char* msg)
	{
		if (errcode != 0)
		{
			std::string reason = msg;
			reason.append(" :").append(ErrorToString(errcode));
			throw Exception(reason);
		}
	}

	template <typename T, void (*init)(T*), void (*deinit)(T*)>
	class RAIIObj
	{
		T obj;

	public:
		RAIIObj()
		{
			init(&obj);
		}

		~RAIIObj()
		{
			deinit(&obj);
		}

		T* get() { return &obj; }
		const T* get() const { return &obj; }
	};

	typedef RAIIObj<mbedtls_entropy_context, mbedtls_entropy_init, mbedtls_entropy_free> Entropy;

	class CTRDRBG final
		: private RAIIObj<mbedtls_ctr_drbg_context, mbedtls_ctr_drbg_init, mbedtls_ctr_drbg_free>
	{
	public:
		bool Seed(Entropy& entropy)
		{
			return (mbedtls_ctr_drbg_seed(get(), mbedtls_entropy_func, entropy.get(), nullptr, 0) == 0);
		}

		void SetupConf(mbedtls_ssl_config* conf)
		{
			mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, get());
		}
	};

	class DHParams final
		: public RAIIObj<mbedtls_dhm_context, mbedtls_dhm_init, mbedtls_dhm_free>
	{
	public:
		void set(const std::string& dhstr)
		{
			// Last parameter is buffer size, must include the terminating null
			int ret = mbedtls_dhm_parse_dhm(get(), reinterpret_cast<const unsigned char*>(dhstr.c_str()), dhstr.size()+1);
			ThrowOnError(ret, "Unable to import DH params");
		}
	};

	class X509Key final
		: public RAIIObj<mbedtls_pk_context, mbedtls_pk_init, mbedtls_pk_free>
	{
	public:
		/** Import */
		X509Key(const std::string& keystr)
		{
#if MBEDTLS_VERSION_MAJOR >= 3
			int ret = mbedtls_pk_parse_key(get(), reinterpret_cast<const unsigned char*>(keystr.c_str()),
				keystr.size() + 1, nullptr, 0, mbedtls_ctr_drbg_random, 0);
#else
			int ret = mbedtls_pk_parse_key(get(), reinterpret_cast<const unsigned char*>(keystr.c_str()),
				keystr.size() + 1, nullptr, 0);
#endif
			ThrowOnError(ret, "Unable to import private key");
		}
	};

	class Ciphersuites final
	{
		std::vector<int> list;

	public:
		Ciphersuites(const std::string& str)
		{
			// mbedTLS uses the ciphersuite format "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256" internally.
			// This is a bit verbose, so we make life a bit simpler for admins by not requiring them to supply the static parts.
			irc::sepstream ss(str, ':');
			for (std::string token; ss.GetToken(token); )
			{
				// Prepend "TLS-" if not there
				if (token.compare(0, 4, "TLS-", 4))
					token.insert(0, "TLS-");

				const int id = mbedtls_ssl_get_ciphersuite_id(token.c_str());
				if (!id)
					throw Exception("Unknown ciphersuite " + token);
				list.push_back(id);
			}
			list.push_back(0);
		}

		const int* get() const { return &list.front(); }
		bool empty() const { return (list.size() <= 1); }
	};

	class Curves final
	{
#if MBEDTLS_VERSION_MAJOR >= 3
		std::vector<uint16_t> list;
#else
		std::vector<mbedtls_ecp_group_id> list;
#endif

	public:
		Curves(const std::string& str)
		{
			irc::sepstream ss(str, ':');
			for (std::string token; ss.GetToken(token); )
			{
				const mbedtls_ecp_curve_info* curve = mbedtls_ecp_curve_info_from_name(token.c_str());
				if (!curve)
					throw Exception("Unknown curve " + token);
#if MBEDTLS_VERSION_MAJOR >= 3
				list.push_back(curve->tls_id);
#else
				list.push_back(curve->grp_id);
#endif
			}
			list.push_back(MBEDTLS_ECP_DP_NONE);
		}

#if MBEDTLS_VERSION_MAJOR >= 3
		const uint16_t* get() const { return &list.front(); }
#else
		const mbedtls_ecp_group_id* get() const { return &list.front(); }
#endif

		bool empty() const { return (list.size() <= 1); }
	};

	class X509CertList final
		: public RAIIObj<mbedtls_x509_crt, mbedtls_x509_crt_init, mbedtls_x509_crt_free>
	{
	public:
		/** Import or create empty */
		X509CertList(const std::string& certstr, bool allowempty = false)
		{
			if ((allowempty) && (certstr.empty()))
				return;
			int ret = mbedtls_x509_crt_parse(get(), reinterpret_cast<const unsigned char*>(certstr.c_str()), certstr.size()+1);
			ThrowOnError(ret, "Unable to load certificates");
		}

		bool empty() const { return (get()->raw.p != nullptr); }
	};

	class X509CRL final
		: public RAIIObj<mbedtls_x509_crl, mbedtls_x509_crl_init, mbedtls_x509_crl_free>
	{
	public:
		X509CRL(const std::string& crlstr)
		{
			if (crlstr.empty())
				return;
			int ret = mbedtls_x509_crl_parse(get(), reinterpret_cast<const unsigned char*>(crlstr.c_str()), crlstr.size()+1);
			ThrowOnError(ret, "Unable to load CRL");
		}
	};

	class X509Credentials final
	{
		/** Private key
		 */
		X509Key key;

		/** Certificate list, presented to the peer
		 */
		X509CertList certs;

	public:
		X509Credentials(const std::string& certstr, const std::string& keystr)
			: key(keystr)
			, certs(certstr)
		{
			// Verify that one of the certs match the private key
			bool found = false;
			for (mbedtls_x509_crt* cert = certs.get(); cert; cert = cert->next)
			{
#if MBEDTLS_VERSION_MAJOR >= 3
				if (mbedtls_pk_check_pair(&cert->pk, key.get(), mbedtls_ctr_drbg_random, 0) == 0)
#else
				if (mbedtls_pk_check_pair(&cert->pk, key.get()) == 0)
#endif
				{
					found = true;
					break;
				}
			}
			if (!found)
				throw Exception("Public/private key pair does not match");
		}

		mbedtls_pk_context* getkey() { return key.get(); }
		mbedtls_x509_crt* getcerts() { return certs.get(); }
	};

	class Context final
	{
		mbedtls_ssl_config conf;

#ifdef INSPIRCD_MBEDTLS_LIBRARY_DEBUG
		static void DebugLogFunc(void* userptr, int level, const char* file, int line, const char* msg)
		{
			// Remove trailing \n
			size_t len = strlen(msg);
			if ((len > 0) && (msg[len-1] == '\n'))
				len--;

			ServerInstance->Logs.Debug(MODNAME, "{}:{} {}", file, line, std::string_view(msg, len));
		}
#endif

	public:
		Context(CTRDRBG& ctrdrbg, unsigned int endpoint)
		{
			mbedtls_ssl_config_init(&conf);
#ifdef INSPIRCD_MBEDTLS_LIBRARY_DEBUG
			mbedtls_debug_set_threshold(INT_MAX);
			mbedtls_ssl_conf_dbg(&conf, DebugLogFunc, nullptr);
#endif

			// TODO: check ret of mbedtls_ssl_config_defaults
			mbedtls_ssl_config_defaults(&conf, endpoint, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
			ctrdrbg.SetupConf(&conf);
		}

		~Context()
		{
			mbedtls_ssl_config_free(&conf);
		}

		void SetMinDHBits(unsigned int mindh)
		{
			mbedtls_ssl_conf_dhm_min_bitlen(&conf, mindh);
		}

		void SetDHParams(DHParams& dh)
		{
			mbedtls_ssl_conf_dh_param_ctx(&conf, dh.get());
		}

		void SetX509CertAndKey(X509Credentials& x509cred)
		{
			mbedtls_ssl_conf_own_cert(&conf, x509cred.getcerts(), x509cred.getkey());
		}

		void SetCiphersuites(const Ciphersuites& ciphersuites)
		{
			mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites.get());
		}

		void SetCurves(const Curves& curves)
		{
#if MBEDTLS_VERSION_MAJOR >= 3
			mbedtls_ssl_conf_groups(&conf, curves.get());
#else
			mbedtls_ssl_conf_curves(&conf, curves.get());
#endif
		}

		void SetVersion(int minver, int maxver)
		{
			if (minver)
				mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, minver);
			if (maxver)
				mbedtls_ssl_conf_max_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, maxver);
		}

		void SetCA(X509CertList& certs, X509CRL& crl)
		{
			mbedtls_ssl_conf_ca_chain(&conf, certs.get(), crl.get());
		}

		void SetOptionalVerifyCert()
		{
			mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
		}

		const mbedtls_ssl_config* GetConf() const { return &conf; }
	};

	class Hash final
	{
	private:
		std::vector<std::pair<const mbedtls_md_info_t*, unsigned char>> mds;

		/** Buffer where cert hashes are written temporarily
		 */
		mutable std::vector<unsigned char> buf;

	public:
		Hash(const std::string& hashstr)
		{
			irc::spacesepstream hashstream(hashstr);
			unsigned char mdsize = 0;
			for (std::string hash; hashstream.GetToken(hash); )
			{
				std::transform(hash.begin(), hash.end(), hash.begin(), ::toupper);
				const auto* md = mbedtls_md_info_from_string(hash.c_str());
				if (!md)
					throw Exception("Unknown hash: " + hash);

				mds.push_back(std::make_pair(md, mbedtls_md_get_size(md)));
				mdsize = std::max(mdsize, mds.back().second);
			}

			buf.resize(mdsize);
			buf.shrink_to_fit();
		}

		void hash(const unsigned char* input, size_t length, std::vector<std::string>& fingerprints) const
		{
			for (const auto& [md, size] : mds)
			{
				mbedtls_md(md, input, length, &buf.front());
				fingerprints.push_back(Hex::Encode(&buf.front(), size));
			}
		}
	};

	class Profile final
	{
		/** Name of this profile
		 */
		const std::string name;

		X509Credentials x509cred;

		/** Ciphersuites to use
		 */
		Ciphersuites ciphersuites;

		/** Curves accepted for use in ECDHE and in the peer's end-entity certificate
		 */
		Curves curves;

		Context serverctx;
		Context clientctx;

		DHParams dhparams;

		X509CertList cacerts;

		X509CRL crl;

		/** Hashing algorithm to use when generating certificate fingerprints
		 */
		Hash hash;

		/** Rough max size of records to send
		 */
		const unsigned int outrecsize;

	public:
		struct Config final
		{
			const std::string name;

			CTRDRBG& ctrdrbg;

			const std::string certstr;
			const std::string keystr;
			const std::string dhstr;

			const std::string ciphersuitestr;
			const std::string curvestr;
			const unsigned int mindh;
			const std::string hashstr;

			std::string crlstr;
			std::string castr;

			const int minver;
			const int maxver;
			const unsigned int outrecsize;
			const bool requestclientcert;

			Config(const std::string& profilename, const std::shared_ptr<ConfigTag>& tag, CTRDRBG& ctr_drbg)
				: name(profilename)
				, ctrdrbg(ctr_drbg)
				, certstr(ReadFile(tag->getString("certfile", "cert.pem", 1)))
				, keystr(ReadFile(tag->getString("keyfile", "key.pem", 1)))
				, dhstr(ReadFile(tag->getString("dhfile", "dhparams.pem", 1)))
				, ciphersuitestr(tag->getString("ciphersuites"))
				, curvestr(tag->getString("curves"))
				, mindh(tag->getNum<unsigned int>("mindhbits", 2048))
				, hashstr(tag->getString("hash", "sha256", 1))
				, castr(tag->getString("cafile"))
				, minver(tag->getNum<int>("minver", 0))
				, maxver(tag->getNum<int>("maxver", 0))
				, outrecsize(tag->getNum<unsigned int>("outrecsize", 2048, 512, 16384))
				, requestclientcert(tag->getBool("requestclientcert", true))
			{
				if (!castr.empty())
				{
					castr = ReadFile(castr);
					crlstr = tag->getString("crlfile");
					if (!crlstr.empty())
						crlstr = ReadFile(crlstr);
				}
			}
		};

		Profile(Config& config)
			: name(config.name)
			, x509cred(config.certstr, config.keystr)
			, ciphersuites(config.ciphersuitestr)
			, curves(config.curvestr)
			, serverctx(config.ctrdrbg, MBEDTLS_SSL_IS_SERVER)
			, clientctx(config.ctrdrbg, MBEDTLS_SSL_IS_CLIENT)
			, cacerts(config.castr, true)
			, crl(config.crlstr)
			, hash(config.hashstr)
			, outrecsize(config.outrecsize)
		{
			serverctx.SetX509CertAndKey(x509cred);
			clientctx.SetX509CertAndKey(x509cred);
			clientctx.SetMinDHBits(config.mindh);

			if (!ciphersuites.empty())
			{
				serverctx.SetCiphersuites(ciphersuites);
				clientctx.SetCiphersuites(ciphersuites);
			}

			if (!curves.empty())
			{
				serverctx.SetCurves(curves);
				clientctx.SetCurves(curves);
			}

			serverctx.SetVersion(config.minver, config.maxver);
			clientctx.SetVersion(config.minver, config.maxver);

			if (!config.dhstr.empty())
			{
				dhparams.set(config.dhstr);
				serverctx.SetDHParams(dhparams);
			}

			clientctx.SetOptionalVerifyCert();
			clientctx.SetCA(cacerts, crl);
			// The default for servers is to not request a client certificate from the peer
			if (config.requestclientcert)
			{
				serverctx.SetOptionalVerifyCert();
				serverctx.SetCA(cacerts, crl);
			}
		}

		static std::string ReadFile(const std::string& filename)
		{
			auto file = ServerInstance->Config->ReadFile(filename, ServerInstance->Time());
			if (!file)
				throw Exception("Cannot read file " + filename + ": " + file.error);
			return file.contents;
		}

		/** Set up the given session with the settings in this profile
		 */
		void SetupClientSession(mbedtls_ssl_context* sess)
		{
			mbedtls_ssl_setup(sess, clientctx.GetConf());
		}

		void SetupServerSession(mbedtls_ssl_context* sess)
		{
			mbedtls_ssl_setup(sess, serverctx.GetConf());
		}

		const std::string& GetName() const { return name; }
		X509Credentials& GetX509Credentials() { return x509cred; }
		unsigned int GetOutgoingRecordSize() const { return outrecsize; }
		const Hash& GetHash() const { return hash; }
	};
}

class mbedTLSIOHook final
	: public SSLIOHook
{
private:
	mbedtls_ssl_context sess;

	void CloseSession()
	{
		if (status == STATUS_NONE)
			return;

		mbedtls_ssl_close_notify(&sess);
		mbedtls_ssl_free(&sess);
		certificate = nullptr;
		status = STATUS_NONE;
	}

	// Returns 1 if handshake succeeded, 0 if it is still in progress, -1 if it failed
	int Handshake(StreamSocket* sock)
	{
		int ret = mbedtls_ssl_handshake(&sess);
		if (ret == 0)
		{
			// Change the session state
			this->status = STATUS_OPEN;

			VerifyCertificate();

			// Finish writing, if any left
			SocketEngine::ChangeEventMask(sock, FD_WANT_POLL_READ | FD_WANT_NO_WRITE | FD_ADD_TRIAL_WRITE);

			return 1;
		}

		this->status = STATUS_HANDSHAKING;
		if (ret == MBEDTLS_ERR_SSL_WANT_READ)
		{
			SocketEngine::ChangeEventMask(sock, FD_WANT_POLL_READ | FD_WANT_NO_WRITE);
			return 0;
		}
		else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			SocketEngine::ChangeEventMask(sock, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
			return 0;
		}

		sock->SetError("Handshake Failed - " + mbedTLS::ErrorToString(ret));
		CloseSession();
		return -1;
	}

	// Returns 1 if application I/O should proceed, 0 if it must wait for the underlying protocol to progress, -1 on fatal error
	int PrepareIO(StreamSocket* sock)
	{
		if (status == STATUS_OPEN)
			return 1;
		else if (status == STATUS_HANDSHAKING)
		{
			// The handshake isn't finished, try to finish it
			return Handshake(sock);
		}

		CloseSession();
		sock->SetError("No TLS session");
		return -1;
	}

	void VerifyCertificate()
	{
		this->certificate = new ssl_cert;
		const mbedtls_x509_crt* const cert = mbedtls_ssl_get_peer_cert(&sess);
		if (!cert)
		{
			certificate->error = "No client certificate sent";
			return;
		}

		// If there is a certificate we can always generate a fingerprint
		GetProfile().GetHash().hash(cert->raw.p, cert->raw.len, certificate->fingerprints);

		// At this point mbedTLS verified the cert already, we just need to check the results
		const uint32_t flags = mbedtls_ssl_get_verify_result(&sess);
		if (flags == 0xFFFFFFFF)
		{
			certificate->error = "Internal error during verification";
			return;
		}

		certificate->activation = GetTime(&cert->valid_from);
		certificate->expiration = GetTime(&cert->valid_to);
		if (flags == 0)
		{
			// Verification succeeded
			certificate->trusted = true;
		}
		else
		{
			// Verification failed
			certificate->trusted = false;
			if (flags & MBEDTLS_X509_BADCERT_FUTURE)
			{
				certificate->error = INSP_FORMAT("Certificate not active for {} (on {})",
					Duration::ToString(certificate->activation - ServerInstance->Time()),
					Time::ToString(certificate->activation));
			}
			else if (flags & MBEDTLS_X509_BADCERT_EXPIRED)
			{
				certificate->error = INSP_FORMAT("Certificate expired {} ago (on {})",
					Duration::ToString(ServerInstance->Time() - certificate->expiration),
					Time::ToString(certificate->expiration));
			}
		}

		certificate->unknownsigner = (flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED);
		certificate->revoked = (flags & MBEDTLS_X509_BADCERT_REVOKED);
		certificate->invalid = ((flags & MBEDTLS_X509_BADCERT_BAD_KEY) || (flags & MBEDTLS_X509_BADCERT_BAD_MD) || (flags & MBEDTLS_X509_BADCERT_BAD_PK));

		GetDNString(&cert->subject, certificate->dn);
		GetDNString(&cert->issuer, certificate->issuer);
	}

	static void GetDNString(const mbedtls_x509_name* x509name, std::string& out)
	{
		char buf[512];
		const int ret = mbedtls_x509_dn_gets(buf, sizeof(buf), x509name);
		if (ret <= 0)
			return;

		out.assign(buf, ret);
		for (size_t pos = 0; ((pos = out.find_first_of("\r\n", pos)) != std::string::npos); )
			out[pos] = ' ';
	}

	static time_t GetTime(const mbedtls_x509_time* x509time)
	{
		// HACK: this is terrible but there's no sensible way I can see to get
		// a time_t from this.
		tm ts;
		ts.tm_year = x509time->year - 1900;
		ts.tm_mon  = x509time->mon  - 1;
		ts.tm_mday = x509time->day;
		ts.tm_hour = x509time->hour;
		ts.tm_min  = x509time->min;
		ts.tm_sec  = x509time->sec;

		return timegm(&ts);
	}

	static int Pull(void* userptr, unsigned char* buffer, size_t size)
	{
		StreamSocket* const sock = reinterpret_cast<StreamSocket*>(userptr);
		if (sock->GetEventMask() & FD_READ_WILL_BLOCK)
			return MBEDTLS_ERR_SSL_WANT_READ;

		const ssize_t ret = SocketEngine::Recv(sock, reinterpret_cast<char*>(buffer), size, 0);
		if (ret < 0 || size_t(ret) < size)
		{
			SocketEngine::ChangeEventMask(sock, FD_READ_WILL_BLOCK);
			if ((ret == -1) && (SocketEngine::IgnoreError()))
				return MBEDTLS_ERR_SSL_WANT_READ;
		}

		// This cast isn't entirely safe but the interface is given by mbedtls.
		return int(ret);
	}

	static int Push(void* userptr, const unsigned char* buffer, size_t size)
	{
		StreamSocket* const sock = reinterpret_cast<StreamSocket*>(userptr);
		if (sock->GetEventMask() & FD_WRITE_WILL_BLOCK)
			return MBEDTLS_ERR_SSL_WANT_WRITE;

		const ssize_t ret = SocketEngine::Send(sock, buffer, size, 0);
		if (ret < 0 || size_t(ret) < size)
		{
			SocketEngine::ChangeEventMask(sock, FD_WRITE_WILL_BLOCK);
			if ((ret == -1) && (SocketEngine::IgnoreError()))
				return MBEDTLS_ERR_SSL_WANT_WRITE;
		}

		// This cast isn't entirely safe but the interface is given by mbedtls.
		return int(ret);
	}

public:
	mbedTLSIOHook(const std::shared_ptr<IOHookProvider>& hookprov, StreamSocket* sock, bool isserver)
		: SSLIOHook(hookprov)
	{
		mbedtls_ssl_init(&sess);
		if (isserver)
			GetProfile().SetupServerSession(&sess);
		else
			GetProfile().SetupClientSession(&sess);

		mbedtls_ssl_set_bio(&sess, reinterpret_cast<void*>(sock), Push, Pull, nullptr);

		sock->AddIOHook(this);
		Handshake(sock);
	}

	void OnStreamSocketClose(StreamSocket* sock) override
	{
		CloseSession();
	}

	ssize_t OnStreamSocketRead(StreamSocket* sock, std::string& recvq) override
	{
		// Finish handshake if needed
		int prepret = PrepareIO(sock);
		if (prepret <= 0)
			return prepret;

		// If we resumed the handshake then this->status will be STATUS_OPEN.
		char* const readbuf = ServerInstance->GetReadBuffer();
		const size_t readbufsize = ServerInstance->Config->NetBufferSize;
		int ret = mbedtls_ssl_read(&sess, reinterpret_cast<unsigned char*>(readbuf), readbufsize);
		if (ret > 0)
		{
			recvq.append(readbuf, ret);

			// Schedule a read if there is still data in the mbedTLS buffer
			if (mbedtls_ssl_get_bytes_avail(&sess) > 0)
				SocketEngine::ChangeEventMask(sock, FD_ADD_TRIAL_READ);
			return 1;
		}
		else if (ret == MBEDTLS_ERR_SSL_WANT_READ)
		{
			SocketEngine::ChangeEventMask(sock, FD_WANT_POLL_READ);
			return 0;
		}
		else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE)
		{
			SocketEngine::ChangeEventMask(sock, FD_WANT_NO_READ | FD_WANT_SINGLE_WRITE);
			return 0;
		}
		else if (ret == 0)
		{
			sock->SetError("Connection closed");
			CloseSession();
			return -1;
		}
		else // error or MBEDTLS_ERR_SSL_CLIENT_RECONNECT which we treat as an error
		{
			sock->SetError(mbedTLS::ErrorToString(ret));
			CloseSession();
			return -1;
		}
	}

	ssize_t OnStreamSocketWrite(StreamSocket* sock, StreamSocket::SendQueue& sendq) override
	{
		// Finish handshake if needed
		int prepret = PrepareIO(sock);
		if (prepret <= 0)
			return prepret;

		// Session is ready for transferring application data
		while (!sendq.empty())
		{
			FlattenSendQueue(sendq, GetProfile().GetOutgoingRecordSize());
			const StreamSocket::SendQueue::Element& buffer = sendq.front();
			int ret = mbedtls_ssl_write(&sess, reinterpret_cast<const unsigned char*>(buffer.data()), buffer.length());
			if (ret == (int)buffer.length())
			{
				// Wrote entire record, continue sending
				sendq.pop_front();
			}
			else if (ret > 0)
			{
				sendq.erase_front(ret);
				SocketEngine::ChangeEventMask(sock, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else if (ret == 0)
			{
				sock->SetError("Connection closed");
				CloseSession();
				return -1;
			}
			else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			{
				SocketEngine::ChangeEventMask(sock, FD_WANT_SINGLE_WRITE);
				return 0;
			}
			else if (ret == MBEDTLS_ERR_SSL_WANT_READ)
			{
				SocketEngine::ChangeEventMask(sock, FD_WANT_POLL_READ);
				return 0;
			}
			else
			{
				sock->SetError(mbedTLS::ErrorToString(ret));
				CloseSession();
				return -1;
			}
		}

		SocketEngine::ChangeEventMask(sock, FD_WANT_NO_WRITE);
		return 1;
	}

	void GetCiphersuite(std::string& out) const override
	{
		if (!IsHookReady())
			return;
		out.append(mbedtls_ssl_get_version(&sess)).push_back('-');

		// All mbedTLS ciphersuite names currently begin with "TLS-" which provides no useful information so skip it, but be prepared if it changes
		const char* const ciphersuitestr = mbedtls_ssl_get_ciphersuite(&sess);
		const char prefix[] = "TLS-";
		unsigned int skip = sizeof(prefix)-1;
		if (strncmp(ciphersuitestr, prefix, sizeof(prefix)-1) != 0)
			skip = 0;
		out.append(ciphersuitestr + skip);
	}

	bool GetServerName(std::string& out) const override
	{
		// TODO: Implement SNI support.
		return false;
	}

	mbedTLS::Profile& GetProfile();
};

class mbedTLSIOHookProvider final
	: public SSLIOHookProvider
{
	mbedTLS::Profile profile;

public:
	mbedTLSIOHookProvider(Module* mod, mbedTLS::Profile::Config& config)
		: SSLIOHookProvider(mod, config.name)
		, profile(config)
	{
		ServerInstance->Modules.AddService(*this);
	}

	~mbedTLSIOHookProvider() override
	{
		ServerInstance->Modules.DelService(*this);
	}

	void OnAccept(StreamSocket* sock, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server) override
	{
		new mbedTLSIOHook(shared_from_this(), sock, true);
	}

	void OnConnect(StreamSocket* sock) override
	{
		new mbedTLSIOHook(shared_from_this(), sock, false);
	}

	mbedTLS::Profile& GetProfile() { return profile; }
};

mbedTLS::Profile& mbedTLSIOHook::GetProfile()
{
	return std::static_pointer_cast<mbedTLSIOHookProvider>(prov)->GetProfile();
}

class ModuleSSLmbedTLS final
	: public Module
{
private:
	typedef std::vector<std::shared_ptr<mbedTLSIOHookProvider>> ProfileList;

	mbedTLS::Entropy entropy;
	mbedTLS::CTRDRBG ctr_drbg;
	ProfileList profiles;

	void ReadProfiles()
	{
		// First, store all profiles in a new, temporary container. If no problems occur, swap the two
		// containers; this way if something goes wrong we can go back and continue using the current profiles,
		// avoiding unpleasant situations where no new TLS connections are possible.
		ProfileList newprofiles;

		auto tags = ServerInstance->Config->ConfTags("sslprofile");
		if (tags.empty())
			throw ModuleException(this, "You have not specified any <sslprofile> tags that are usable by this module!");

		for (const auto& [_, tag] : tags)
		{
			if (!insp::equalsci(tag->getString("provider", "mbedtls", 1), "mbedtls"))
			{
				ServerInstance->Logs.Debug(MODNAME, "Ignoring non-mbedTLS <sslprofile> tag at " + tag->source.str());
				continue;
			}

			const std::string name = tag->getString("name");
			if (name.empty())
			{
				ServerInstance->Logs.Warning(MODNAME, "Ignoring <sslprofile> tag without name at " + tag->source.str());
				continue;
			}

			std::shared_ptr<mbedTLSIOHookProvider> prov;
			try
			{
				mbedTLS::Profile::Config profileconfig(name, tag, ctr_drbg);
				prov = std::make_shared<mbedTLSIOHookProvider>(this, profileconfig);
			}
			catch (const CoreException& ex)
			{
				throw ModuleException(this, "Error while initializing TLS profile \"" + name + "\" at " + tag->source.str() + " - " + ex.GetReason());
			}

			newprofiles.push_back(prov);
		}

		// New profiles are ok, begin using them
		// Old profiles are deleted when their refcount drops to zero
		for (const auto& profile : profiles)
			ServerInstance->Modules.DelService(*profile);

		profiles.swap(newprofiles);
	}

public:
	ModuleSSLmbedTLS()
		: Module(VF_VENDOR, "Allows TLS encrypted connections using the mbedTLS library.")
	{
		thismod = this;
	}

	void init() override
	{
		char verbuf[16]; // Should be at least 9 bytes in size
		mbedtls_version_get_string(verbuf);

		ServerInstance->Logs.Normal(MODNAME, "Module was compiled against mbedTLS version {} and is running against version {}",
			MBEDTLS_VERSION_STRING, verbuf);

		if (!ctr_drbg.Seed(entropy))
			throw ModuleException(this, "CTR DRBG seed failed");
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("mbedtls");
		if (status.initial || tag->getBool("onrehash", true))
		{
			// Try to help people who have outdated configs.
			for (const auto& field : {"cafile", "certfile", "ciphersuites", "crlfile", "curves", "dhfile", "hash", "keyfile", "maxver", "mindhbits", "minver", "outrecsize", "requestclientcert"})
			{
				if (!tag->getString(field).empty())
					throw ModuleException(this, "TLS settings have moved from <mbedtls> to <sslprofile>. See " INSPIRCD_DOCS "modules/ssl_mbedtls/#sslprofile for more information.");
			}
			ReadProfiles();
		}
	}

	void OnModuleRehash(User* user, const std::string& param) override
	{
		if (!irc::equals(param, "tls") && !irc::equals(param, "ssl"))
			return;

		try
		{
			ReadProfiles();
			ServerInstance->SNO.WriteToSnoMask('r', "mbedTLS TLS profiles have been reloaded.");
		}
		catch (const ModuleException& ex)
		{
			ServerInstance->SNO.WriteToSnoMask('r', "Failed to reload the mbedTLS TLS profiles. " + ex.GetReason());
		}
	}

	void OnCleanup(ExtensionType type, Extensible* item) override
	{
		if (type != ExtensionType::USER)
			return;

		LocalUser* user = IS_LOCAL(static_cast<User*>(item));
		if ((user) && (user->eh.GetModHook(this)))
		{
			// User is using TLS, they're a local user, and they're using our IOHook.
			// Potentially there could be multiple TLS modules loaded at once on different ports.
			ServerInstance->Users.QuitUser(user, "mbedTLS module unloading");
		}
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		const mbedTLSIOHook* const iohook = static_cast<mbedTLSIOHook*>(user->eh.GetModHook(this));
		if ((iohook) && (!iohook->IsHookReady()))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleSSLmbedTLS)
