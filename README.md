# TxF
PoCs about Transactional NTFS

## TL;DR

Transactional NTFS (TxF) introduces atomicity in file operations on an NTFS file system volume. These run within transactions, protecting data integrity and rollbacking the operations across any failure.

TxF binds a file handle to a transaction: in this way, API function working on handles like ReadFile or WriteFile run without any change.
However, APIs expecting file names have their counterpart, e.g.

[CreateFile](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea) -> [CreateFileTransacted](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfiletransacteda)

[CreateDirectory](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectorya) -> [CreateDirectoryTransacted](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createdirectorytransacteda)

TxF provides isolation. A file or directory created within a transaction is not visible to anything outside the current transaction. Likewise, file updates are not seen outside the transaction, even from AVs.  

After a file is locked by a transaction, other file system operations external to the locking transaction that try to modify the transactionally locked file will fail with either ERROR_SHARING_VIOLATION or ERROR_TRANSACTIONAL_CONFLICT.

Moreover, any attempt to create a file with the same name fails with the error ERROR_TRANSACTIONAL_CONFLICT, effectively reserving the file name for when the transaction commits or is rolled back.


## Getting started

1. Create a transaction by calling [CreateTransaction](https://docs.microsoft.com/en-us/windows/win32/api/ktmw32/nf-ktmw32-createtransaction)
```cpp
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
```

2. Get transacted file handle(s) by calling [CreateFileTransacted](https://docs.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfiletransacteda)
```cpp
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
```

3. Modify the file(s) as necessary e.g. with [WriteFile](https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile)
4. Close all transacted file handles associated with the transaction
```cpp
CloseHandle(hTrFile);
```
5. Commit or abort the transaction
```cpp
CommitTransaction(hTr);
```
6. Close transaction handle
```cpp
CloseHandle(hTr);
```


Ref. https://docs.microsoft.com/en-us/windows/win32/fileio/transactional-ntfs-portal
