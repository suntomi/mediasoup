#ifndef MS_DEP_USRSCTP_HPP
#define MS_DEP_USRSCTP_HPP

#include "common.hpp"
#include "RTC/SctpAssociation.hpp"
#include "handles/TimerHandle.hpp"
#include <absl/container/flat_hash_map.h>

typedef int (*SendStcpDataCB)(void* addr, void* data, size_t len, uint8_t /*tos*/, uint8_t /*setDf*/);

class DepUsrSCTP
{
private:
	class Checker : public TimerHandle::Listener
	{
	public:
		Checker();
		~Checker() override;

	public:
		void Start();
		void Stop();

		/* Pure virtual methods inherited from TimerHandle::Listener. */
	public:
		void OnTimer(TimerHandle* timer) override;

	private:
		TimerHandle* timer{ nullptr };
		uint64_t lastCalledAtMs{ 0u };
	};

public:
	static void ClassInit(SendStcpDataCB cb = nullptr);
	static void ClassDestroy();
	static void CreateChecker();
	static void CloseChecker();
	static uintptr_t GetNextSctpAssociationId();
	static void RegisterSctpAssociation(RTC::SctpAssociation* sctpAssociation);
	static void DeregisterSctpAssociation(RTC::SctpAssociation* sctpAssociation);
	static RTC::SctpAssociation* RetrieveSctpAssociation(uintptr_t id);
	static absl::flat_hash_map<uintptr_t, RTC::SctpAssociation*> &associations()
	{
		return mapIdSctpAssociation;
	}
	static size_t CheckInterval();

private:
	thread_local static Checker* checker;
	static uint64_t numSctpAssociations;
	static uintptr_t nextSctpAssociationId;
	static absl::flat_hash_map<uintptr_t, RTC::SctpAssociation*> mapIdSctpAssociation;
};

#endif
