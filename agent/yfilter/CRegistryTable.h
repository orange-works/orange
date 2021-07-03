#pragma once
#include <stdint.h>

#define TAG_OBJECT			'tjbo'

typedef	uint64_t		REGUID;

typedef struct REGISTRY_ENTRY
{
	REGUID				RegPUID;
	REGUID				RegUID;
	HANDLE				PID;
	PROCUID				PUID;
	HANDLE				TID;
	ULONG				nCount;
	REG_NOTIFY_CLASS	nClass;
	UNICODE_STRING		RegPath;
	UNICODE_STRING		RegValueName;
	LARGE_INTEGER		times[2];
	DWORD64				dwTicks;
	DWORD64				dwSize;
	PVOID				pContext;
} REG_ENTRY, * PREG_ENTRY;
typedef struct {
	PVOID				pArgument2;	
	HANDLE				PID;
	HANDLE				TID;
	PROCUID				PUID;
	REGUID				RegUID;				//	key + value의 조합
	REGUID				RegPUID;			//	pid + key + value의 조합
	PUNICODE_STRING		pRegPath;
	PUNICODE_STRING		pRegValueName;
	PVOID				pArgument;
	REG_NOTIFY_CLASS	notifyClass;
	NTSTATUS			status;
	ULONG				nSize;
	YFilter::Message::SubType	subType;
	PVOID				pContext;
}	REG_CALLBACK_ARG, *PREG_CALLBACK_ARG;
typedef void (*PRegTableCallback)(

	IN PREG_ENTRY			pEntry,			//	대상 엔트리 
	IN PVOID				pContext
);

void		CreateRegistryMessage(
	PREG_ENTRY			p,
	PY_REGISTRY_MESSAGE	*pOut
);
PROCUID		Path2CRC64(PUNICODE_STRING pValue);
PROCUID		Path2CRC64(PCWSTR pValue);

