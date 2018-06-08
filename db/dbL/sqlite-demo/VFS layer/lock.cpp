/*
	文件锁的种类：
	**   1. POSIX locking (the default),
	**   2. No locking,
	**   3. Dot-file locking,
	**   4. flock() locking,
	**   5. AFP locking (OSX only),
	**   6. Named POSIX semaphores (VXWorks only),
	**   7. proxy locking. (OSX only)


	Posix Advisory Locking
	一个文件有硬链接或者符号链接，使用不同的文件名打开两个fd。
	同一个进程对两个fd先后加锁，后一个锁覆盖前一个锁。
	所以，只能用于不同进程的线程之间同步文件的访问，不能用于同一个进程的线程之间。

	sqlite内部处理：寻找数据库文件的inode，检查是否已存在锁。


*/
#include"sqlite3.h"
#include"vfs.h"

#include<stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include<pthread.h>
#include<string.h>

// 共享内存结构
typedef struct unixShmNode unixShmNode;       /* Shared memory instance */
struct unixShmNode {
	unixInodeInfo *pInode;     /* unixInodeInfo that owns this SHM node */
	pthread_mutex_t *mutex;      /* Mutex to access this object */
	char *zFilename;           /* Name of the mmapped file */
	int h;                     /* Open file descriptor */
	int szRegion;              /* Size of shared-memory regions */
	uint16_t nRegion;               /* Size of array apRegion */
	uint8_t isReadonly;             /* True if read-only */
	uint8_t isUnlocked;             /* True if no DMS lock held */
	char **apRegion;           /* Array of mapped shared-memory regions */
	int nRef;                  /* Number of unixShm objects pointing to this */
	unixShm *pFirst;           /* All unixShm objects pointing to this */
#ifdef SQLITE_DEBUG
	uint8_t exclMask;               /* Mask of exclusive locks held */
	uint8_t sharedMask;             /* Mask of shared locks held */
	uint8_t nextShmId;              /* Next available unixShm.id value */
#endif
};

// 打开的共享内存连接的状态
struct unixShm {
	unixShmNode *pShmNode;     /* The underlying unixShmNode object */
	unixShm *pNext;            /* Next unixShm with the same unixShmNode */
	uint8_t hasMutex;               /* True if holding the unixShmNode mutex */
	uint8_t id;                     /* Id of this connection within its unixShmNode */
	uint16_t sharedMask;            /* Mask of shared locks held */
	uint16_t exclMask;              /* Mask of exclusive locks held */
};

// iNode结构的ID
//用来定位unixInodeInfo对象的key
struct unixFileId {
	dev_t dev; //设备号

#if OS_VXWORKS
	struct vxworksFileId *pId;
#else
	uint64_t ino; // inode序号
#endif
};

// 文件handle关闭、但是fd没有关闭的文件fd链表
// 等待关闭或者重用
struct UnixUnusedFd {
	int fd;
	int flags;
	UnixUnusedFd *pNext;
};

// iNode结构
struct unixInodeInfo {
	struct unixFileId fileId; // 查找的key
	int nShared; //共享锁的个数
	unsigned char eFileLock; // SHARED_LOCK, RESERVED_LOCK等 
	unsigned char bProcessLock; // 独占的进程锁
	int nRef; // 引用计数
	unixShmNode *pShmNode; // 与此inode相关的共享内存
	int nLock;
	UnixUnusedFd *pUnused; // 
	unixInodeInfo *pNext;           /* List of all unixInodeInfo objects */
	unixInodeInfo *pPrev;           /*    .... doubly linked */
#if SQLITE_ENABLE_LOCKING_STYLE
	unsigned long long sharedByte;
#endif
#if OS_VXWORKS
	sem_t* pSem;
	char aSemName[MAX_PATHNAME + 2];
#endif
};

// unix VFS使用的sqlite3_file子类
typedef struct unixFile unixFile;
struct unixFile {
	sqlite3_io_methods const *pMethod;
	sqlite3_vfs *pVfs;
	unixInodeInfo *pInode;

};

static unixInodeInfo *inodeList = 0;
static unsigned int nUnusedFd = 0;

#define unixLogError(a, b, c) unixLogErrorAtLine(a, b, c, __LINE__)
static int unixLogErrorAtLine(int errcode, const char* zFunc, const char* zPath, int iLine)
{
	char *zErr;
	int iErrno = errno;

#if SQLITE_THREADSAFE  && defined(HAVE_STRERROR_R)
	char aErr[80];
	memset(aErr, 0, sizeof(aErr));
	zErr = aErr;
	#if defined(STRERROR_R_CHAR_P)  ||  defined(__USE_GNU)
		zErr =
	#endif
		strerror_r(iErrno, aErr, sizeof(aErr) - 1);
#elif SQLITE_THREADSAFE
	zErr = "";
#else
	zErr = strerror(iErrno);
#endif

	if (zPath == 0)
		zPath = "";
	sqlite3_log(errcode,
		"os_unix.c:%d: (%d) %s(%s) - %s",
		iLine, iErrno, zFunc, zPath, zErr
	);

	return errcode;
}

static void robust_close(unixFile* pFile, int h, int lineno)
{

}


static int proxyLock(sqlite3_file *id, int eFileLock)
{
	unixFile *pFile = (unixFile*)id;
	int rc = proxyTakeConch(pFile);//7023
	if (rc == SQLITE_OK)
	{
		proxyLockingContext *pCtx = (proxyLockingContex*)pFile->lockingContext;
		if (pCtx->conchHeld > 0)
		{
			unixFile *proxy = pCtx->lockProxy;
			rc = proxy->pMethod->xLock((sqlite3_file*)proxy, eFileLock);
			pFile->eFileLock = proxy->eFileLock;
		}
		else
		{
		}
	}
	return rc;
}


/*
	dot-file 锁

*/


/*
	flock锁

	int flock(int fd, int operation);
	LOCK_SH
	LOCK_EX
	LOCK_UN
*/