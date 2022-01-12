#include "X9PServer.h"
#include <stdio.h>
#include <vector>
#include "fs.h"
#include <unordered_map>



static std::unordered_map<fid_t, direntry_t*> s_fids;


void CBaseX9PServer::Begin(const char* ip, const char* port)
{
	m_listener.Listen(ip, port);

	m_maxmessagesize = 8192;
	m_workingbuf = (char*)malloc(m_maxmessagesize);

	virt_init();

}

void CBaseX9PServer::End()
{

}

void CBaseX9PServer::AcceptConnections()
{
	for(;;)
	{
		TCPClientSocket client;
		ISOCKET_RESULT r = m_listener.Accept(client);
		if (r == ISOCKET_RESULT::OK)
		{
			printf("new connection\n");
			m_connections.push_back({ client });
		}
		else
			break;
	} 
}



void CBaseX9PServer::ProcessPackets()
{
	for (int idx = 0; idx < m_connections.size(); idx++)
	{
		CX9PConnection& client = m_connections[idx];

		int written;
		ISOCKET_RESULT r = client.m_socket.Recv(m_workingbuf, m_maxmessagesize, written);
	
		// Anything new?
		if (r == ISOCKET_RESULT::WOULDBLOCK)
			continue;

		// Disconnected?
		if (written == 0 || r != ISOCKET_RESULT::OK)
		{
			client.m_socket.Close();
			m_connections.erase(m_connections.begin() + idx);
			idx--;
			printf("disconnected\n");
			continue;
		}

		printf("\ngot!\n");


		char* respbuf = (char*)malloc(m_maxmessagesize);

		// Alright, what're we working with?
		message_t* msg = (message_t*)m_workingbuf;
		
		puts(X9PMessageTypeName(msg->type));
		/*

		if (msg->type == X9P_TVERSION)
		{
			
		}
		else if (msg->type == X9P_TATTACH)
		{
			
		}
		else
		{
			for (int k = 0; k < written; k++)
			{
				printf("%u|", (unsigned int)((unsigned char*)m_workingbuf)[k]);
			}
		}
		*/

		foperr_t err;
		fauth_t* auth = 0;

		message_t* msgresp = (Rwalk_t*)respbuf;
		msgresp->type = msg->type + 1;
		msgresp->tag = msg->tag;

		switch (msg->type)
		{
		case X9P_TVERSION:
		{
			Tversion_t* req = (Tversion_t*)m_workingbuf;


			// Compose a response
			const int size = sizeof(Tversion_t) + 2 + X9P_VERSION_LEN;
			Rversion_t* resp = (Rversion_t*)respbuf;
			resp->size = size;
			resp->type = X9P_RVERSION;
			resp->tag = req->tag;

			// Negotiate a nice size
			//if (req->msize > m_maxmessagesize)
			resp->msize = m_maxmessagesize;
			//else
			//	resp->msize = req->msize;

			// We only support 1 version of 9p
			resp->version()->len = X9P_VERSION_LEN;
			memcpy(resp->version()->str(), X9P_VERSION, X9P_VERSION_LEN);

			// Shoot off our response
			client.m_socket.Send((char*)resp, resp->size);

			printf("Tversion %d\n", resp->msize);
			break;
		}
		/*
		case X9P_TAUTH:
		{
			"NOT SUPPORTED";
			break;
		}
		*/
		case X9P_TATTACH:
		{
			Tattach_t* req = (Tattach_t*)m_workingbuf;

			// Put it on the stack for speediness

			// Compose a response
			Rattach_t resp;
			resp.size = sizeof(Rattach_t);
			resp.type = X9P_RATTACH;
			resp.tag = req->tag;

			// Track the root to the fid
			direntry_t* root = virt_rootdirentry();
			s_fids.emplace(req->fid, root);
			resp.qid = filenodeqid(root->node);

			// Shoot off our response
			client.m_socket.Send((char*)&resp, resp.size);

			printf("Tattach %s\n", req->uname()->str());
			break;
		}
		/*
		case X9P_TFLUSH:
		{

			break;
		}
		*/
		case X9P_TCLUNK:
		{
			Twalk_t* req = (Twalk_t*)m_workingbuf;
			s_fids.erase(s_fids.find(req->fid));

			Rclunk_t* resp = (Rclunk_t*)respbuf;
			resp->size = sizeof(Rclunk_t);
			client.m_socket.Send((char*)resp, resp->size);

			break;
		}



		case X9P_TWALK:
		{
			Twalk_t* req = (Twalk_t*)m_workingbuf;
			direntry_t* start = s_fids.at(req->fid);
			filenode_t* node = start->node;
			printf("Walking ");
			xstrprint(start->name);

			Rwalk_t* resp = (Rwalk_t*)respbuf;
			resp->type = X9P_RWALK;
			resp->tag = req->tag;
			if (req->nwname > 0)
			{

				walkparam_t param;
				param.nwname = req->nwname;
				param.wname = req->wname();
				walkret_t ret;
				ret.wqid = new filenode_t*[req->nwname];

				err = node->ops->walk(auth, node, &param, &ret);
				if (err) goto reportError;

				size_t size = sizeof(Rwalk_t) + sizeof(qid_t) * ret.nwqid;
			
				resp->size = size;
				resp->nwqid = ret.nwqid;

				for (int i = 0; i < ret.nwqid; i++)
					resp->wqid()[i] = filenodeqid(ret.wqid[i]);

				s_fids.emplace(req->newfid, ret.walked);
				//s_fids[req->newfid] = ret.walked;

				delete ret.wqid;
			}
			else
			{
				resp->nwqid = 0;
				resp->size = sizeof(Rwalk_t);

				s_fids.emplace(req->newfid, start);
			}

			client.m_socket.Send((char*)resp, resp->size);



			break;
		}
		case X9P_TOPEN:
		{
			Topen_t* req = (Topen_t*)m_workingbuf;
			filenode_t* node = s_fids.at(req->fid)->node;
			uint64_t size = 0;
			err = node->ops->open(auth, node, req->mode, &size);
			if (err)
			{

			}
			else
			{
				Ropen_t* resp = (Ropen_t*)respbuf;
				resp->size = sizeof(Ropen_t);
				resp->qid = filenodeqid(node);

				//resp->iounit = size;
				//if (resp->iounit + sizeof(Rread_t) + sizeof(xstrlen_t) > m_maxmessagesize)
				resp->iounit = m_maxmessagesize - sizeof(Rread_t) - sizeof(xstrlen_t);

				client.m_socket.Send((char*)resp, resp->size);
			}
			break;
		}
		case X9P_TCREATE:
		{
			Tcreate_t* req = (Tcreate_t*)m_workingbuf;
			direntry_t* dentry = s_fids.at(req->fid);
			filenode_t* dir = dentry->node;
			direntry_t* ret = 0;
			err = dir->ops->create(auth, dir, req->name(), *req->perm(), *req->mode(), &ret);
			if (err) 
				goto reportError;

			Rcreate_t* resp = (Rcreate_t*)respbuf;
			resp->size = sizeof(Rcreate_t);
			resp->qid = filenodeqid(ret->node);
			resp->iounit = 1024;

			s_fids[req->fid] = ret;
			client.m_socket.Send((char*)resp, resp->size);


			break;
		}
		case X9P_TREAD:
		{
			Tread_t* req = (Tread_t*)m_workingbuf;
			direntry_t* entry = s_fids.at(req->fid);
			filenode_t* node = entry->node;
			fileio_t fio;
			printf("Reading %u:%u of ", req->offset, req->count);
			xstrprint(entry->name);

			Rread_t* resp = (Rread_t*)respbuf;

			// TODO: CAP THIS OFF! IOUNITS!
			fio.data = resp->data();
			fio.result = 0;
			fio.offset = req->offset;
			fio.count = req->count;
			node->ops->read(auth, node, &fio);


			resp->size = sizeof(Rread_t) + fio.result;
			resp->count = fio.result;

			client.m_socket.Send((char*)resp, resp->size);

			break;
		}
		case X9P_TWRITE:
		{
			Twrite_t* req = (Twrite_t*)m_workingbuf;
			direntry_t* entry = s_fids.at(req->fid);
			filenode_t* node = entry->node;
			fileio_t fio;
			printf("Writing %u:%u to ", req->offset, req->count);
			xstrprint(entry->name);

			Rwrite_t* resp = (Rwrite_t*)respbuf;

			// TODO: CAP THIS OFF! IOUNITS!
			fio.data = req->data();
			fio.result = 0;
			fio.offset = req->offset;
			fio.count = req->count;
			err = node->ops->write(auth, node, &fio);
			if (err) goto reportError;

			resp->size = sizeof(Rread_t) + fio.result;
			resp->count = fio.result;

			client.m_socket.Send((char*)resp, resp->size);

			//filenode_t* node = s_fids.at(req->fid)->node;
			//node->ops->write(auth, node, );
			break;
		}
		case X9P_TREMOVE:
		{
			Tremove_t* req = (Tremove_t*)m_workingbuf;
			direntry_t* de = s_fids.at(req->fid);
			filenode_t* node = de->node;
			s_fids.erase(s_fids.find(req->fid));

			err = node->ops->remove(auth, de);
			if (err) goto reportError;


			Rremove_t* resp = (Rremove_t*)respbuf;
			resp->size = sizeof(Rremove_t);
			client.m_socket.Send((char*)resp, resp->size);

			break;
		}
		case X9P_TSTAT:
		{
			Tstat_t* req = (Tstat_t*)m_workingbuf;
			direntry_t* de = s_fids.at(req->fid);
			filenode_t* node = de->node;
			filestats_t stats;
			err = node->ops->stat(auth, node, &stats);
			if (!err)
			{

				size_t szname = de->name->len;
				size_t szuid  = strlen(stats.uid);
				size_t szgid  = strlen(stats.gid);
				size_t szmuid = strlen(stats.muid);
				size_t size = sizeof(Rstat_t) + szname + 2 + szuid + 2 + szgid + 2 + szmuid + 2;

				if (size > m_maxmessagesize)
				{
					printf("errrrrrrrr");
				}
				
				Rstat_t* resp = (Rstat_t*)respbuf;
				resp->size = size;
				resp->type = X9P_RSTAT;
				resp->tag = req->tag;
				resp->n = size - sizeof(message_t) + 2;

				stat_t* rootdir = &resp->stat;
				rootdir->size = size - sizeof(message_t);
				rootdir->atime = stats.atime;
				rootdir->mtime = stats.mtime;
				rootdir->mode = stats.mode | X9P_DM_READ | X9P_DM_WRITE | X9P_DM_EXEC; // X9P_DM_DIR ;
				rootdir->length = stats.size;
				rootdir->qid = filenodeqid(node);

				rootdir->name()->len = szname;
				memcpy(rootdir->name()->str(), de->name->str(), szname);

				rootdir->uid()->len = szuid;
				memcpy(rootdir->uid()->str(), stats.uid, szuid);

				rootdir->gid()->len = szgid;
				memcpy(rootdir->gid()->str(), stats.gid, szgid);

				rootdir->muid()->len = szmuid;
				memcpy(rootdir->muid()->str(), stats.muid, szmuid);


				client.m_socket.Send((char*)resp, resp->size);
			}



			printf("Tstat %u", req->fid);
			break;
		}
		case X9P_TWSTAT:
		{
			printf("Liar Twstat\n");
			Rwstat_t* resp = (Rwstat_t*)respbuf;
			resp->size = sizeof(Rwstat_t);
			client.m_socket.Send((char*)resp, resp->size);
			//node->ops->wstat(auth, node, );
			break;
		}

		default:
			for (int k = 0; k < written; k++)
			{
				printf("%u|", (unsigned int)((unsigned char*)m_workingbuf)[k]);
			}
			printf("\n");
			err = "Unknown or not implemented!";
			goto reportError;
		}

		
		free(respbuf);
		continue;
	
	reportError:
		
		printf("---! Reporting error \"%s\"", err);

		Rerror_t* resp = (Rerror_t*)respbuf;
		resp->tag = msg->tag;
		resp->type = X9P_RERROR;
		resp->ename()->len = strlen(err);
		memcpy(resp->ename()->str(), err, resp->ename()->len);
		resp->size = sizeof(Rerror_t) + resp->ename()->len;

		client.m_socket.Send((char*)resp, resp->size);
		free(respbuf);
		
	}
}
