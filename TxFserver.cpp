/**
  Copyright © 2020 lorenzoinvidia. All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  
  3. The name of the author may not be used to endorse or promote products
  derived from this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY AUTHORS "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. 
*/

#include <Windows.h>
#include <winternl.h>
#include <TlHelp32.h>
#include <iostream>

using namespace std;
#define MAXBUFFSIZE 1024
#define DESC L"test"
#define PIPENAME "\\\\.\\pipe\\test"
#define TARGETNAME "TxFclient.exe"
//#define DEBUG //uncomment to get debug output 

typedef HANDLE(WINAPI *_CreateTransaction)(
	IN LPSECURITY_ATTRIBUTES lpTransactionAttributes,
	IN LPGUID                UOW,
	IN DWORD                 CreateOptions,
	IN DWORD                 IsolationLevel,
	IN DWORD                 IsolationFlags,
	IN DWORD                 Timeout,
	LPWSTR                   Description
);

/*
	Return the PID given the process name
*/
DWORD getPID(const char *procName) {
	HANDLE hProcess;
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32);
	hProcess = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	do {
		if (strcmp(pe32.szExeFile, procName) == 0) {
			CloseHandle(hProcess);
			return pe32.th32ProcessID;
		}
	} while (Process32Next(hProcess, &pe32));
	CloseHandle(hProcess);
	return 0;
}//getPID


int main(){
	std::cout << "[i] PID="<<std::dec<<GetCurrentProcessId()<<endl;

	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, getPID(TARGETNAME));
	if (!hProcess){
		cout << "[-] OpenProcess failed with err: " << GetLastError() << " Exiting." << endl;
		return EXIT_FAILURE;
	}
	cout <<"[+] " << TARGETNAME <<" found. Continue." << endl;


	/* *************************** SETUP START *************************** */
	cout << "[+] Setup started ..." << endl;

	/* Runtime loading of TxF functions */
	HMODULE hKtmW32;
	if (!(hKtmW32 = LoadLibraryA("KtmW32.dll"))) {
		cout << "[-] LoadLibrary failed" << endl;
		return EXIT_FAILURE;
	}

	_CreateTransaction CreateTransaction = NULL;
	if (!(CreateTransaction = (_CreateTransaction)GetProcAddress(hKtmW32, "CreateTransaction"))) {
		cout << "[-] GetProcAddress failed" << endl;
		return EXIT_FAILURE;
	}

	/* Create named pipe to share dup handles */
	HANDLE hPipe = CreateNamedPipeA(
		PIPENAME,								// name
		PIPE_ACCESS_DUPLEX,						// bi-directional (however, only the server is going to write here)
		PIPE_TYPE_BYTE | 						// W as bytes
		PIPE_READMODE_BYTE |					// R as bytes
		PIPE_WAIT, 								// blocking enabled
		1,										// just 1 instance for now
		MAXBUFFSIZE,							// out buff size
		MAXBUFFSIZE,							// in buff size
		NMPWAIT_USE_DEFAULT_WAIT,				// default timeout
		NULL									// default attributes
	);

	if (hPipe == INVALID_HANDLE_VALUE){
		cout << "[-] Failed creating pipe: " << GetLastError() << " Exiting." << endl;
		return EXIT_FAILURE;
	}
#ifdef DEBUG
	cout << "[+] Pipe successfully created" << endl;
#endif

	/* Create Named mutex to handle file writing */
    HANDLE hMutex = CreateMutex(
                        NULL,			// default , no inheritance
                        TRUE,			// init owned
                        "mtx"			// named mutex
                    ); 
	if (!hMutex){
		cout << "[-] Failed to create mutex. Exiting." << endl;
		return EXIT_FAILURE;
	}
#ifdef DEBUG
	cout << "[+] Mutex successfully created" << endl;
