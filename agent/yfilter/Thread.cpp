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
		//	PsLookupThreadByThreadId �� ���� ������ object�� �Ʒ��� ���� ����� ��.
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
		//	PsLookupThreadByThreadId �� ���� ������ object�� �Ʒ��� ���� ����� ��.
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
	//	�� ��ƾ�� ��� ����7�� ���������� ȣ��Ǵ� �κ��� 
	//	���� ������� �����尡 �ƴ϶�, 
	//	�����带 ����� �θ� �����忡�� ȣ���� �ȴ�. 
	//	������ ����� �� ������ context���� ȣ��ȴ�.
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
	dll injection ������ ���� ����ִ� �̾߱�.
	https://patents.google.com/patent/KR101097590B1/ko

	��� ȹ���� LoadLibrary(xx) �Լ� �ּҿ� ������ Ŀ�� ������Ʈ�� ���� �����Լ� �ּҸ� 
	���Ͽ� ���� ��쿡�� DLL �������� �̷����� ������ Ȯ���� �� �ִ�. 
	
	��� DLL ���������� �Ǵܵ� ��쿡 ��� ȹ���� ������ Ŀ�� ������Ʈ�� �Ķ���� �޸𸮿� 
	�����ǵ� DLL�� ��ü ��ΰ� �����ϰ� �Ǹ�, �� �޸𸮸� �ʱ�ȭ�Ѵ�. 
	
	�̷��� �Ǹ� LoadLibrary(xx) �Լ��� �ε��� DLL�� ��ΰ� �����Ƿ� 
	�ڿ������� �����ϰ� �Ǿ� �������� �̷������ �ʰ� �ȴ�.
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
	//	�� ��ƾ�� ��� ����7�� ���������� ȣ��Ǵ� �κ��� 
	//	���� ������� �����尡 �ƴ϶�, 
	//	�����带 ����� �θ� �����忡�� ȣ���� �ȴ�. 
	//	������ ����� �� ������ context���� ȣ��ȴ�.
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
		//	���⼭ �ٷ� SendMessage �ߴ��� ����� ó�� �ǹ�������.
		//SendMessage(__FUNCTION__, &Config()->client.event, &msg, sizeof(msg));
		//	KeGetCurrentThread
		//	This routine is identical to PsGetCurrentThread.
		//	�ٸ� ���ϰ��� �ٸ��� ������ ���� �ּҰ� ���� �ֵ��̶� ���ٰ� ���� ������ ��.
		//	�����Ϸ��� �����尡 �ڱⰡ �ƴ� �ٸ� ���μ����ΰ���?
		//	�׷� code injection �̶� ���ϴ�.
	}
	else {
		//	������ �־�..
		//	������ ����� ȣ��Ǵϱ� �� ������� �� ����� �༮�ε� 
		//	�̰� �񵿱�� ������ �����൵ �ۿ��� �ش� ������ ������ ���Ϸ� �� ��
		//	�� �����尡 �̹� ������� ������ �� �ִ� ������.
		//FN_NtQueryInformationThread;
		// KeQuerySystemTimePrecise

		//	����ִ� �̾߱�:
		//	thread�� start address �� LoadLibraryA, LoadLibraryW �� �ּҰ� ������ 
		//	dll injection �̶�� �Ѵ�.

		pMsg->data.subType = YFilter::Message::SubType::ThreadStop;
		//	�ش� �������� ���ҽ� ������� ���Ѵ�.
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
			//	pMsg�� SendMessage ���� �� ������ ���̴�. 
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
				//	pMsg�� SendMessage ���� �� ������ ���̴�. 
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