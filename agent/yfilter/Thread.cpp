#include "yfilter.h"

//#define USE_PsSetCreateThreadNotifyRoutineEx

/*
void fgt(PVOID* Callers, ULONG Count)
{
	NTSTATUS status;

	ULONG cb = 0x10000;
	do
	{
		status = STATUS_INSUFFICIENT_RESOURCES;

		if (PRTL_PROCESS_MODULES prpm = (PRTL_PROCESS_MODULES)ExAllocatePool(PagedPool, cb))
		{
			if (0 <= (status = NtQuerySystemInformation(SystemModuleInformation, prpm, cb, &cb)))
			{
				do
				{
					PVOID Caller = *Callers++;

					if (ULONG NumberOfModules = prpm->NumberOfModules)
					{
						PRTL_PROCESS_MODULE_INFORMATION Modules = prpm->Modules;

						do
						{
							if ((SIZE_T)Caller - (SIZE_T)Modules->ImageBase < Modules->ImageSize)
							{
								DbgPrint("%p> %s\n", Caller, Modules->FullPathName);
								break;
							}
						} while (Modules++, --NumberOfModules);
					}

				} while (--Count);
			}
			ExFreePool(prpm);
		}

	} while (status == STATUS_INFO_LENGTH_MISMATCH);
}
*/

#ifndef THREAD_QUERY_INFORMATION
#define THREAD_QUERY_INFORMATION (0x0040)	
/*
THREAD_QUERY_INFORMATION (0x0040)	Required to read certain information from the thread object, such as the exit code (see GetExitCodeThread).
THREAD_QUERY_LIMITED_INFORMATION (0x0800)	Required to read certain information from the thread objects (see GetProcessIdOfThread). A handle that has the THREAD_QUERY_INFORMATION access right is automatically granted THREAD_QUERY_LIMITED_INFORMATION.Windows Server 2003 and Windows XP: This access right is not supported.
*/
#endif

NTSTATUS	GetThreadWin32StartAddress(IN HANDLE hThreadId, OUT PVOID * pAddress)
{
	NTSTATUS	status	= STATUS_UNSUCCESSFUL;
	HANDLE		hThread	= NULL;
	PETHREAD	pThread	= NULL;

	if (NULL == Config()->pZwQueryInformationThread)	return STATUS_BAD_FUNCTION_TABLE;
	if( NULL == pAddress )								return STATUS_INVALID_PARAMETER;

	__try {
		status	= PsLookupThreadByThreadId(hThreadId, &pThread);
		if( NT_FAILED(status) ) {
			__log("%s PsLookupThreadByThreadId() failed. status=%08x", __FUNCTION__, status);
			__leave;
		}
		status	= ObOpenObjectByPointer(pThread, OBJ_KERNEL_HANDLE, NULL, 
					THREAD_QUERY_LIMITED_INFORMATION,
					*PsThreadType, KernelMode, &hThread);
		//	PsLookupThreadByThreadId 에 의해 참조된 object를 아래와 같이 해줘야 함.
		ObDereferenceObject(pThread);
		if( NT_SUCCESS(status)) {
			
		}
		else {
			__log("%s ObOpenObjectByPointer() failed. status=%08x", __FUNCTION__, status);
			__leave;
		}
		status	= Config()->pZwQueryInformationThread(hThread, ThreadQuerySetWin32StartAddress, 
							pAddress, sizeof(PVOID), NULL);
		if (NT_SUCCESS(status)) {
			//__log("%s startaddress=%p", __FUNCTION__, *pAddress);
		}
		else {
			__log("%s NtQueryInformationThread() failed. status=%08x", __FUNCTION__, status);
		}
	}
	__finally
	{
		if( hThread )	ObCloseHandle(hThread, KernelMode);
	}
	return status;
}