#endif

	cout << "[+] Setup completed !" << endl;

	/* *************************** SETUP END *************************** */


	/* Create NTFS Transaction */
	HANDLE hTr = CreateTransaction(
		NULL,							// No inheritance
		0,								// Reserved
		TRANSACTION_DO_NOT_PROMOTE,		// The transaction cannot be distributed
		0,								// Reserved
		0,								// Reserved
		0,								// Abort after timeout (ms), 0 = infinite
		(LPWSTR)DESC					// Description
	);

	if (hTr == INVALID_HANDLE_VALUE) {
		cout << "[-] CreateTransaction failed with err: " << GetLastError() << " Exiting." << endl;
		return EXIT_FAILURE;
	}

	/* Write file (transacted) */
	HANDLE hTrFile = CreateFileTransactedA(
		"C:\\Users\\t\\Desktop\\TrFile.txt",	// Path
		GENERIC_WRITE,							// Write data
		0,										// Do not share
		NULL,									// Default security
		CREATE_ALWAYS,							// Create if not exists
		FILE_ATTRIBUTE_NORMAL,					// Normal file
		NULL, 									// No template file
		hTr,									// Transaction handle
		NULL,									// Miniversion (?)
		NULL									// Reserved
	);

	/* Write file (standard) */
	// HANDLE hTrFile = CreateFileA(
	// 	"C:\\Users\\t\\Desktop\\TrFile.txt",	// Path
	// 	GENERIC_WRITE,							// Write data
	// 	0,										// Do not share
	// 	NULL,									// Default security
	// 	CREATE_ALWAYS,							// Create if not exists
	// 	FILE_ATTRIBUTE_NORMAL,					// Normal file
	// 	NULL 									// No template file
	// );

	if (hTrFile == INVALID_HANDLE_VALUE) {
		cout << "[-] CreateFile failed with err: " << GetLastError() << endl;
		return EXIT_FAILURE;
	}

	/*
		Wait for client(s) to connect to the pipe
		(It does not return until a client is connected or an error occurs)
	*/

	cout << "[i] Wait for client(s)" << endl;
	ConnectNamedPipe(hPipe, NULL);
	cout << "[i] Client connected to pipe" << endl;

// #ifdef DEBUG
// 	cout << "[DEBUG] About to duplicate handle(s). Type any key to continue." << endl;
// 	std::cin.get();
// #endif

	
	/* *************************** HANDLES DUP *************************** */
	HANDLE hTrDst, hTrFileDst; 			// destination handles
	char bufferPipe[MAXBUFFSIZE];		// named pipe buffer
	
	/* Duplicate transaction handle */
	DuplicateHandle(
		GetCurrentProcess(), 			// source process
        hTr, 							// source handle
		hProcess,						// dest process
        &hTrDst,						// dest handle						 
        0,								// ignored
        FALSE,							// no inheritance (for now)
        DUPLICATE_SAME_ACCESS			// dest got same access
	);
#ifdef DEBUG
	printf("[DEBUG]: hTr = %#x\n[DEBUG]: hTrDst = %#x\n",hTr,hTrDst);
#endif

	/* Duplicate file handle */
	DuplicateHandle(
		GetCurrentProcess(), 			// source process
        hTrFile, 						// source handle
		hProcess,						// dest process
        &hTrFileDst,					// dest handle						 
        0,								// ignored
        FALSE,							// no inheritance
        DUPLICATE_SAME_ACCESS			// dest got same access
	);
#ifdef DEBUG
	printf("[DEBUG]: hTrFile = %#x\n[DEBUG]: hTrFileDst = %#x\n",hTrFile,hTrFileDst);
#endif

	/* Writing handles to buffer pipe */
    sprintf(bufferPipe, "%d %d\n", hTrDst, hTrFileDst);
    short buffPipeLen = strlen(bufferPipe); // miss \x00 checking here
#ifdef DEBUG
	printf("[DEBUG]: bufferPipe=%s[DEBUG]: buffPipeLen=%d\n", bufferPipe,buffPipeLen);
#endif
	WriteFile(
		hPipe,			// file handle
		bufferPipe,		// buffer
		buffPipeLen,    // no bytes to write
		NULL,			// recv no bytes written
		NULL			// overlap
	);
	
	/* 
		Flush the pipe to allow the client to read the pipe's contents 
		before disconnecting. Then disconnect the pipe, and close the 
		handle to this pipe instance.
  	*/
	FlushFileBuffers(hPipe); 
   	DisconnectNamedPipe(hPipe); 
   	CloseHandle(hPipe); 

	cout << "[+] Sent handles to client(s)!" << endl; 

	/* *************************** HANDLES DUP END *************************** */


	/* Write PID to file */
	char bufferFile[MAXBUFFSIZE];
	sprintf(bufferFile, "%d ", GetCurrentProcessId());
    short buffFileLen = strlen(bufferFile); // miss \x00 checking here

	WriteFile(
		hTrFile,		// file handle
		bufferFile,		// buffer
		buffFileLen,	// no bytes to write
		NULL,			// recv no bytes written
		NULL			// overlap
	);

	/* Close local file handle */
	CloseHandle(hTrFile);
    cout << "[+] File successfully written" << endl;


	SleepEx(5000,TRUE); // Simulate some work (for now)
    
	/* Release mutex */
    ReleaseMutex(hMutex);

#ifdef DEBUG
    cout << "[+] Mutex released" << endl;
#endif


	/*
		The system closes the handle automatically when the process terminates 
		The mutex object is destroyed when its last handle has been closed 
	*/
    CloseHandle(hMutex);
    
    
	cout << "[i] Type any key to exit." << endl;
	std::cin.get();
    return EXIT_SUCCESS;
}
