#ifndef SQLITE3_H_
#define SQLITE3_H_



#define SQLITE_OK           0   /* Successful result */
#define SQLITE_IOERR       10   /* Some kind of disk I/O error occurred */



#define SQLITE_IOERR_READ              (SQLITE_IOERR | (1<<8))
#define SQLITE_IOERR_SHORT_READ			(SQLITE_IOERR | (2<<8))
#define SQLITE_IOERR_WRITE             (SQLITE_IOERR | (3<<8))
#define SQLITE_IOERR_FSYSNC				(SQLITE_IOERR | (4<<8))
#define SQLITE_IOERR_FSTAT				(SQLITE_IOERR |  (5<<8) 

void sqlite3_free(void *);

void sqlite3_log(int iErrCode, const char *format, ...);

#endif