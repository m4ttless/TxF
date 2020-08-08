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
#include <iostream>

using namespace std;
#define MAXBUFFSIZE 1024
#define PIPENAME "\\\\.\\pipe\\test"
//#define DEBUG //uncomment to get debug output 

typedef BOOL (WINAPI *_CommitTransaction)(
	IN HANDLE TransactionHandle
);

int main(){
	std::cout << "[i] PID="<<std::dec<<GetCurrentProcessId()<<endl;
	
	/* *************************** SETUP START *************************** */
	cout << "[+] Setup started ..." << endl;

	/* Runtime loading of TxF functions */
	HMODULE hKtmW32;
	if (!(hKtmW32 = LoadLibraryA("KtmW32.dll"))) {
		cout << "[-] LoadLibrary failed" << endl;
		return EXIT_FAILURE;
	}

	_CommitTransaction CommitTransaction = NULL;
	if (!(CommitTransaction = (_CommitTransaction)GetProcAddress(hKtmW32, "CommitTransaction"))) {
		cout << "[-] GetProcAddress failed" << endl;
		return EXIT_FAILURE;
	}
		cout << "[+] Setup completed !" << endl;

	/* *************************** SETUP END *************************** */

	cout << "[i] Type any key to start." << endl;
	std::cin.get();

	/* *************************** GET DUP HANDLES *************************** */

	/* Open named pipe */
    HANDLE hPipe = CreateFile(
		PIPENAME, 				// name
        GENERIC_READ, 			// R
        0,						// do not share
        NULL,					// default security
        OPEN_EXISTING,			// open if exist
        0,						// 0
        NULL					// no template file 
	);

	if (hPipe == INVALID_HANDLE_VALUE){
		cout << "[-] Failed opening pipe: " << GetLastError() << endl;
		return EXIT_FAILURE;
	}
#ifdef DEBUG
	cout << "[+] Pipe successfully opened" << endl;
#endif


	/* Get handles from buffer */ 
	char bufferPipe[MAXBUFFSIZE] = {0};
	DWORD dwRead = 0;
	while (ReadFile(hPipe, bufferPipe, MAXBUFFSIZE, &dwRead, NULL) != FALSE) {
        /* add terminating zero */
        bufferPipe[dwRead] = '\0';
#ifdef DEBUG
    	printf("[DEBUG]: bufferPipe = %s", bufferPipe);
#endif
    }//while

	// ASSUMPTION: when pipe disconnect we got the handles
#ifdef DEBUG
	cout << "[i] Pipe Disconnected" << endl;
#endif


	HANDLE 	hTr = INVALID_HANDLE_VALUE,			// transaction handle
			hTrFile = INVALID_HANDLE_VALUE;		// transacted file handle
			
	sscanf(bufferPipe, "%d %d", &hTr, &hTrFile);
	if (hTrFile == INVALID_HANDLE_VALUE || hTr == INVALID_HANDLE_VALUE ){
		cout << "[-] Failed to get handles from pipe. Exiting." << endl;
        return EXIT_FAILURE;
	}
#ifdef DEBUG
	printf("[DEBUG]: hTr = %#x\n[DEBUG]: hTrFile = %#x\n",hTr,hTrFile);
#endif

	/* Close pipe handle */
	CloseHandle(hPipe);

	cout << "[+] Got TxF handles" << endl;

	/* *************************** GET DUP HANDLES END *************************** */

    /* Open Named mutex */
    HANDLE hMutex = CreateMutexA(NULL,FALSE,"mtx");

    if (!hMutex){
        cout << GetCurrentProcessId() << "[-] Failed opening the mutex. Exiting." << endl;
        return EXIT_FAILURE;
    }

#ifdef DEBUG
    cout << "[+] Mutex opened successfully" << endl;
#endif

    /* Wait to acquire mutex */
    WaitForSingleObject(hMutex, INFINITE);

    cout << "[+] Got mutex" << endl;
	

	/* Write PID to file */
	char bufferFile[MAXBUFFSIZE];
    sprintf(bufferFile, "%d ", GetCurrentProcessId());
    short buffFileLen = strlen(bufferFile);

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

    /* Release mutex */
    ReleaseMutex(hMutex);
#ifdef DEBUG
    cout << "[+] Mutex released" << endl;
#endif


	/* Commit transaction */
	BOOL res = CommitTransaction(hTr);
	
	if (!res) {
		cout << "[-] CommitTransaction failed with err: " << GetLastError() << " Exiting." << endl;
		return EXIT_FAILURE;
	}

	/* Close transaction handle */
	CloseHandle(hTr);
	
	/*
		The system closes the handle automatically when the process terminates 
		The mutex object is destroyed when its last handle has been closed 
	*/
    CloseHandle(hMutex);

	cout << "[i] Type any key to exit." << endl;
	std::cin.get();
    return EXIT_SUCCESS;
}
