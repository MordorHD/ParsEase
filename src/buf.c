#include "parsease.h"

int
bopen(IOBUFFER *buf, const char *fileName)
{
	int fd;

	fd = open(fileName, O_RDONLY);
	if(fd < 0)
		return -1;
	buf->iRead = buf->iWrite = 0;
	buf->fd = fd;
	buf->record = NULL;
	buf->nRecord = 0;
	return 0;
}

uint32_t
btell(IOBUFFER *buf)
{
	return lseek(buf->fd, 0, SEEK_CUR) - buf->iWrite + buf->iRead;
}

int
brewind(IOBUFFER *buf)
{
	lseek(buf->fd, 0, SEEK_SET);
	buf->iRead = buf->iWrite = 0;
	return 0;
}

int
bgetch(IOBUFFER *buf)
{
	if(buf->iRead == buf->iWrite) {
		buf->iRead = 0;
		if((buf->iWrite = (int32_t) read(buf->fd, buf->buf, sizeof(buf->buf))) < 0) {
			const int oldErrno = errno;
			close(buf->fd);
			errno = oldErrno;
			return EOF;
		}
		if(!buf->iWrite)
			return EOF;
	}
	return buf->buf[buf->iRead++];
}

int
bungetch(IOBUFFER *buf, int ch)
{
	if(!buf->iRead)
		return EOF;
	buf->buf[--buf->iRead] = ch;
	return ch;
}

int
brecord(IOBUFFER *buf, int ch)
{
	buf->record = realloc(buf->record, buf->nRecord + 1);
	ASSERT(buf->record, "Out of memory");
	buf->record[buf->nRecord++] = ch;
	return 0;
}

char *
bsaverecord(IOBUFFER *buf)
{
	int32_t n;
	char *rec;

	n = buf->nRecord;
	rec = malloc(n + 1);
	ASSERT(rec, "Out of memory");
	memcpy(rec, buf->record, n);
	rec[n] = 0;
	buf->nRecord = 0;
	return rec;
}

int
bclose(IOBUFFER *buf)
{
	free(buf->record);
	return close(buf->fd);
}

