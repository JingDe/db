/*
	�ļ��������ࣺ
	**   1. POSIX locking (the default),
	**   2. No locking,
	**   3. Dot-file locking,
	**   4. flock() locking,
	**   5. AFP locking (OSX only),
	**   6. Named POSIX semaphores (VXWorks only),
	**   7. proxy locking. (OSX only)


	Posix Advisory Locking
	һ���ļ���Ӳ���ӻ��߷������ӣ�ʹ�ò�ͬ���ļ���������fd��
	ͬһ�����̶�����fd�Ⱥ��������һ��������ǰһ������
	���ԣ�ֻ�����ڲ�ͬ���̵��߳�֮��ͬ���ļ��ķ��ʣ���������ͬһ�����̵��߳�֮�䡣

	sqlite�ڲ�����Ѱ�����ݿ��ļ���inode������Ƿ��Ѵ�������


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

// �����ڴ�ṹ
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

// �򿪵Ĺ����ڴ����ӵ�״̬
struct unixShm {
	unixShmNode *pShmNode;     /* The underlying unixShmNode object */
	unixShm *pNext;            /* Next unixShm with the same unixShmNode */
	uint8_t hasMutex;               /* True if holding the unixShmNode mutex */
	uint8_t id;                     /* Id of this connection within its unixShmNode */
	uint16_t sharedMask;            /* Mask of shared locks held */
	uint16_t exclMask;              /* Mask of exclusive locks held */
};

// iNode�ṹ��ID
//������λunixInodeInfo�����key
struct unixFileId {
	dev_t dev; //�豸��

#if OS_VXWORKS
	struct vxworksFileId *pId;
#else
	uint64_t ino; // inode���
#endif
};

// �ļ�handle�رա�����fdû�йرյ��ļ�fd����
// �ȴ��رջ�������
struct UnixUnusedFd {
	int fd;
	int flags;
	UnixUnusedFd *pNext;
};

// iNode�ṹ
struct unixInodeInfo {
	struct unixFileId fileId; // ���ҵ�key
	int nShared; //�������ĸ���
	unsigned char eFileLock; // SHARED_LOCK, RESERVED_LOCK�� 
	unsigned char bProcessLock; // ��ռ�Ľ�����
	int nRef; // ���ü���
	unixShmNode *pShmNode; // ���inode��صĹ����ڴ�
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

// unix VFSʹ�õ�sqlite3_file����
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
	dot-file ��

*/


/*
	flock��

	int flock(int fd, int operation);
	LOCK_SH
	LOCK_EX
	LOCK_UN
*/