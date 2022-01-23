#include "Functions.h"

//오류 무시
#pragma warning(push)
#pragma warning(disable:4100)

NTSTATUS DriverInitialize(PDRIVER_OBJECT DriverObject) {
	NTSTATUS retStatus = STATUS_SUCCESS;
	
	UNICODE_STRING   deviceSymLink;
	UNICODE_STRING   deviceName;
	PDEVICE_OBJECT deviceObject = NULL;

	//유니코드 타입 문자열 초기화  -> IoCreateSymbolicLink 함수 인자로 유니코드 타입 문자열 넘어가야함 
	RtlInitUnicodeString(&deviceSymLink, SYMLINK);
	RtlInitUnicodeString(&deviceName, L"\\Device\\testName");

	dmsg("Start Driver initialize");

	//PDEVICE_OBJECT 생성 
	retStatus = IoCreateDevice(
		DriverObject,
		0,
		&deviceName,
		FILE_DEVICE_UNKNOWN,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&deviceObject);

	//NTSTATUS 결과 SUCCESS 인지 검사 
	if (!NT_SUCCESS(retStatus)) {
		dmsg("IoCreateDevice Fail!");
		return STATUS_FAIL_CHECK;
	}
	else 
		dmsg("CreateDevice Complete");

	//구조체에 디바이스 오브젝트 저장 
	g_MalGlobal.DeviceObject = deviceObject;

	//심볼릭 링크 생성 
	retStatus = IoCreateSymbolicLink(&deviceSymLink, &deviceName);

	//NTSTATUS 결과 SUCCESS 인지 검사 
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

	//ZwQuerySystemInformation 함수 포인터 받아옴 
	ZwQuerySystemInformation = (ZwQuerySystemInformation_t) GetFunctionPointer(L"ZwQuerySystemInformation");

	// System Module Information 받아오고 커널 베이스 주소 찾는 함수 
	KernelBase = GetKernelBaseAddress();

	if (!KernelBase) {
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== Finding kernelBase Fail====== \n");
		return STATUS_FAIL_CHECK;
	}	

	//현재 모듈의 LDR_DATA_TABLE_ENTRY 구조 참조, 각 요소를 연결하는 링크에 접근 
	PKLDR_DATA_TABLE_ENTRY  pThisModule = (PKLDR_DATA_TABLE_ENTRY)(DriverObject->DriverSection);

	if (!pThisModule) {
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "===== Finding ThisModule Fail====== \n");
		return STATUS_FAIL_CHECK;
	}

	//PsLoadedModuleList 주소 뽑아오는 동작
	GetPModuleList(pThisModule,KernelBase);

	//드라이버 자신 숨기기 
	//HidingOrderLInks(pThisModule);

	//PsLoadedModuleList 링크 순회하며 드라이버 이름 출력 
	PrintAllModuleList();

	DriverObject->DriverUnload = UnloadMyDriver;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = createHandler;

	return retStatus;
}