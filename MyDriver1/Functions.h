#pragma once
#include "Structures.h"

#define CONTAINING_RECORD(address, type, field) ((type *)( (PCHAR)(address) - (ULONG_PTR)(&((type *)0)->field)))

//SYMLINK 문자열 정의 
#define SYMLINK L"\\DosDevices\\ioctltest"

MAL_GLOBAL g_MalGlobal;

//IRP Major 대응 함수 드라이버 디스패치 정의
_Dispatch_type_(IRP_MJ_CREATE)
DRIVER_DISPATCH createHandler;
DRIVER_UNLOAD UnloadMyDriver;

//DBGPRINT WRAPPING 함수
ULONG dmsg(PCHAR msg) {
	ULONG ret;
	ret = DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "[*] =============== Dbg Message : %s ===============\n", msg);
	return ret;
};

//함수 이름으로 함수 포인터 찾아오는 래핑함수 
PVOID GetFunctionPointer(PCWSTR FunctionName) {
	UNICODE_STRING UnicodeName;
	PVOID RetPointer;

	RtlInitUnicodeString(&UnicodeName,FunctionName);
	RetPointer = MmGetSystemRoutineAddress(&UnicodeName);
	return RetPointer;
}


//ZwQuerySystemInformation 함수원형 선언
typedef NTSTATUS(*NTAPI ZwQuerySystemInformation_t)(
	ULONG SystemInforationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
	);
ZwQuerySystemInformation_t(ZwQuerySystemInformation) = nullptr;

int SystemModuleInformation = 0xb;

//PsLoadedModuleList 주소 뽑아오는 동작
PVOID GetPModuleList(PKLDR_DATA_TABLE_ENTRY pThisModule,PVOID KernelBase) {
	for (PLIST_ENTRY pListEntry = pThisModule->InLoadOrderLinks.Flink; pListEntry != &pThisModule->InLoadOrderLinks; pListEntry = pListEntry->Flink) {

		PKLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(pListEntry, KLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
		if (KernelBase == pEntry->DllBase) {
			if ((PVOID)pListEntry->Blink >= pEntry->DllBase && (PUCHAR)pListEntry->Blink < (PUCHAR)pEntry->DllBase + pEntry->SizeOfImage)
			{
				PsLoadedModuleList = pListEntry->Blink;
				DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== PsLoadedModuleList : %0llx ====== \n", PsLoadedModuleList);
				break;
			}
		}
	}
	return 0;
}

//PsLoadedModuleList 링크 순회하며 드라이버 이름 출력 
PVOID PrintAllModuleList() {
	if (PsLoadedModuleList) {
		for (PLIST_ENTRY pListEntry = PsLoadedModuleList->Flink; pListEntry->Flink != 0; pListEntry = pListEntry->Flink) {
			PKLDR_DATA_TABLE_ENTRY pEntry = CONTAINING_RECORD(pListEntry, KLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
			if (PsLoadedModuleList == pListEntry)
				break;
			else
				DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== DriverName - %ws ====== \n", pEntry->BaseDllName.Buffer);
		}
	}
	else
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== PsLoadedModuleList Error ====== \n");	
	return 0;
}

PVOID HidingOrderLInks(PKLDR_DATA_TABLE_ENTRY pThisModule) {
	//드라이버 자신 숨기기 
	PLIST_ENTRY BackModule = pThisModule->InLoadOrderLinks.Blink;
	PLIST_ENTRY FrontModule = pThisModule->InLoadOrderLinks.Flink;

	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== BackModule : %ws ====== \n", BackModule);
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== FrontModule :  %ws ====== \n", FrontModule);


	BackModule->Flink = pThisModule->InLoadOrderLinks.Flink;
	FrontModule->Blink = pThisModule->InLoadOrderLinks.Blink;
	return 0;
}

PVOID GetKernelBaseAddress() {
	NTSTATUS status = STATUS_SUCCESS;
	ULONG bytes = 0;
	PSYSTEM_MODULE_INFORMATION pMods = NULL;
	PVOID KernelBase = 0;
	// SystemModuleInformation 크기 반환
	status = ZwQuerySystemInformation(SystemModuleInformation, 0, bytes, &bytes);

	/*if (status == STATUS_INFO_LENGTH_MISMATCH)
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== LENGTH MISMATCH ERROR ====== \n");
		return 0;
	}*/

	// 반환받은 크기만큼 동적 메모리 할당
	pMods = (PSYSTEM_MODULE_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, bytes, '1doM');
	if (pMods == 0){
		dmsg("Allocate Fail");
		return 0;
	}
	
	RtlZeroMemory(pMods, bytes);

	status = ZwQuerySystemInformation(SystemModuleInformation, pMods, bytes, &bytes);

	

	//커널베이스 주소 찾기 
	if (status == STATUS_SUCCESS) {
		if (pMods) {
			PSYSTEM_MODULE_ENTRY pMod = pMods->Modules;

			if (pMods->Modules) {
				KernelBase = pMod[0].ImageBase;
				DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== KernelBase : %0llx ====== \n", KernelBase);
			}
		}
	}
	ExFreePoolWithTag(pMods, '1doM');
	return KernelBase;
}

//Create 대응 함수 - 함수동작 성공시 STATUS_SUCESS를 반환해주어야 한다. 
NTSTATUS createHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP irp) {
	//I/O 상태 변경 
	irp->IoStatus.Status = STATUS_SUCCESS;

	//IRP 동작 완료 처리 함수 
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	dmsg("CREATE EVENT!");
	return STATUS_SUCCESS;
}

//드라이버 언로드 함수 
void UnloadMyDriver(PDRIVER_OBJECT DriverObject) {
	dmsg("Driver Unloaded!");
	if (g_MalGlobal.DeviceObject != nullptr)
	{
		//심볼릭 링크 값 초기화 해주고 변수 할당 해제
		UNICODE_STRING Symlink = { 0, };
		RtlInitUnicodeString(&Symlink, SYMLINK);
		IoDeleteSymbolicLink(&Symlink);

		//디바이스 오브젝트 삭제 
		IoDeleteDevice(g_MalGlobal.DeviceObject);
	}
	return;
}