class CRegTable
{
public:
	bool	IsInitialized()
	{
		return m_bInitialize;
	}
	void	Initialize(IN bool bSupportDIRQL = true)
	{
		__log(__FUNCTION__);
		RtlZeroMemory(this, sizeof(CProcessTable));
		m_bSupportDIRQL = bSupportDIRQL;
		m_pooltype		= bSupportDIRQL ? NonPagedPoolNx : PagedPool;
		ExInitializeFastMutex(&m_mutex);
		KeInitializeSpinLock(&m_lock);
		RtlInitializeGenericTable(&m_table, Compare, Alloc, Free, this);

		m_thread.Create(1, Thread, this);
		m_bInitialize = true;
	}
	static	REGUID		GetRegistryPUID(HANDLE PID, REG_NOTIFY_CLASS nClass, PUNICODE_STRING pRegPath, PUNICODE_STRING pValueName) {
		CWSTRBuffer	buf;

		if( NULL == pValueName)
			RtlStringCbPrintfW((PWSTR)buf, buf.CbSize(), L"%d.%d.%wZ", PID, nClass, pRegPath);
		else 
			RtlStringCbPrintfW((PWSTR)buf, buf.CbSize(), L"%d.%d.%wZ.%wZ", PID, nClass, pRegPath, pValueName);
		REGUID	uid	= Path2CRC64((PCWSTR)buf);
		//__log("%-32s %p %ws", __func__, uid, (PCWSTR)buf);
		return uid;
	}
	static	REGUID		GetRegistryUID(PUNICODE_STRING pRegPath, PUNICODE_STRING pValueName) {
		CWSTRBuffer	buf;

		if( pRegPath && pValueName )
			RtlStringCbPrintfW((PWSTR)buf, buf.CbSize(), L"%wZ.%wZ", pRegPath, pValueName);
		else if( pRegPath && NULL == pValueName)
			RtlStringCbPrintfW((PWSTR)buf, buf.CbSize(), L"%wZ", pRegPath);
		return Path2CRC64(buf);
	}
	static	YFilter::Message::SubType	RegClass2SubType(REG_NOTIFY_CLASS nClass)
	{
		switch( nClass ) {
		case RegNtPostQueryValueKey:
		case RegNtPreQueryValueKey:
			return YFilter::Message::SubType::RegistryGetValue;
		case RegNtPreSetValueKey:
		case RegNtPostSetValueKey:
			return YFilter::Message::SubType::RegistrySetValue;
		case RegNtDeleteValueKey:
			return YFilter::Message::SubType::RegistryDeleteValue;

		default:
			__log("%-32s unknown class:%d", __func__, nClass);


		}
		return YFilter::Message::SubType::RegistryUnknown;
	}
	ULONG	Flush(HANDLE PID)
	{
		struct FLUSH_INFO
		{
			CRegTable	*pClass;
			HANDLE		PID;
			ULONG		nTicks;
			ULONG		nCount;
		} info;

		info.pClass		= this;
		info.PID		= PID;
		KeQueryTickCount(&info.nTicks);
		info.nCount		= 0;

		Flush(&info, [](PREG_ENTRY pEntry, PVOID pContext, bool *pDelete) {
			struct FLUSH_INFO	*p	= (struct FLUSH_INFO *)pContext;
			bool	bDelete	= false;

			if( p->PID ) {
				if( p->PID == pEntry->PID ) {
					bDelete	= true;				
				}			
			}
			else {
				if( ProcessTable()->IsExisting(pEntry->PID, NULL, NULL)) {
					if( (p->nTicks - pEntry->dwTicks) > 3000 ) {
						//__dlog("%d %p %d %I64d %d", 
						//	bDelete, pEntry->RegPUID, pEntry->nCount, pEntry->dwSize, 
						//	(DWORD)(p->nTicks - pEntry->dwTicks));
						bDelete	= true;
					}
				}
				else {
					bDelete	= true;
				}			
			}
			if( bDelete ) {
				p->nCount++;
				PY_REGISTRY_MESSAGE		pReg	= NULL;
				CreateRegistryMessage(pEntry, &pReg);
				if( pReg ) {
					if (MessageThreadPool()->Push(__FUNCTION__,
						pReg->mode,
						pReg->category,
						pReg, pReg->wDataSize + pReg->wStringSize, false))
					{
						
					}
					else {
						CMemory::Free(pReg);
					}
				}
			}
			*pDelete	= bDelete;
		});
		if( info.nCount )
			MessageThreadPool()->Alert(YFilter::Message::Category::Registry);
		return info.nCount;
	}
	static	void	Thread(PVOID pContext) {
		CRegTable	*pClass	= (CRegTable *)pContext;
		pClass->Flush(NULL);
	}
	void	Destroy()
	{
		__log(__FUNCTION__);
		if (m_bInitialize) {
			m_thread.Destroy();
			Clear();
			m_bInitialize = false;
		}
	}
	void		Update(PREG_ENTRY pEntry, DWORD dwSize) {
		pEntry->nCount++;
		KeQueryTickCount(&pEntry->dwTicks);
		pEntry->dwSize	+= dwSize;
	
	}
	bool		Add
	(
		PREG_CALLBACK_ARG		p,
		OUT ULONG				&nCount
	)
	{
		if (false == IsPossible())	return false;
		BOOLEAN			bRet = false;
		REG_ENTRY		entry;
		PREG_ENTRY		pEntry = NULL;
		KIRQL			irql = KeGetCurrentIrql();

		if (irql >= DISPATCH_LEVEL) {
			__dlog("%s DISPATCH_LEVEL", __FUNCTION__);
			return false;
		}
		RtlZeroMemory(&entry, sizeof(REG_ENTRY));

		entry.RegUID	= GetRegistryUID(p->pRegPath, p->pRegValueName);
		entry.RegPUID	= GetRegistryPUID(p->PID, p->notifyClass, p->pRegPath, p->pRegValueName);

		entry.PID		= p->PID;
		entry.TID		= p->TID;
		entry.PUID		= p->PUID;
		entry.nClass	= p->notifyClass;
		entry.nCount	= 0;
		entry.times[0];
		entry.dwSize	= p->nSize;
		entry.pContext	= this;
		
		KeQuerySystemTime(&entry.times[0]);
		KeQueryTickCount(&entry.dwTicks);
		//	TIME_FIELDS
		//RtlTimeToTimeFields;
		__try 
		{
			Lock(&irql);
			//ULONG	nTableCount = RtlNumberGenericTableElements(&m_table);
			//__dlog("RegistryTable:%d", nTableCount);

			if( IsExisting(entry.RegPUID, false, &entry, [](PREG_ENTRY pEntry, PVOID pContext) {
				PREG_ENTRY	p	= (PREG_ENTRY)pContext;
				CRegTable	*pClass	= (CRegTable *)p->pContext;
				pClass->Update(pEntry, (DWORD)p->dwSize);
			
			})) {
			
			
			}
			else {
				if (p->pRegPath)
				{
					if (false == CMemory::AllocateUnicodeString(PoolType(), &entry.RegPath, p->pRegPath, 'PDDA')) {
						__dlog("CMemory::AllocateUnicodeString() failed.");
						__leave;
					}
				}
				if (p->pRegValueName)
				{
					if (false == CMemory::AllocateUnicodeString(PoolType(), &entry.RegValueName, p->pRegValueName, 'PDDA')) {
						__dlog("CMemory::AllocateUnicodeString() failed.");
						__leave;
					}
				}
				pEntry = (PREG_ENTRY)RtlInsertElementGenericTable(&m_table, &entry, sizeof(REG_ENTRY), &bRet);
				//	RtlInsertElementGenericTable returns a pointer to the newly inserted element's associated data, 
				//	or it returns a pointer to the existing element's data if a matching element already exists in the generic table.
				//	If no matching element is found, but the new element cannot be inserted
				//	(for example, because the AllocateRoutine fails), RtlInsertElementGenericTable returns NULL.
				//	if( bRet && pEntry ) PrintProcessEntry(__FUNCTION__, pEntry);
				pEntry->nCount++;
				if( false == bRet && pEntry ) {
					Update(pEntry, p->nSize);			
					nCount	= pEntry->nCount;
				}
			}
			Unlock(irql);
		}
		__finally
		{
			if (false == bRet)
			{
				if (pEntry)
				{
					//	이미 존재하고 있는 것이다. 
					//__dlog("  %p %10d already existing", pEntry, h);
				}
				else
				{
					//	존재하지 않은 건에 대한 입력 실패
				}
				//__dlog("%10d failed. IRQL=%d", (int)h, KeGetCurrentIrql());
				FreeEntryData(&entry, false);
			}
		}
		return bRet;
	}

