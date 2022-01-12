#include "fs.h"

qid_t filenodeqid(filenode_t* node)
{
	qid_t qid;
	qid.path = node->nid;
	qid.version = node->ops->fileversion(node);
	qid.type = 0;
	if (node->type == X9P_FT_DIRECTORY)
		qid.type |= X9P_QT_DIR;
	if (node->type == X9P_FT_APPEND)
		qid.type |= X9P_QT_APPEND;

	return qid;
}


uint32_t filenodenewid()
{
	static uint32_t id = 0;
	return id++;
}