NTSTATUS	GetThreadTimes(IN HANDLE hThreadId, KERNEL_USER_TIMES * p)
{
	NTSTATUS	status = STATUS_UNSUCCESSFUL;
	HANDLE		hThread = NULL;
	PETHREAD	pThread = NULL;

	if (NULL == Config()->pZwQueryInformationThread)	return STATUS_BAD_FUNCTION_TABLE;
	if (NULL == p)										return STATUS_INVALID_PARAMETER;

	__try {
		status = PsLookupThreadByThreadId(hThreadId, &pThread);
		if (NT_FAILED(status)) {
			__log("%s PsLookupThreadByThreadId() failed. status=%08x", __FUNCTION__, status);
			__leave;
		}
		status = ObOpenObjectByPointer(pThread, OBJ_KERNEL_HANDLE, NULL,
			THREAD_QUERY_LIMITED_INFORMATION,
			*PsThreadType, KernelMode, &hThread);
		//	PsLookupThreadByThreadId 에 의해 참조된 object를 아래와 같이 해줘야 함.
		ObDereferenceObject(pThread);
		if (NT_SUCCESS(status)) {

		}
		else {
			__log("%s ObOpenObjectByPointer() failed. status=%08x", __FUNCTION__, status);
			__leave;
		}
		status = Config()->pZwQueryInformationThread(hThread, ThreadTimes,
			p, sizeof(KERNEL_USER_TIMES), NULL);
		if (NT_SUCCESS(status)) {
			//__log("%s startaddress=%p", __FUNCTION__, *pAddress);
		}
		else {
			__log("%s NtQueryInformationThread() failed. status=%08x", __FUNCTION__, status);
		}
	}
	__finally
	{
		if (hThread)	ObCloseHandle(hThread, KernelMode);
	}
	return status;
}

#ifdef USE_PsSetCreateThreadNotifyRoutineEx


VOID	CreateThreadNotifyRoutineEx(
	_Inout_ PEPROCESS Process,
	_In_ HANDLE ProcessId,
	_Inout_opt_ PPS_CREATE_INFO CreateInfo
)
{
	UNREFERENCED_PARAMETER(Process);
	UNREFERENCED_PARAMETER(CreateInfo);
	//	이 루틴의 경우 윈도7도 지원하지만 호출되는 부분이 
	//	새로 만들어진 스레드가 아니라, 
	//	스레드를 만드는 부모 스레드에서 호출이 된다. 
	//	하지만 종료는 그 스레드 context에서 호출된다.
	__log("%s %d %p", __FUNCTION__, ProcessId, CreateInfo);
	//GetSystemTimes()

	if (NULL == Config()) {
		__log("%s Config() is NULL.", __FUNCTION__);
		return;
	}

}
bool		StartThreadFilter()
{
	__log("%s", __FUNCTION__);
	NTSTATUS	status = STATUS_SUCCESS;
	if (NULL == Config())	return false;

	__try
	{
		if (false == Config()->bThreadNotifyCallbackEx &&
			Config()->pPsSetCreateThreadNotifyRoutineEx ) {
			PsSetCreateThreadNotifyRoutineEx(PsCreateThreadNotifyNonSystem, )
				CreateThreadNotifyRoutineEx, FALSE);
			if (NT_SUCCESS(status)) {
				Config()->bThreadNotifyCallbackEx = true;
				__log("%s succeeded.", __FUNCTION__);
			}
			else {
				Config()->bThreadNotifyCallbackEx = false;
				__log("%s pPsSetCreateThreadNotifyRoutineEx() failed. status=%08x", __FUNCTION__, status);
				__leave;
			}

		}
	}
	__finally
	{

	}
	return Config()->bModuleNotifyCallback;
}
void		StopThreadFilter(void)
{
	__log("%s", __FUNCTION__);
	if (NULL == Config())	return;

	if (Config()->bThreadNotifyCallbackEx) {
		Config()->pPsSetCreateThreadNotifyRoutineEx(
			CreateThreadNotifyRoutineEx, TRUE);
		Config()->bThreadNotifyCallback = false;
	}
}
#else

