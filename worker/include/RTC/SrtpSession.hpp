#ifndef MS_RTC_SRTP_SESSION_HPP
#define MS_RTC_SRTP_SESSION_HPP

#include "common.hpp"
#include "FBS/srtpParameters.h"
#include <srtp.h>

namespace RTC
{
	class SrtpSession
	{
	public:
		enum class CryptoSuite
		{
			AEAD_AES_256_GCM = 0,
			AEAD_AES_128_GCM,
			AES_CM_128_HMAC_SHA1_80,
			AES_CM_128_HMAC_SHA1_32,
		};

	public:
		enum class Type
		{
			INBOUND = 1,
			OUTBOUND
		};

	public:
		static void ClassInit();
		static FBS::SrtpParameters::SrtpCryptoSuite CryptoSuiteToFbs(CryptoSuite cryptoSuite);
		static CryptoSuite CryptoSuiteFromFbs(FBS::SrtpParameters::SrtpCryptoSuite cryptoSuite);

	private:
		static void OnSrtpEvent(srtp_event_data_t* data);

	public:
		SrtpSession(Type type, CryptoSuite cryptoSuite, uint8_t* key, size_t keyLen);
		~SrtpSession();

		inline srtp_t context() const { return this->session; }
		static srtp_policy_t CreatePolicy(Type type, uint32_t ssrc, CryptoSuite cryptoSuite, uint8_t* key, size_t keyLen);
		bool SetRoc(uint32_t ssrc, uint32_t roc, const std::string& srtp_remote_key, CryptoSuite srtp_crypto_suite);
		bool GetRoc(uint32_t ssrc, uint32_t& roc);

	public:
		bool EncryptRtp(const uint8_t** data, size_t* len);
		bool DecryptSrtp(uint8_t* data, size_t* len);
		bool EncryptRtcp(const uint8_t** data, size_t* len);
		bool DecryptSrtcp(uint8_t* data, size_t* len);
		void RemoveStream(uint32_t ssrc)
		{
			// FIX(iyatomi): srtp_t manage streams that belong to it by using ssrc of network byte order.
			// srtp_stream_remove convert ssrc to network byte order by using htonl.
			// here, ssrc should be host byte order, htonl is not required. 
			// now srtp_stream_remove can correctly remove stream with that ssrc
			srtp_stream_remove(this->session, ssrc);
		}

	private:
		// Allocated by this.
		srtp_t session{ nullptr };
	};
} // namespace RTC

#endif
