#include "X9PServer.h"
#include "XLogging.h"
#include <stdio.h>
#include <vector>
#include <unordered_map>



void X9PServer::Begin(const char* ip, const char* port, X9PFileSystem* fs) 
{
	m_listener.Listen(ip, port);
	m_listener.SetNonblocking(true);

	m_maxmessagesize = 8192;
	m_respbuffer = (char*)malloc(m_maxmessagesize);
	
	m_filesystem = fs;
}

void X9PServer::End()
{

}


int SendError(X9PClient& cl, xerr_t err, mtag_t tag)
{
	if (!err) return 0;


	printf("\n---! Reporting error \"%s\"\n", err);

	Rerror_t* send = (Rerror_t*)cl.m_sendbuffer;
	send->tag = tag;
	send->type = X9P_RERROR;
	send->ename()->len = strlen(err);
	memcpy(send->ename()->str(), err, send->ename()->len);
	send->size = sizeof(Rerror_t) + send->ename()->len;

	cl.SendMessage(send);

	return 1;
}

void X9PServer::AcceptConnections()
{
	for(;;)
	{
		TCPClientSocket client;
		ISOCKET_RESULT r = m_listener.Accept(client);
		if (r == ISOCKET_RESULT::OK)
		{
			printf("new connection\n");
			client.SetNonblocking(true);
			m_clients.push_back({ client });
		}
		else
			break;
	} 
}


// TODO: Size of an iounit should be msize - sizeof(Rread_t) or msize - sizeof(Twrite_t)

