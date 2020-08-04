#include <Windows.h>
#include <iostream>

using namespace std;

#define DESC L"test"

char buffer[] = "Hello";
#define SIZE sizeof(buffer)

typedef HANDLE(WINAPI *_CreateTransaction)(
	IN LPSECURITY_ATTRIBUTES lpTransactionAttributes,
	IN LPGUID                UOW,
	IN DWORD                 CreateOptions,
	IN DWORD                 IsolationLevel,
	IN DWORD                 IsolationFlags,
	IN DWORD                 Timeout,
	LPWSTR                   Description
);

typedef BOOL (WINAPI *_CommitTransaction)(
	IN HANDLE TransactionHandle
);


int main() {

	BOOL res = FALSE;

	/* 
		Runtime loading, otherwise linking errors by including ktmw32.h
	*/
	HMODULE hKtmW32;
	if (!(hKtmW32 = LoadLibraryA("KtmW32.dll"))) {
		cout << "LoadLibrary failed" << endl;
		return EXIT_FAILURE;
	}

	_CreateTransaction CreateTransaction = NULL;
	if (!(CreateTransaction = (_CreateTransaction)GetProcAddress(hKtmW32, "CreateTransaction"))) {
		cout << "GetProcAddress failed" << endl;
		return EXIT_FAILURE;
	}

	_CommitTransaction CommitTransaction = NULL;
	if (!(CommitTransaction = (_CommitTransaction)GetProcAddress(hKtmW32, "CommitTransaction"))) {
		cout << "GetProcAddress failed" << endl;
		return EXIT_FAILURE;
	}


	/* Create Transaction */
	HANDLE hTr = CreateTransaction(
		NULL,                           // No inheritance
		0,                              // Reserved
		TRANSACTION_DO_NOT_PROMOTE,     // The transaction cannot be distributed
		0,                              // Reserved
		0,                              // Reserved
		0,                              // Abort after timeout (ms), 0 = infinite
		(LPWSTR)DESC			// Description
	);

	if (hTr == INVALID_HANDLE_VALUE) {
		cout << "CreateTransaction failed with err: " << GetLastError() << endl;
		return EXIT_FAILURE;
	}


	/* Create file */
	HANDLE hTrFile = CreateFileTransactedA(
		"C:\\Users\\t\\Desktop\\TrFile.txt",    // Path
		GENERIC_READ | GENERIC_WRITE,		// R+W
		0,					// Do not share
		NULL,					// Default security
		CREATE_ALWAYS,				// Overwrite if file exists
		FILE_ATTRIBUTE_NORMAL,			// Normal file
		NULL,					// No template file
		hTr,					// Transaction handle
		NULL,					// Miniversion (?)
		NULL					// Reserved
	);

	if (hTrFile == INVALID_HANDLE_VALUE) {
		cout << "CreateFile failed with err: " << GetLastError() << endl;
		return EXIT_FAILURE;
	}

	/* File ops */
	res = WriteFile(
		hTrFile,	// file handle
		buffer,		// buffer
		SIZE,		// no bytes to write
		NULL,		// recv no bytes written
		NULL		// overlap
	);

	if (!res) {
		cout << "WriteFile failed with err: " << GetLastError() << endl;
		return EXIT_FAILURE;
	}

	/* Close file handle */
	CloseHandle(hTrFile);

	/* Commit transaction */
	res = CommitTransaction(hTr);
	
	if (!res) {
		cout << "CommitTransaction failed with err: " << GetLastError() << endl;
		return EXIT_FAILURE;
	}

	/* Close transaction handle */
	CloseHandle(hTr);


	cin.get();
	return EXIT_SUCCESS;
}
