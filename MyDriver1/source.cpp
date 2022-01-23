#include "Functions.h"

//���� ����
#pragma warning(push)
#pragma warning(disable:4100)

NTSTATUS DriverInitialize(PDRIVER_OBJECT DriverObject) {
	NTSTATUS retStatus = STATUS_SUCCESS;
	
	UNICODE_STRING   deviceSymLink;
	UNICODE_STRING   deviceName;
	PDEVICE_OBJECT deviceObject = NULL;

	//�����ڵ� Ÿ�� ���ڿ� �ʱ�ȭ  -> IoCreateSymbolicLink �Լ� ���ڷ� �����ڵ� Ÿ�� ���ڿ� �Ѿ���� 
	RtlInitUnicodeString(&deviceSymLink, SYMLINK);
	RtlInitUnicodeString(&deviceName, L"\\Device\\testName");

	dmsg("Start Driver initialize");

	//PDEVICE_OBJECT ���� 
	retStatus = IoCreateDevice(
		DriverObject,
		0,
		&deviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&deviceObject);

	//NTSTATUS ��� SUCCESS ���� �˻� 
	if (!NT_SUCCESS(retStatus)) {
		dmsg("IoCreateDevice Fail!");
		return STATUS_FAIL_CHECK;
	}
	else 
		dmsg("CreateDevice Complete");

	//����ü�� ����̽� ������Ʈ ���� 
	g_MalGlobal.DeviceObject = deviceObject;

	//�ɺ��� ��ũ ���� 
	retStatus = IoCreateSymbolicLink(&deviceSymLink, &deviceName);

	//NTSTATUS ��� SUCCESS ���� �˻� 
	if (!NT_SUCCESS(retStatus)) {
		dmsg("IoCreateSymLink Fail!");
		return STATUS_FAIL_CHECK;
	}
	else 
		dmsg("IoCreateSymLink Complete");
	return STATUS_SUCCESS;
}

extern "C" NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath){

	NTSTATUS retStatus = STATUS_SUCCESS;
	PVOID KernelBase = 0;

	retStatus = DriverInitialize(DriverObject);
	if (!NT_SUCCESS(retStatus)) {
		dmsg("Driver Initialize Fail!");
		return retStatus;
	}
	else 
		dmsg("Driver Initialize Complete!");

	//ZwQuerySystemInformation �Լ� ������ �޾ƿ� 
	ZwQuerySystemInformation = (ZwQuerySystemInformation_t) GetFunctionPointer(L"ZwQuerySystemInformation");

	// System Module Information �޾ƿ��� Ŀ�� ���̽� �ּ� ã�� �Լ� 
	KernelBase = GetKernelBaseAddress();

	if (!KernelBase) {
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== Finding kernelBase Fail====== \n");
		return STATUS_FAIL_CHECK;
	}	

	//���� ����� LDR_DATA_TABLE_ENTRY ���� ����, �� ��Ҹ� �����ϴ� ��ũ�� ���� 
	PKLDR_DATA_TABLE_ENTRY  pThisModule = (PKLDR_DATA_TABLE_ENTRY)(DriverObject->DriverSection);

	if (!pThisModule) {
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== Finding ThisModule Fail====== \n");
		return STATUS_FAIL_CHECK;
	}

	//PsLoadedModuleList �ּ� �̾ƿ��� ����
	GetPModuleList(pThisModule,KernelBase);

	//����̹� �ڽ� ����� 
	//HidingOrderLInks(pThisModule);

	//PsLoadedModuleList ��ũ ��ȸ�ϸ� ����̹� �̸� ��� 
	PrintAllModuleList();

	DriverObject->DriverUnload = UnloadMyDriver;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = createHandler;

	return retStatus;
}