#include "XLogging.h"
#include "X9PProtocol.h"

void XLogDumpStat(stat_t* stat)
{
	printf("Size: %d\n", (int)stat->size);
	printf("Unused: ");

	for (int i = 0; i < 6; i++)
		printf("%2X, ", stat->_unused[i]);
	putchar('\n');


	printf("QID:\n");
	printf("\tType: ");

	if (stat->qid.type & X9P_QT_DIR)
		printf("DIR");
	else
		printf("FILE");

	if (stat->qid.type & X9P_QT_APPEND)
		printf(" | APPEND");
	if (stat->qid.type & X9P_QT_EXCL)
		printf(" | EXCL");
	if (stat->qid.type & X9P_QT_MOUNT)
		printf(" | MOUNT");
	if (stat->qid.type & X9P_QT_AUTH)
		printf(" | AUTH");
	if (stat->qid.type & X9P_QT_TMP)
		printf(" | TMP");
	putchar('\n');

	printf("\tVersion: %u\n", stat->qid.version);
	printf("\tPath: %llu\n", stat->qid.path);

	printf("Dir Mode:\n");
	printf("\tType: ");
	if (stat->mode & X9P_DM_DIR)
		printf("DIR");
	else
		printf("FILE");
	if (stat->mode & X9P_DM_APPEND)
		printf(" | APPEND");
	if (stat->mode & X9P_DM_EXCL)
		printf(" | EXCL");
	if (stat->mode & X9P_DM_MOUNT)
		printf(" | MOUNT");
	if (stat->mode & X9P_DM_AUTH)
		printf(" | AUTH");
	if (stat->mode & X9P_DM_TMP)
		printf(" | TMP");
	putchar('\n');
	printf("\tPermissions: %#o\n", stat->mode & X9P_DM_PERM_MASK);

	printf("Access Time: %u\n", stat->atime);
	printf("Modification Time: %u\n", stat->mtime);
	printf("Length: %llu\n", stat->length);

	printf("File Name: ");
	xstrprint(stat->name());
	putchar('\n');


	printf("Owner: ");
	xstrprint(stat->uid());
	putchar('\n');

	printf("Group: ");
	xstrprint(stat->gid());
	putchar('\n');

	printf("Last Modifier: ");
	xstrprint(stat->muid());
	putchar('\n');

}