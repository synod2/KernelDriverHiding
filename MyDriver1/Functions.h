#pragma once
#include "Structures.h"

#define CONTAINING_RECORD(address, type, field) ((type *)( (PCHAR)(address) - (ULONG_PTR)(&((type *)0)->field)))

//SYMLINK ���ڿ� ���� 
#define SYMLINK L"\\DosDevices\\ioctltest"

MAL_GLOBAL g_MalGlobal;

//IRP Major ���� �Լ� ����̹� ����ġ ����
_Dispatch_type_(IRP_MJ_CREATE)
DRIVER_DISPATCH createHandler;
DRIVER_UNLOAD UnloadMyDriver;

//DBGPRINT WRAPPING �Լ�
ULONG dmsg(PCHAR msg) {
	ULONG ret;
	ret = DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "[*] =============== Dbg Message : %s ===============\n", msg);
	return ret;
};

//�Լ� �̸����� �Լ� ������ ã�ƿ��� �����Լ� 
PVOID GetFunctionPointer(PCWSTR FunctionName) {
	UNICODE_STRING UnicodeName;
	PVOID RetPointer;

	RtlInitUnicodeString(&UnicodeName,FunctionName);
	RetPointer = MmGetSystemRoutineAddress(&UnicodeName);
	return RetPointer;
}


//ZwQuerySystemInformation �Լ����� ����
typedef NTSTATUS(*NTAPI ZwQuerySystemInformation_t)(
	ULONG SystemInforationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
	);
ZwQuerySystemInformation_t(ZwQuerySystemInformation) = nullptr;

int SystemModuleInformation = 0xb;

//PsLoadedModuleList �ּ� �̾ƿ��� ����
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

//PsLoadedModuleList ��ũ ��ȸ�ϸ� ����̹� �̸� ��� 
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
	//����̹� �ڽ� ����� 
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
	// SystemModuleInformation ũ�� ��ȯ
	status = ZwQuerySystemInformation(SystemModuleInformation, 0, bytes, &bytes);

	/*if (status == STATUS_INFO_LENGTH_MISMATCH)
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== LENGTH MISMATCH ERROR ====== \n");
		return 0;
	}*/

	// ��ȯ���� ũ�⸸ŭ ���� �޸� �Ҵ�
	pMods = (PSYSTEM_MODULE_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, bytes, '1doM');
	if (pMods == 0){
		dmsg("Allocate Fail");
		return 0;
	}
	
	RtlZeroMemory(pMods, bytes);

	status = ZwQuerySystemInformation(SystemModuleInformation, pMods, bytes, &bytes);

	

	//Ŀ�κ��̽� �ּ� ã�� 
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

//Create ���� �Լ� - �Լ����� ������ STATUS_SUCESS�� ��ȯ���־�� �Ѵ�. 
NTSTATUS createHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP irp) {
	//I/O ���� ���� 
	irp->IoStatus.Status = STATUS_SUCCESS;

	//IRP ���� �Ϸ� ó�� �Լ� 
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	dmsg("CREATE EVENT!");
	return STATUS_SUCCESS;
}

//����̹� ��ε� �Լ� 
void UnloadMyDriver(PDRIVER_OBJECT DriverObject) {
	dmsg("Driver Unloaded!");
	if (g_MalGlobal.DeviceObject != nullptr)
	{
		//�ɺ��� ��ũ �� �ʱ�ȭ ���ְ� ���� �Ҵ� ����
		UNICODE_STRING Symlink = { 0, };
		RtlInitUnicodeString(&Symlink, SYMLINK);
		IoDeleteSymbolicLink(&Symlink);

		//����̽� ������Ʈ ���� 
		IoDeleteDevice(g_MalGlobal.DeviceObject);
	}
	return;
}