void X9PServer::ProcessPackets()
{
	for (int idx = 0; idx < m_clients.size(); idx++)
	{
		X9PClient& cl = m_clients[idx];

		// Anything new from this client?
		//size_t written = 0;
		bool wouldblock = false;
		message_t* recvmsg = (message_t*)cl.m_recvbuffer;
		xerr_t r = cl.RecvMessage(wouldblock);

		mtag_t tag = recvmsg->tag;

		// Nothing new... Next client
		if (wouldblock)
			continue;

		// Disconnected or errored?
		if (r)
		{
			cl.m_socket.Close();
			m_clients.erase(m_clients.begin() + idx);
			idx--;
			if(r) printf(r);
			printf("disconnected\n");
			continue;
		}


		switch (recvmsg->type)
		{
		case X9P_TVERSION:
		{
			Tversion_t* recv = (Tversion_t*)recvmsg;

			// Negotiate a nice size
			uint32_t msize;
			if (recv->msize > m_maxmessagesize)
				msize = m_maxmessagesize;
			else
				msize = recv->msize;

			mtag_t tag = recv->tag;
			uint8_t type = recv->type;

			// TODO: maybe move this to when we get another response? The client could disconnect at this point
			// Bump up our buffer now that we know how much we need
			cl.m_maxmessagesize = msize;
			free(cl.m_sendbuffer);
			free(cl.m_recvbuffer);
			cl.m_sendbuffer = (char*)malloc(cl.m_maxmessagesize);
			cl.m_recvbuffer = (char*)malloc(cl.m_maxmessagesize);
			message_t* sendmsg = (message_t*)cl.m_sendbuffer;
			recvmsg = (message_t*)cl.m_recvbuffer;
			

			// At this point "recv" is invalid


			// Compose a response
			const int size = sizeof(Tversion_t) + 2 + X9P_VERSION_LEN;
			Rversion_t* send = (Rversion_t*)sendmsg;
			// Normally these first two are autofilled, but we've gotta fill em in since we deleted the buffer
			send->type = type + 1;
			send->tag = tag;
			send->size = size;
			send->msize = msize;

			// We only support 1 version of 9P for now
			xstr_t version = send->version();
			version->len = X9P_VERSION_LEN;
			memcpy(version->str(), X9P_VERSION, X9P_VERSION_LEN);


			XPRINTF("msize: %u\n", msize);
			cl.SendMessage(send);

			break;
		}
		case X9P_TAUTH:
		{
			SendError(cl, "Tauth not supported", tag);
			break;
		}
		case X9P_TATTACH:
		{
			Tattach_t* recv = (Tattach_t*)recvmsg;
			xhnd hnd = m_filesystem->NewFileHandle(0);
			cl.m_fids.emplace(recv->fid, hnd);

			m_filesystem->Tattach(hnd, recv->afid, recv->uname(), recv->aname(), [&](xerr_t err, qid_t* qid)
			{
				if (SendError(cl, err, tag)) return;

				// Compose a response
				Rattach_t* send = (Rattach_t*)cl.m_sendbuffer;
				send->type = X9P_RATTACH;
				send->tag = recvmsg->tag;
				send->size = sizeof(Rattach_t);
				send->qid = *qid;
				cl.SendMessage(send);
			});
			break;
		}
		case X9P_TFLUSH:
		{
			SendError(cl, "Tflush not supported", tag);
			break;
		}
		case X9P_TCLUNK:
		{
			Tclunk_t* recv = (Tclunk_t*)recvmsg;
			xhnd hnd = cl.m_fids[recv->fid];
			cl.m_fids.erase(recv->fid);
			m_filesystem->Tclunk(hnd, [&](xerr_t err)
			{
				if (SendError(cl, err, tag)) return;

				// Compose a response
				Rclunk_t* send = (Rclunk_t*)cl.m_sendbuffer;
				send->type = X9P_RCLUNK;
				send->tag = recvmsg->tag;
				send->size = sizeof(Rclunk_t);
				cl.SendMessage(send);
			});
			break;
		}
		case X9P_TWALK:
		{
			Twalk_t* recv = (Twalk_t*)recvmsg;
			if (recv->nwname > 16)
			{
				SendError(cl, "Twalk cannot walk further than 16 elements", recv->tag);
				break;
			}

			xhnd hnd = cl.m_fids[recv->fid];
			xhnd newhnd = m_filesystem->NewFileHandle(0);
			cl.m_fids.emplace(recv->newfid, newhnd);
			

			m_filesystem->Twalk(hnd, newhnd, recv->nwname, recv->wname(), [&](xerr_t err, uint16_t nwqid, qid_t* wqid)
			{
				if (SendError(cl, err, tag)) return;

				// Compose a response
				Rwalk_t* send = (Rwalk_t*)cl.m_sendbuffer;
				send->type = X9P_RWALK;
				send->tag = recvmsg->tag;
				send->size = sizeof(Rwalk_t) + sizeof(qid_t) * nwqid;
				send->nwqid = nwqid;
				memcpy(send->wqid(), wqid, nwqid * sizeof(qid_t));
				cl.SendMessage(send);
			});
			break;
		}
		case X9P_TOPEN:
		{
			Topen_t* recv = (Topen_t*)recvmsg;
			xhnd hnd = cl.m_fids[recv->fid];
			m_filesystem->Topen(hnd, recv->mode, [&](xerr_t err, qid_t* qid, uint32_t iounit)
			{
				if (SendError(cl, err, tag)) return;

				// Compose a response
				Ropen_t* send = (Ropen_t*)cl.m_sendbuffer;
				send->type = X9P_ROPEN;
				send->tag = recvmsg->tag;
				send->size = sizeof(Ropen_t);
				send->qid = *qid;
				send->iounit = iounit;
				cl.SendMessage(send);
			});
			break;
		}
		case X9P_TCREATE:
		{
			Tcreate_t* recv = (Tcreate_t*)recvmsg;
			xhnd hnd = cl.m_fids[recv->fid];
			m_filesystem->Tcreate(hnd, recv->name(), *recv->perm(), *recv->mode(), [&](xerr_t err, qid_t* qid, uint32_t iounit)
			{
				if (SendError(cl, err, tag)) return;

				// Compose a response
				Rcreate_t* send = (Rcreate_t*)cl.m_sendbuffer;
				send->type = X9P_RCREATE;
				send->tag = recvmsg->tag;
				send->size = sizeof(Rcreate_t);
				send->qid = *qid;
				send->iounit = iounit;
				cl.SendMessage(send);
			});
			break;
		}
		case X9P_TREAD:
		{
			Tread_t* recv = (Tread_t*)recvmsg;
			xhnd hnd = cl.m_fids[recv->fid];
			m_filesystem->Tread(hnd, recv->offset, recv->count, [&](xerr_t err, uint32_t count, uint8_t* data)
			{
				if (SendError(cl, err, tag)) return;

				// Compose a response
				Rread_t* send = (Rread_t*)cl.m_sendbuffer;
				send->type = X9P_RREAD;
				send->tag = recvmsg->tag;
				send->size = sizeof(Rread_t) + count;
				send->count = count;
				memcpy(send->data(), data, count);
				cl.SendMessage(send);
			});
			break;
		}
		case X9P_TWRITE:
		{
			Twrite_t* recv = (Twrite_t*)recvmsg;
			xhnd hnd = cl.m_fids[recv->fid];
			m_filesystem->Twrite(hnd, recv->offset, recv->count, recv->data(), [&](xerr_t err, uint32_t count)
			{
				if (SendError(cl, err, tag)) return;

				// Compose a response
				Rwrite_t* send = (Rwrite_t*)cl.m_sendbuffer;
				send->type = X9P_RWRITE;
				send->tag = recvmsg->tag;
				send->size = sizeof(Rwrite_t);
				send->count = count;
				cl.SendMessage(send);
			});
			break;
		}
		case X9P_TREMOVE:
		{
			Tremove_t* recv = (Tremove_t*)recvmsg;
			xhnd hnd = cl.m_fids[recv->fid];
			m_filesystem->Tremove(hnd, [&](xerr_t err)
			{
				if (SendError(cl, err, tag)) return;

				// Compose a response
				Rremove_t* send = (Rremove_t*)cl.m_sendbuffer;
				send->type = X9P_RREMOVE;
				send->tag = recvmsg->tag;
				send->size = sizeof(Rremove_t);
				cl.SendMessage(send);
			});
			break;
		}
		case X9P_TSTAT:
		{
			Tstat_t* recv = (Tstat_t*)recvmsg;
			xhnd hnd = cl.m_fids[recv->fid];
			m_filesystem->Tstat(hnd, [&](xerr_t err, stat_t* stat)
			{
				if (SendError(cl, err, tag)) return;

				uint64_t sz = sizeof(Rstat_t) + stat->size;
				if (sz > m_maxmessagesize)
				{
					SendError(cl, "Max message size was too small???", tag);
					printf("_________FIXME! TSTAT\n");
					return;
				}

				// Compose a response
				Rstat_t* send = (Rstat_t*)cl.m_sendbuffer;
				send->type = X9P_RSTAT;
				send->tag = recvmsg->tag;
				send->size = sz;
				send->n = 1;
				memcpy(&send->stat, stat, stat->size);
				cl.SendMessage(send);
			});
			break;
		}
		case X9P_TWSTAT:
		{
			Twstat_t* recv = (Twstat_t*)recvmsg;
			xhnd hnd = cl.m_fids[recv->fid];
			m_filesystem->Twstat(hnd, &recv->stat, [&](xerr_t err)
			{
				if (SendError(cl, err, tag)) return;

				// Compose a response
				Rwstat_t* send = (Rwstat_t*)cl.m_sendbuffer;
				send->type = X9P_RWSTAT;
				send->tag = recvmsg->tag;
				send->size = sizeof(Rwstat_t);
				cl.SendMessage(send);
			});
			break;
		}
		default:
			for (uint32_t k = 0; k < recvmsg->size; k++)
			{
				printf("%u|", (unsigned int)((unsigned char*)recvmsg)[k]);
			}
			printf("\n");
			SendError(cl, "Invalid or missing implementation", tag);
		}

		
	}
}
