#pragma once

#include "Define.h"
#include <stdio.h>
#include <mutex>
#include <queue>


//Ŭ���̾�Ʈ ������ ������� ����ü
class ClientInfo
{
public:
	ClientInfo()
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(OverlappedEX));
	}

	void Init(const UINT32 index, HANDLE iocpHandle_)
	{
		mIndex = index;
		mIOCPHandle = iocpHandle_;
	}

	UINT32 GetIndex() { return mIndex; }

	bool IsConnectd() { return isConnected; }
	
	SOCKET GetSock() { return socket; }

	// UINT64 GetLatestClosedTimeSec() { return mLatestClosedTimeSec; }

	char* RecvBuffer() { return mRecvBuf; }


	bool OnConnect(HANDLE iocpHandle_, SOCKET socket_)
	{
		socket = socket_;
		isConnected = true;
		
		Clear();

		//I/O Completion Port��ü�� ������ �����Ų��.
		if (BindIOCompletionPort(iocpHandle_) == false)
		{
			return false;
		}

		return BindRecv();
	}

	void Close(bool bIsForce = false)
	{
	//	struct linger stLinger = { 0, 0 };	// SO_DONTLINGER�� ����

	//// bIsForce�� true�̸� SO_LINGER, timeout = 0���� �����Ͽ� ���� ���� ��Ų��. ���� : ������ �ս��� ������ ���� 
	//	if (true == bIsForce)
	//	{
	//		stLinger.l_onoff = 1;
	//	}

		//socketClose������ ������ �ۼ����� ��� �ߴ� ��Ų��.
		shutdown(socket, SD_BOTH);

		//���� �ɼ��� �����Ѵ�.
		//setsockopt(socket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));
				
		isConnected = false;

		// mLatestClosedTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		//���� ������ ���� ��Ų��.
		closesocket(socket);		
		socket = INVALID_SOCKET;
	}

	void Clear()
	{		
	}

	bool PostAccept(SOCKET listenSock_)
	{
		//printf_s("PostAccept. client Index: %d\n", GetIndex());

		socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP,
			NULL, 0, WSA_FLAG_OVERLAPPED);
		if (socket == INVALID_SOCKET)
		{
			printf_s("client Socket WSASocket Error : %d\n", GetLastError());
			return false;
		}

		ZeroMemory(&mAcceptContext, sizeof(OverlappedEX));
		
		DWORD bytes = 0;
		DWORD flags = 0;
		mAcceptContext.m_wsaBuf.len = 0;
		mAcceptContext.m_wsaBuf.buf = nullptr;
		mAcceptContext.m_eOperation = IOOperation::ACCEPT;
		mAcceptContext.SessionIndex = mIndex;

		/*
		* AcceptEx�� ������ Socket�� �Ű������� ����������Ѵ�
		*/
		if (FALSE == AcceptEx(listenSock_, socket, mAcceptBuf, 0,
			sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED) & (mAcceptContext)))
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				printf_s("AcceptEx Error : %d\n", GetLastError());
				return false;
			}
		}

		return true;
	}

	bool AcceptCompletion()
	{
		/*
		* CreateIOCompletionPort �κа� socket ���� 
		*/
		if (OnConnect(mIOCPHandle, socket) == false)
		{
			return false;
		}

		SOCKADDR_IN		stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);
		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
		printf("Ŭ���̾�Ʈ ���� : IP(%s) SOCKET(%d)\n", clientIP, (int)socket);
		
		return true;
	}

	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		//socket�� pClientInfo�� CompletionPort��ü�� �����Ų��.
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock()
			, iocpHandle_
			, (ULONG_PTR)(this), 0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
			return false;
		}

		return true;
	}

	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		//Overlapped I/O�� ���� �� ������ ������ �ش�.
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCK_RECVBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(socket,
			&(mRecvOverlappedEx.m_wsaBuf),
			1, // 1���ϸ� ���� ū ���ΰ�?.. �ѹ��� ���� �� �ִ� ������ �ִ뷮 �κ�
			// TCP ������������ ���� ū������ �ϱ⵵�� 
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (mRecvOverlappedEx),
			NULL);

		//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[����] WSARecv()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	/*
	* �ش� �κ� ������ ���� 
	*/
	// 1���� �����忡���� ȣ���ؾ� �Ѵ�!
	bool SendMsg(const UINT32 dataSize_, char* pMsg_)
	{	
		auto sendOverlappedEx = new OverlappedEX;
		ZeroMemory(sendOverlappedEx, sizeof(OverlappedEX));
		sendOverlappedEx->m_wsaBuf.len = dataSize_;
		sendOverlappedEx->m_wsaBuf.buf = new char[dataSize_];
		CopyMemory(sendOverlappedEx->m_wsaBuf.buf, pMsg_, dataSize_);
		sendOverlappedEx->m_eOperation = IOOperation::SEND;
		
		std::lock_guard<std::mutex> guard(mSendLock);

		mSendDataqueue.push(sendOverlappedEx);

		if (mSendDataqueue.size() == 1)
		{
			SendIO();
		}
		
		return true;
	}	

	void SendCompleted(const UINT32 dataSize_)
	{		
		printf("[�۽� �Ϸ�] bytes : %d\n", dataSize_);

		std::lock_guard<std::mutex> guard(mSendLock);

		// delete[] mSendDataqueue.front()->m_wsaBuf.buf;
		
		delete mSendDataqueue.front();
		mSendDataqueue.pop();

		if (mSendDataqueue.empty() == false)
		{
			SendIO();
		}
	}


private:
	bool SendIO()
	{
		auto sendOverlappedEx = mSendDataqueue.front();

		DWORD dwRecvNumBytes = 0;
		int nRet = WSASend(socket,
			&(sendOverlappedEx->m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED)sendOverlappedEx,
			NULL);

		//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			// printf("[����] WSASend()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	bool SetSocketOption()
	{
		/*if (SOCKET_ERROR == setsockopt(mSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)GIocpManager->GetListenSocket(), sizeof(SOCKET)))
		{
			printf_s("[DEBUG] SO_UPDATE_ACCEPT_CONTEXT error: %d\n", GetLastError());
			return false;
		}*/

		int opt = 1;
		if (SOCKET_ERROR == setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int)))
		{
			printf_s("[DEBUG] TCP_NODELAY error: %d\n", GetLastError());
			return false;
		}

		opt = 0;
		if (SOCKET_ERROR == setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (const char*)&opt, sizeof(int)))
		{
			printf_s("[DEBUG] SO_RCVBUF change error: %d\n", GetLastError());
			return false;
		}

		return true;
	}


private:

	INT32 mIndex = 0;
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	bool isConnected = 0;

	SOCKET			socket = INVALID_SOCKET;			//Cliet�� ����Ǵ� ����

	OverlappedEX	mAcceptContext;
	char mAcceptBuf[64];

	OverlappedEX	mRecvOverlappedEx;	//RECV Overlapped I/O�۾��� ���� ����	
	char			mRecvBuf[MAX_SOCK_RECVBUF]; //������ ����

	std::mutex mSendLock;
	std::queue<OverlappedEX*> mSendDataqueue;
};