	ULONG		Count()
	{
		if (false == IsPossible())	return 0;
		KIRQL			irql = KeGetCurrentIrql();

		if (irql >= DISPATCH_LEVEL) {
			__dlog("%s DISPATCH_LEVEL", __FUNCTION__);
			return 0;
		}
		ULONG	nCount = 0;
		Lock(&irql);
		nCount = RtlNumberGenericTableElements(&m_table);
		Unlock(irql);
		
		return nCount;
	}
	bool		IsExisting(
		IN	REGUID					RegPUID,
		IN	bool					bLock,
		IN	PVOID					pCallbackPtr = NULL,
		IN	PRegTableCallback		pCallback = NULL
	)
	{
		if (false == IsPossible())
		{
			__dlog("%s IsPossible() = false", __FUNCTION__);
			return false;
		}
		bool			bRet	= false;
		KIRQL			irql	= KeGetCurrentIrql();
		REG_ENTRY		entry;
		entry.RegPUID	= RegPUID;

		if( bLock )	Lock(&irql);
		LPVOID			p = RtlLookupElementGenericTable(&m_table, &entry);
		if (p)
		{
			if (pCallback) {
				pCallback((PREG_ENTRY)p, pCallbackPtr);
			}
			else
			{
				__log("%s pCallback is NULL.", __FUNCTION__);
			}
			bRet = true;
		}
		else
		{
			__log("%-32s %I64d not found.", __FUNCTION__, RegPUID);
		}
		if( bLock )	Unlock(irql);
		return bRet;
	}
	bool		Remove(
		IN REGUID			RegPUID,
		IN bool				bLock,
		IN PVOID			pCallbackPtr,
		PRegTableCallback	pCallback
	)
	{
		if (false == IsPossible())
		{
			return false;
		}
		bool			bRet	= false;
		REG_ENTRY		entry	= { 0, };
		KIRQL			irql	= KeGetCurrentIrql();

		entry.RegPUID	= RegPUID;
		if (bLock)	Lock(&irql);
		PREG_ENTRY	pEntry = (PREG_ENTRY)RtlLookupElementGenericTable(&m_table, &entry);
		if (pEntry)
		{
			if (pCallback)	pCallback(pEntry, pCallbackPtr);
			FreeEntryData(pEntry);
			if (RtlDeleteElementGenericTable(&m_table, pEntry))
			{
				bRet = true;
			}
			else
			{
				//__dlog("%10d can not delete.");
			}
		}
		else
		{
			//__dlog("%10d not existing");
			if (pCallback) pCallback(&entry, pCallbackPtr);
		}
		if (bLock)	Unlock(irql);
		return bRet;
	}
	void		FreeEntryData(PREG_ENTRY pEntry, bool bLog = false)
	{
		if (bLog)	__function_log;
		if (NULL == pEntry)	return;
		if( bLog )	__dlog("%-32s RegPath", __func__);
		CMemory::FreeUnicodeString(&pEntry->RegPath, bLog);
		if( bLog )	__dlog("%-32s RegValueName", __func__);
		CMemory::FreeUnicodeString(&pEntry->RegValueName, bLog);
	}
	void		Flush(PVOID pContext, void (*pCallback)(PREG_ENTRY pEntry, PVOID pContext, bool *pDelete))
	{
		//__dlog("%s IRQL=%d", __FUNCTION__, KeGetCurrentIrql());
		if (false == IsPossible() || false == m_bInitialize)
		{
			return;
		}
		KIRQL		irql;
		PREG_ENTRY	pEntry = NULL;
		ULONG		nCount	= 0;
		Lock(&irql);
		for (ULONG i = 0; i < RtlNumberGenericTableElements(&m_table); )
		{
			pEntry	= (PREG_ENTRY)RtlGetElementGenericTable(&m_table, i);
			if( NULL == pEntry )	break;

			bool	bDelete;
			pCallback(pEntry, pContext, &bDelete);
			if( bDelete ) {
				FreeEntryData(pEntry, false);
				RtlDeleteElementGenericTable(&m_table, pEntry);
				nCount++;
				//__dlog("%-32s %p deleted", __func__, pEntry->RegPUID);
			}
			else {
				i++;
			}
		}
		nCount	= RtlNumberGenericTableElements(&m_table);
		if( nCount > 100 )
			__dlog("%-32s T:%d D:%d", __func__, RtlNumberGenericTableElements(&m_table), nCount);
		Unlock(irql);
	}
	void		Clear()
	{
		//__dlog("%s IRQL=%d", __FUNCTION__, KeGetCurrentIrql());
		if (false == IsPossible() || false == m_bInitialize)
		{
			return;
		}
		KIRQL		irql;
		PREG_ENTRY	pEntry = NULL;
		Lock(&irql);
		while (!RtlIsGenericTableEmpty(&m_table))
		{
			pEntry = (PREG_ENTRY)RtlGetElementGenericTable(&m_table, 0);
			if (pEntry)
			{
				//__dlog("%s %d", __FUNCTION__, pEntry->handle);
				//__dlog("  path    %p(%d)", pEntry->path.Buffer, pEntry->path.Length);
				//__dlog("  command %p(%d)", pEntry->command.Buffer, pEntry->command.Length);
				FreeEntryData(pEntry, false);
				RtlDeleteElementGenericTable(&m_table, pEntry);
			}
		}
		Unlock(irql);
	}
	void		Lock(OUT KIRQL* pOldIrql)
	{
		/*
			KeAcquireSpinLock first resets the IRQL to DISPATCH_LEVEL and then acquires the lock.
			The previous IRQL is written to OldIrql after the lock is acquired.

			The code within a critical region guarded by an spin lock must neither be pageable nor make any references to pageable data.
			The code within a critical region guarded by a spin lock can neither call any external function
			that might access pageable data or raise an exception, nor can it generate any exceptions.
			The caller should release the spin lock with KeReleaseSpinLock as quickly as possible.
			https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-keacquirespinlock
		*/
		if (false == m_bInitialize)	return;
		if (m_bSupportDIRQL)	KeAcquireSpinLock(&m_lock, pOldIrql);
		else
			ExAcquireFastMutex(&m_mutex);
	}
	void		Unlock(IN KIRQL oldIrql)
	{
		if (false == m_bInitialize)	return;
		if (m_bSupportDIRQL)	KeReleaseSpinLock(&m_lock, oldIrql);
		else
			ExReleaseFastMutex(&m_mutex);
	}
private:
	bool				m_bInitialize;
	bool				m_bSupportDIRQL;
	RTL_GENERIC_TABLE	m_table;
	FAST_MUTEX			m_mutex;
	KSPIN_LOCK			m_lock;
	POOL_TYPE			m_pooltype;
	CThread				m_thread;

