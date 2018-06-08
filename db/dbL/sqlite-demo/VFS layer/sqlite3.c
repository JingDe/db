#include"sqlite3.h"
#include<time.h>
#include<cstdarg>
#include<stdint.h>


#define MEMTYPE_HEAP       0x01  /* General heap allocations */


void sqlite3_free(void * p)
{
	if (p == 0)
		return;
	
	free(p);
}


void sqlite3_log(int iErrCode, const char *zFormat, ...)
{
	va_list ap;
	va_start(ap, zFormat);
	char buf[200];
	vsnprintf(buf, sizeof(buf), zFormat, ap);
	va_end(ap);
	prinf("%d: %s\n", iErrCode, buf);
}