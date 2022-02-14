#include "xstdio.h"
#include "fs.h"
#include <stdlib.h>
#include <stdio.h>
#include <unordered_map>

// Things like ".." should get handled by the local file system, rather than the remote one

static std::unordered_map<int, direntry_t*> s_fds;
static direntry_t* s_cwd = 0;
static int s_fdserial = 0;

int xopen(const char* path, enum XOPEN_FLAG oflag)
{
	int segments = 0;
	xstr_t split = xstrsplit(path, '/', &segments);

	// Check for ..'s
	xstr_t s = split;
	for (int i = 0; i < segments; i++)
	{
		if (s->len == 2 && *reinterpret_cast<short*>(s->str()) == *reinterpret_cast<short*>(".."))
		{
			// Go back!
			printf("INVALID PATH");
		}
		s = xstrnext(s);
	}

	if (segments > 16)
		printf("TOO MANY SEGMENTS!");


	walkparam_t param;
	param.nwname = segments;
	param.wname = split;

	walkret_t ret;
	xerr_t err = s_cwd->node->ops->walk(0, s_cwd->node, &param, &ret);
	if (err)
	{
		printf(err);
		return -1;
	}


	if (oflag & XO_CREAT)
	{
		s_cwd->node->ops->create(0, s_cwd->node, );
	}
	else
	{
		s_cwd->node->ops->open(0, ret.walked, X9P_OPEN_READWRITE, 0)
	}


	int id = s_fdserial;
	s_fdserial++;

	s_fds.emplace(id, ret.walked);
	return id;
}

void xclose(int xfd)
{

}

int xaio_write(xaiocb* aiocbp)
{
	int fd = aiocbp->aio_fildes;
	if (fd == -1) return -1;

	auto find = s_fds.find(fd);
	if (find == s_fds.end()) return -1;

	direntry_t* de = find->second;

	fileio_t fio;
	fio.count = aiocbp->aio_nbytes;
	fio.offset = 0;
	fio.data = (unsigned char*)aiocbp->aio_buf;
	xerr_t err = de->node->ops->write(0, de->node, &fio);

	if (err) return -1;
	return 0;
}

int xread(int xfd, void* buf, size_t nbytes)
{
	return 0;

}

