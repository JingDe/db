/*
	不同的Unix vfs在于它们的文件锁不同。默认vfs是unix。实现的共同部分在os_unix.c文件。
	Windows上默认的vfs是win32.

	VFS Shims

	简单的Unix vfs : "demo"。

	VFS实现：实现三个类：sqlite3_vfs，sqlite3_io_methods，sqlite3_file。
	sqlite3_vfs结构体定义：vfs的名字、实现os接口的核心函数（如文件操作、随机数、日期时间等）。
	sqlite3_file结构体表示一个打开的文件。跟踪文件状态。
	sqlite3_io_methods对象包含于打开文件交互的方法。如读写、flush等等。

	实现一个新的VFS的方法：
	实现sqlite3_vfs子类，并注册。
	打开sqlite3_vfs获得sqlite3_file对象，sqlite3_file对象指向一个sqlite3_io_methods对象。
*/



typedef struct sqlite3_io_methods sqlite3_io_methods;
struct sqlite3_io_methods {
	int iVersion;
	int (*xClose)(sqlite3_file*);
	int (*xRead)(sqlite3_file*, void*, int iAmt, int64_t iOfst);
	int (*xWrite)(sqlite3_file*, const void*, int iAmt, int64_t iOfst);
	int (*xTruncate)(sqlite3_file*, int64_t size);
	int(*xSync)(sqlite3_file*, int flags);
	int(*xFileSize)(sqlite3_file*, int64_t *pSize);
	int(*xLock)(sqlite3_file*, int);
	int(*xUnlock)(sqlite3_file*, int);
	int(*xCheckReservedLock)(sqlite3_file*, int *pResOut);
	int(*xFileControl)(sqlite3_file*, int op, void *pArg);
	int(*xSectorSize)(sqlite3_file*);
	int(*xDeviceCharacteristics)(sqlite3_file*);
	/* Methods above are valid for version 1 */

	int(*xShmMap)(sqlite3_file*, int iPg, int pgsz, int, void volatile**);
	int(*xShmLock)(sqlite3_file*, int offset, int n, int flags);
	void(*xShmBarrier)(sqlite3_file*);
	int(*xShmUnmap)(sqlite3_file*, int deleteFlag);
	/* Methods above are valid for version 2 */

	int(*xFetch)(sqlite3_file*, int64_t iOfst, int iAmt, void **pp);
	int(*xUnfetch)(sqlite3_file*, int64_t iOfst, void *p);
	/* Methods above are valid for version 3 */
	/* Additional methods may be added in future releases */
};


typedef struct sqlite3_file sqlite3_file;
struct sqlite3_file {
	const struct sqlite3_io_methods *pMethods;  /* Methods for an open file */
};


struct sqlite3_vfs {
	int iVersion;            /* Structure version number (currently 3) */
	int szOsFile;            /* Size of subclassed sqlite3_file */
	int mxPathname;          /* Maximum file pathname length */
	sqlite3_vfs *pNext;      /* Next registered VFS */
	const char *zName;       /* Name of this virtual file system */
	void *pAppData;          /* Pointer to application-specific data */
	int(*xOpen)(sqlite3_vfs*, const char *zName, sqlite3_file*,
		int flags, int *pOutFlags);
	int(*xDelete)(sqlite3_vfs*, const char *zName, int syncDir);
	int(*xAccess)(sqlite3_vfs*, const char *zName, int flags, int *pResOut);
	int(*xFullPathname)(sqlite3_vfs*, const char *zName, int nOut, char *zOut);
	void *(*xDlOpen)(sqlite3_vfs*, const char *zFilename);
	void(*xDlError)(sqlite3_vfs*, int nByte, char *zErrMsg);
	void(*(*xDlSym)(sqlite3_vfs*, void*, const char *zSymbol))(void);
	void(*xDlClose)(sqlite3_vfs*, void*);
	int(*xRandomness)(sqlite3_vfs*, int nByte, char *zOut);
	int(*xSleep)(sqlite3_vfs*, int microseconds);
	int(*xCurrentTime)(sqlite3_vfs*, double*);
	int(*xGetLastError)(sqlite3_vfs*, int, char *);
	/*
	** The methods above are in version 1 of the sqlite_vfs object
	** definition.  Those that follow are added in version 2 or later
	*/
	int(*xCurrentTimeInt64)(sqlite3_vfs*, int64_t*);
	/*
	** The methods above are in versions 1 and 2 of the sqlite_vfs object.
	** Those below are for version 3 and greater.
	*/
	//int(*xSetSystemCall)(sqlite3_vfs*, const char *zName, sqlite3_syscall_ptr);
	//sqlite3_syscall_ptr (*xGetSystemCall)(sqlite3_vfs*, const char *zName);
	const char *(*xNextSystemCall)(sqlite3_vfs*, const char *zName);
	/*
	** The methods above are in versions 1 through 3 of the sqlite_vfs object.
	** New fields may be appended in future versions.  The iVersion
	** value will increment whenever this happens.
	*/
};