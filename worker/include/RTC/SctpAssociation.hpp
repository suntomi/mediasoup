#ifndef MS_RTC_SCTP_ASSOCIATION_HPP
#define MS_RTC_SCTP_ASSOCIATION_HPP

#include "common.hpp"
#include "Utils.hpp"
#include "RTC/DataConsumer.hpp"
#include "RTC/DataProducer.hpp"
#include <unordered_map>
#include <vector>
#include <usrsctp.h>

namespace RTC
{
	class SctpAssociation
	{
	public:
		enum class SctpState
		{
			NEW = 1,
			CONNECTING,
			CONNECTED,
			FAILED,
			CLOSED
		};

	private:
		struct MessageKey
		{
			uint16_t streamId{ 0u };
			uint16_t ssn{ 0u };
			uint32_t ppid{ 0u };

			bool operator==(const MessageKey& other) const
			{
				return this->streamId == other.streamId && this->ssn == other.ssn && this->ppid == other.ppid;
			}
		};

		struct MessageKeyHash
		{
			size_t operator()(const MessageKey& key) const
			{
				return static_cast<size_t>(key.streamId) ^ (static_cast<size_t>(key.ssn) << 16u) ^
				       (static_cast<size_t>(key.ppid) << 32u);
			}
		};

		struct MessageBuffer
		{
			std::vector<uint8_t> data;
		};

		enum class StreamDirection
		{
			INCOMING = 1,
			OUTGOING
		};

	protected:
		using onQueuedCallback = const std::function<void(bool queued, bool sctpSendBufferFull)>;

	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnSctpAssociationConnecting(RTC::SctpAssociation* sctpAssociation) = 0;
			virtual void OnSctpAssociationConnected(RTC::SctpAssociation* sctpAssociation)  = 0;
			virtual void OnSctpAssociationFailed(RTC::SctpAssociation* sctpAssociation)     = 0;
			virtual void OnSctpAssociationClosed(RTC::SctpAssociation* sctpAssociation)     = 0;
			virtual void OnSctpAssociationSendData(
			  RTC::SctpAssociation* sctpAssociation, const uint8_t* data, size_t len) = 0;
			virtual void OnSctpAssociationMessageReceived(
			  RTC::SctpAssociation* sctpAssociation,
			  uint16_t streamId,
			  const uint8_t* msg,
			  size_t len,
			  uint32_t ppid) = 0;
			virtual void OnSctpAssociationBufferedAmount(
			  RTC::SctpAssociation* sctpAssociation, uint32_t len) = 0;
			virtual void OnSctpStreamReset(RTC::SctpAssociation* sctpAssociation, uint16_t streamId) = 0;
			virtual void OnSctpWebRtcDataChannelControlDataReceived(
			  RTC::SctpAssociation* sctpAssociation,
			  uint16_t streamId,
			  const uint8_t* msg,
			  size_t len) = 0;
		};

	public:
		static bool IsSctp(const uint8_t* data, size_t len)
		{
			// clang-format off
			return (
				(len >= 12) &&
				// Must have Source Port Number and Destination Port Number set to 5000 (hack).
				(Utils::Byte::Get2Bytes(data, 0) == 5000) &&
				(Utils::Byte::Get2Bytes(data, 2) == 5000)
			);
			// clang-format on
		}

	public:
		SctpAssociation(
		  Listener* listener,
		  uint16_t os,
		  uint16_t mis,
		  size_t maxSctpMessageSize,
		  size_t sctpSendBufferSize,
		  bool isDataChannel);
		~SctpAssociation();

	public:
		flatbuffers::Offset<FBS::SctpParameters::SctpParameters> FillBuffer(
		  flatbuffers::FlatBufferBuilder& builder) const;
		void TransportConnected();
		SctpState GetState() const
		{
			return this->state;
		}
		size_t GetSctpBufferedAmount() const
		{
			return this->sctpBufferedAmount;
		}
		void ProcessSctpData(const uint8_t* data, size_t len) const;
		int SendSctpMessage(
		  RTC::DataConsumer* dataConsumer,
		  const uint8_t* msg,
		  size_t len,
		  uint32_t ppid,
		  onQueuedCallback* cb = nullptr) {
			int r = SendSctpMessage(dataConsumer->GetSctpStreamParameters(), msg, len, ppid, cb);
			if (r == SctpSendResult::ErrorAgain) {
				dataConsumer->SctpAssociationSendBufferFull();
			}
			return r;
		}
		void HandleDataConsumer(RTC::DataConsumer* dataConsumer) {
			HandleDataConsumer(dataConsumer->GetSctpStreamParameters().streamId);
		}
		void DataProducerClosed(RTC::DataProducer* dataProducer) {
			DataProducerClosed(dataProducer->GetSctpStreamParameters().streamId);
		}
		void DataConsumerClosed(RTC::DataConsumer* dataConsumer) {
			DataConsumerClosed(dataConsumer->GetSctpStreamParameters().streamId);
		}

		enum SctpSendResult {
			Ok = 0,
			ErrorAgain = -1,
			ErrorOthers = -2,
		};
		int SendSctpMessage(
		  const RTC::SctpStreamParameters &parameters,
		  const uint8_t* msg,
		  size_t len,
		  uint32_t ppid,
		  onQueuedCallback* cb = nullptr);
		void HandleDataConsumer(uint16_t streamId);
		void DataProducerClosed(uint16_t streamId);
		void DataConsumerClosed(uint16_t streamId);

	private:
		void ResetSctpStream(uint16_t streamId, StreamDirection direction);
		void AddOutgoingStreams(bool force = false);

		/* Callbacks fired by usrsctp events. */
	public:
		void OnUsrSctpSendSctpData(void* buffer, size_t len);
		void OnUsrSctpReceiveSctpData(
		  uint16_t streamId, uint16_t ssn, uint32_t ppid, int flags, const uint8_t* data, size_t len);
		void OnUsrSctpReceiveSctpNotification(union sctp_notification* notification, size_t len);
		void OnUsrSctpSentData(uint32_t freeBuffer);

	public: /* fix for multi threaded case  */
		static void SetSctpThreadId(uint16_t threadId);
		static void ClearSctpThreadId();
		static uintptr_t GetSctpThreadId();
		static uintptr_t GetNextSctpAssociationId();

	public:
		uintptr_t id{ 0u };

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		uint16_t os{ 1024u };
		uint16_t mis{ 1024u };
		size_t maxSctpMessageSize{ 262144u };
		size_t sctpSendBufferSize{ 262144u };
		size_t sctpBufferedAmount{ 0u };
		bool isDataChannel{ false };
		std::unordered_map<MessageKey, MessageBuffer, MessageKeyHash> messageBuffers;
		// Others.
		SctpState state{ SctpState::NEW };
		struct socket* socket{ nullptr };
		uint16_t desiredOs{ 0u };
	};
} // namespace RTC

#endif