/*
	dll injection 방지에 대한 재미있는 이야기.
	https://patents.google.com/patent/KR101097590B1/ko

	상기 획득한 LoadLibrary(xx) 함수 주소와 쓰레드 커널 오브젝트의 실제 시작함수 주소를 
	비교하여 같을 경우에는 DLL 인젝션이 이뤄지고 있음을 확인할 수 있다. 
	
	상기 DLL 인젝션으로 판단된 경우에 상기 획득한 쓰레드 커널 오브젝트의 파라미터 메모리에 
	인젝션될 DLL의 전체 경로가 존재하게 되며, 이 메모리를 초기화한다. 
	
	이렇게 되면 LoadLibrary(xx) 함수는 로드할 DLL의 경로가 없으므로 
	자연스럽게 실패하게 되어 인젝션은 이루어지지 않게 된다.
*/

#define TAG_THREAD			'thrd'

KSPIN_LOCK	m_lock;

NTSTATUS	AddEearlyProcess(HANDLE hPID);

void CreateThreadNotifyRoutine(
	HANDLE		ProcessId,
	HANDLE		ThreadId,
	BOOLEAN		Create
)
{
	//	이 루틴의 경우 윈도7도 지원하지만 호출되는 부분이 
	//	새로 만들어진 스레드가 아니라, 
	//	스레드를 만드는 부모 스레드에서 호출이 된다. 
	//	하지만 종료는 그 스레드 context에서 호출된다.
	//__log("%s %d %d %d", __FUNCTION__, ProcessId, ThreadId, Create);
	//GetSystemTimes()

	if (NULL == Config()) {
		__log("%s Config() is NULL.", __FUNCTION__);
		return;
	}

	//__log(__LINE);
	PVOID	pStartAddress = NULL;
	GetThreadWin32StartAddress(ThreadId, &pStartAddress);
	//__log("[%d] process %p thread %p", Create, PsGetCurrentThreadId(), pStartAddress);

	HANDLE	hCurrentProcessId = PsGetCurrentProcessId();

	CWSTRBuffer		dest;
	if (NT_FAILED(GetProcessImagePathByProcessId(ProcessId, dest,
		(ULONG)dest.CbSize(), NULL))) {
		__log("%s GetProcessImagePathByProcessId(%d) failed.", __FUNCTION__, ProcessId);
		return;
	}
	CWSTRBuffer		src;
	if (NT_FAILED(GetProcessImagePathByProcessId(hCurrentProcessId, src,
		(ULONG)src.CbSize(), NULL))) {
		__log("%s GetProcessImagePathByProcessId(%d) failed.", __FUNCTION__, hCurrentProcessId);
		return;
	}

	PCWSTR				pSrcName	= NULL, 
						pDestName	= NULL;
	bool				bShow		= false;
	pSrcName	= wcsrchr((PCWSTR)src, L'\\');
	if( NULL == pSrcName )	pSrcName	= (PCWSTR)src;
	pDestName	= wcsrchr((PCWSTR)dest, L'\\');
	if( NULL == pDestName )	pDestName	= (PCWSTR)dest;

	if (!_wcsicmp(pSrcName, L"\\baretail.exe")	||
		!_wcsicmp(pDestName, L"\\baretail.exe")	||
		!_wcsicmp(pSrcName, L"SYSTEM")			||
		!_wcsicmp(pDestName, L"SYSTEM")	
	) {
		bShow	= true;
	}
	if ((HANDLE)4 == hCurrentProcessId) {
		//__log("%s system thread", __FUNCTION__);
		//bShow	= true;
		return;
	}

	bShow	= false;
	if( bShow )	
	{
		__log("%6d %ws => %6d %ws", hCurrentProcessId, pSrcName, ProcessId, pDestName);
		__log("  %10s %d", Create? "create":"terminate", ThreadId);
		__log("  ExGetPreviousMode()=%d", ExGetPreviousMode());
		__log("  PsIsSystemThread(current) %d", PsIsSystemThread(KeGetCurrentThread()));

		NTSTATUS	status;
		PETHREAD	pThread = NULL;
		status = PsLookupThreadByThreadId(ThreadId, &pThread);
		if( NT_SUCCESS(status)) {
			__log("  PsIsSystemThread(target) %d", PsIsSystemThread(pThread));
		}
	}
	if ((HANDLE)4 == hCurrentProcessId) {
		__log("%s system thread", __FUNCTION__);
	}
	if (KeGetCurrentIrql() != PASSIVE_LEVEL ||
		(HANDLE)4 == ProcessId || PsIsSystemThread(KeGetCurrentThread()))
		return;
	//KPROCESSOR_MODE;
	if ((HANDLE)4 == hCurrentProcessId) {
		return;
	}

	PYFILTER_MESSAGE	pMsg	= NULL;
	pMsg = (PYFILTER_MESSAGE)CMemory::Allocate(NonPagedPoolNx, sizeof(YFILTER_MESSAGE), 
			TAG_THREAD);

	if (pMsg) {
		RtlZeroMemory(pMsg, sizeof(YFILTER_MESSAGE));

		pMsg->header.mode = YFilter::Message::Mode::Event;
		pMsg->header.category = YFilter::Message::Category::Thread;
		pMsg->header.size = sizeof(YFILTER_MESSAGE);
		pMsg->data.dwProcessId = (DWORD)ProcessId;
		//__log("%ws", msg.data.szCommand);

		pMsg->data.dwThreadId = (DWORD)ThreadId;
		pMsg->data.pStartAddress = (ULONG_PTR)pStartAddress;
		pMsg->data.dwCreateProcessId = (DWORD)hCurrentProcessId;

		RtlStringCbCopyW(pMsg->data.szPath, sizeof(pMsg->data.szPath),
			(PCWSTR)src);
	}
	else {
		return;
	}

	if (ProcessTable()->IsExisting(ProcessId, pMsg, [](
		IN bool					bCreationSaved,
		IN PPROCESS_ENTRY		pEntry,
		IN PVOID				pCallbackPtr
		) {
		PYFILTER_MESSAGE	pMsg = (PYFILTER_MESSAGE)pCallbackPtr;
		if (bCreationSaved)
			RtlCopyMemory(&pMsg->data.ProcGuid, &pEntry->uuid, sizeof(pEntry->uuid));
	})) {

	}
	else {
		AddEearlyProcess(ProcessId);
		PUNICODE_STRING	pImageFileName = NULL;
		if (NT_SUCCESS(GetProcessImagePathByProcessId(ProcessId, &pImageFileName)))
		{
			KERNEL_USER_TIMES	times;
			GetProcessTimes(ProcessId, &times);
			RtlStringCbCopyUnicodeString(pMsg->data.szPath, sizeof(pMsg->data.szPath), pImageFileName);
			GetProcGuid(false, ProcessId, NULL, pImageFileName, &times.CreateTime,
				&pMsg->data.ProcGuid);
			CMemory::Free(pImageFileName);
		}
	}

	if (Create ) {
		pMsg->data.subType = YFilter::Message::SubType::ThreadStart;
		bool	bSameProcess = (hCurrentProcessId == ProcessId);
		if (false == bSameProcess)
		{
			//__log("%s Process(%d) => Process(%d)", 
			//	__FUNCTION__, hCurrentProcessId, ProcessId);
		}
		//	여기서 바로 SendMessage 했더니 데드락 처럼 되버리더라.
		//SendMessage(__FUNCTION__, &Config()->client.event, &msg, sizeof(msg));
		//	KeGetCurrentThread
		//	This routine is identical to PsGetCurrentThread.
		//	다만 리턴값이 다른데 어차피 시작 주소가 같은 애들이라 같다고 봐도 무방할 듯.
		//	생성하려는 스레드가 자기가 아닌 다른 프로세스인가요?
		//	그럼 code injection 이라 봅니다.
	}
	else {
		//	문제가 있어..
		//	스레드 종료시 호출되니까 이 스레드는 곧 사라질 녀석인데 
		//	이걸 비동기로 앱으로 전해줘도 앱에서 해당 스레드 정보를 구하려 할 때
		//	이 스레드가 이미 사라지고 없어질 수 있단 말이지.
		//FN_NtQueryInformationThread;
		// KeQuerySystemTimePrecise

		//	재미있는 이야기:
		//	thread의 start address 가 LoadLibraryA, LoadLibraryW 와 주소가 같으면 
		//	dll injection 이라고 한다.

		pMsg->data.subType = YFilter::Message::SubType::ThreadStop;
		//	해당 스레드의 리소스 사용율을 구한다.
		KERNEL_USER_TIMES	times;
		GetThreadTimes(ThreadId, &times);
		KeQuerySystemTime(&times.ExitTime);
		pMsg->data.times.CreateTime	= times.CreateTime;
		pMsg->data.times.ExitTime	= times.ExitTime;
		pMsg->data.times.KernelTime	= times.KernelTime;
		pMsg->data.times.UserTime	= times.UserTime;
	}

	if (pMsg) {
		if (MessageThreadPool()->Push(
			__FUNCTION__,
			YFilter::Message::Mode::Event,
			YFilter::Message::Category::Thread,
			pMsg, sizeof(YFILTER_MESSAGE),
			bShow)) {
			//	pMsg는 SendMessage 성공 후 해제될 것이다. 
			MessageThreadPool()->Alert(YFilter::Message::Category::Thread);
			pMsg	= NULL;
		}
	}
	/*
	if (bTest) {
		if( pMsg )	CMemory::Free(pMsg);
	}
	else {
		if (false == bQueued && pMsg ) {
			if (MessageThreadPool()->Push(
				__FUNCTION__,
				YFilter::Message::Mode::Event,
				YFilter::Message::Category::Thread,
				pMsg, sizeof(YFILTER_MESSAGE), 
				bShow)) {
				//	pMsg는 SendMessage 성공 후 해제될 것이다. 
				MessageThreadPool()->Alert(YFilter::Message::Category::Thread);
			}
			else
			{
				CMemory::Free(pMsg);
			}
		}
	}
	*/
	if( pMsg )
	{
		CMemory::Free(pMsg);
	}
}
bool		StartThreadFilter()
{
	__log("%s", __FUNCTION__);
	NTSTATUS	status = STATUS_SUCCESS;
	if (NULL == Config())	return false;

	KeInitializeSpinLock(&m_lock);
	__try
	{
		if (false == Config()->bThreadNotifyCallback) {
			status = PsSetCreateThreadNotifyRoutine(CreateThreadNotifyRoutine);
			if (NT_SUCCESS(status)) {
				Config()->bThreadNotifyCallback = true;
				__log("%s succeeded.", __FUNCTION__);
			}
			else {
				Config()->bThreadNotifyCallback	= false;
				__log("%s PsSetCreateThreadNotifyRoutine() failed. status=%08x", __FUNCTION__, status);
				__leave;
			}
		}
		if (false == Config()->bThreadNotifyCallbackEx) {

			Config()->bThreadNotifyCallbackEx	= true;
		}
	}
	__finally
	{

	}
	return Config()->bModuleNotifyCallback;
}
void		StopThreadFilter(void)
{
	__log("%s", __FUNCTION__);
	if (NULL == Config())	return;

	if (Config()->bThreadNotifyCallback) {
		PsRemoveCreateThreadNotifyRoutine(CreateThreadNotifyRoutine);
		Config()->bThreadNotifyCallback = false;
	}
	if (Config()->bThreadNotifyCallbackEx) {
		Config()->bThreadNotifyCallbackEx	= false;
	}
}
#endif