	POOL_TYPE	PoolType()
	{
		//__dlog("%-32s %d", __func__, m_pooltype);
		return m_pooltype;
	}
	bool	IsPossible()
	{
		if (false == m_bInitialize)	return false;
		if (false == m_bSupportDIRQL &&
			KeGetCurrentIrql() >= DISPATCH_LEVEL) {
			DbgPrintEx(0, 0, "%s return false.\n", __FUNCTION__);
			return false;
		}
		return true;
	}
	static	RTL_GENERIC_COMPARE_RESULTS	NTAPI	Compare(
		struct _RTL_GENERIC_TABLE* Table,
		PVOID FirstStruct,
		PVOID SecondStruct
	)
	{
		UNREFERENCED_PARAMETER(Table);
		//CHandleTable *	pClass = (CHandleTable *)Table->TableContext;
		PREG_ENTRY	pFirst	= (PREG_ENTRY)FirstStruct;
		PREG_ENTRY	pSecond = (PREG_ENTRY)SecondStruct;
		if (pFirst && pSecond)
		{
			if (pFirst->RegPUID > pSecond->RegPUID ) {
				return GenericGreaterThan;
			}
			else if (pFirst->RegPUID < pSecond->RegPUID) {
				return GenericLessThan;
			}
		}
		return GenericEqual;
	}
	static	PVOID	NTAPI	Alloc(
		struct _RTL_GENERIC_TABLE* Table,
		__in CLONG  nBytes
	)
	{
		UNREFERENCED_PARAMETER(Table);
		CRegTable* pClass = (CRegTable*)Table->TableContext;
		PVOID	p = CMemory::Allocate(pClass->PoolType(), nBytes, 'AIDL');
		return p;
	}
	static	PVOID	NTAPI	AllocInNoDispatchLevel(
		struct _RTL_GENERIC_TABLE* Table,
		__in CLONG  nBytes
	)
	{
		UNREFERENCED_PARAMETER(Table);
		return CMemory::Allocate(PagedPool, nBytes, 'AIDL');
	}
	static	VOID	NTAPI	Free(
		struct _RTL_GENERIC_TABLE* Table,
		__in PVOID  Buffer
	)
	{
		UNREFERENCED_PARAMETER(Table);
		CMemory::Free(Buffer);
	}
};

EXTERN_C_START
CRegTable*		RegistryTable();
void			CreateRegistryTable();
void			DestroyRegistryTable();
EXTERN_C_END