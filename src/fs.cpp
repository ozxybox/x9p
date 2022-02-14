#include "fs.h"

qid_t filenodeqid(XVirtualFile* node)
{
	qid_t qid;
	qid.path = (uint64_t)node;
	qid.version = node->Version();
	qid.type = 0;
	ftype_t type = node->Type();
	if (type == X9P_FT_DIRECTORY)
		qid.type |= X9P_QT_DIR;
	if (type == X9P_FT_APPEND)
		qid.type |= X9P_QT_APPEND;

	return qid;
}


uint32_t filenodenewid()
{
	static uint32_t id = 0;
	return id++;
}
