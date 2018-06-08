/*
	demo VFS的实现

**    File-system: access(), unlink(), getcwd()
**    File IO:     open(), read(), write(), fsync(), close(), fstat()
**    Other:       sleep(), usleep(), time()

提交事务：
	先写回滚的日志文件：
		回滚信息从开头顺序写入日志文件
		sync
		修改日志文件开头的几个字节
		sync

	分配8192字节的缓存，sync时写入日志文件。
*/

#include"vfs.h"
#include"sqlite3.h"

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/param.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

#include<stdlib.h>

#ifndef SQLITE_DEMOVFS_BUFFERSZ
# define SQLITE_DEMOVFS_BUFFERSZ 8192
#endif

#define MAXPATHNAME 512

typedef struct DemoFile;
struct DemoFile {
	sqlite3_file base;
	int fd;

	char *aBuffer;	// 文件读写的缓存，应用层缓存
	int nBuffer;

	int64_t iBufferOfst;	// aBuffer[0]在文件中的偏移量
};

// 刷新到内核级缓存
static int demoDirectWrite(
	DemoFile *p,                    /* File handle */
	const void *zBuf,               /* Buffer containing data to write */
	int iAmt,                       /* Size of data to write in bytes */
	int64_t iOfst              /* File offset to write to */)
{
	off_t ofst; // long int
	size_t nWrite;

	ofst = lseek(p->fd, iOfst, SEEK_SET);
	if (ofst != iOfst)
		return SQLITE_IOERR_WRITE;

	nWrite = write(p->fd, zBuf, iAmt);
	if (nWrite != iAmt)
		return SQLITE_IOERR_WRITE;

	return SQLITE_OK;
}

static int demoClose(sqlite3_file *pFile)
{
	int rc;
	DemoFile *p = (DemoFile*)pFile;
	rc = demoFlushBuffer(p);
	//sqlite3_free(p->aBuffer);
	free(p->aBuffer);// free对于malloc; 对于new，使用delete
	close(p->fd);
	return rc;
}

static int demoFlushBuffer(DemoFile* p)
{
	int rc = SQLITE_OK;
	if (p->nBuffer)
	{
		rc = demoDirectWrite(p, p->aBuffer, p->nBuffer, p->iBufferOfst);
		p->nBuffer = 0;
	}
	return rc;
}

static int demoRead(sqlite3_file *pFile,
	void *zBuf,
	int iAmt,
	int64_t iOfst)
{
	DemoFile *p = (DemoFile*)pFile;
	off_t ofst;
	int nRead;
	int rc;

	rc = demoFlushBuffer(p);
	if (rc != SQLITE_OK)
		return rc;

	ofst = lseek(p->fd, iOfst, SEEK_SET);
	if (ofst != iOfst)
		return SQLITE_IOERR_READ;
	nRead = read(p->fd, zBuf, iAmt);

	if (nRead == iAmt)
		return SQLITE_OK;
	else if (nRead >= 0)
		return SQLITE_IOERR_SHORT_READ;

	return SQLITE_IOERR_READ;
}

static int demoWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, int64_t iOfst)
{
	DemoFile* p = (DemoFile*)pFile;

	if (p->aBuffer)
	{
		char *z = (char*)zBuf;
		int n = iAmt;
		int64_t i = iOfst;

		while (n > 0)
		{
			int nCopy;

			// p->iBufferOfst + p->nBuffer != i，表示当前写的数据不是接着缓存p->nBuffer中的数据之后的
			// 允许多个writer在不同位置写同一个文件
			if (p->nBuffer == SQLITE_DEMOVFS_BUFFERSZ || p->iBufferOfst + p->nBuffer != i)
			{
				int rc = demoFlushBuffer(p);
				if (rc != SQLITE_OK)
					return rc;
			}
			assert(p->nBuffer == 0 || p->iBufferOfst + p->nBuffer == i);
			p->iBufferOfst = i - p->nBuffer;//当p->iBufferOfst + p->nBuffer != i时，更新文件偏移量p->iBufferOfst

			nCopy = SQLITE_DEMOVFS_BUFFERSZ - p->nBuffer;
			if (nCopy > n)
				nCopy = n;
			memcpy(&p->aBuffer[p->nBuffer], z, nCopy);
			p->nBuffer += nCopy;

			n -= nCopy;
			i += nCopy;
			z += nCopy;
		}
	}
	else
		return demoDirectWrite(p, zBuf, iAmt, iOfst);
	return SQLITE_OK;
}

static int demoTruncate(sqlite3_file * pFile, int64_t size)
{
#if 0
	if (ftruncate(((DemoFile*)pFile)->fd, size))
		return SQLITE_OK;
#endif
	return SQLITE_OK;
}

static int demoSync(sqlite3_file *pFile, int flags)
{
	DemoFile* p = (DemoFile*)pFile;
	int rc;

	rc = demoFlushBuffer(p);
	if (rc != SQLITE_OK)
		return rc;

	rc = fsync(p->fd);// 同步文件在内存中的数据到磁盘上
	return (rc == 0 ? SQLITE_OK : SQLITE_IOERR_FSYSNC);
}

static int demoFileSize(sqlite3_file *pFile, int64_t *pSize)
{
	DemoFile *p = (DemoFile*)pFile;
	int rc;
	struct stat sStat;

	rc = demoFlushBuffer(p);
	if (rc != SQLITE_OK)
		return rc;

	rc = fstat(p->fd, &sStat);
	if (rc != 0)
		return SQLITE_IOERR_FSTAT;
	*pSize = sStat.st_size;
	return SQLITE_OK;
}

static int demoLock(sqlite3_file *pFile, int eLock)
{
	return SQLITE_OK;
}

static int demoUnlock(sqlite3_file *pFile, int eLock)
{
	return SQLITE_OK;
}

