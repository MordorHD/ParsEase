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
bclose(IOBUFFER *buf)
{
	return close(buf->fd);
}

