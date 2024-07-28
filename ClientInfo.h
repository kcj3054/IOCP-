#pragma once

#include "Define.h"
#include <stdio.h>
#include <mutex>
#include <queue>


//클라이언트 정보를 담기위한 구조체
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

	// nullptr로 죽는다 
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

		//I/O Completion Port객체와 소켓을 연결시킨다.
		if (BindIOCompletionPort(iocpHandle_) == false)
		{
			return false;
		}

		return BindRecv();
	}

	void Close(bool bIsForce = false)
	{
	//	struct linger stLinger = { 0, 0 };	// SO_DONTLINGER로 설정

	//// bIsForce가 true이면 SO_LINGER, timeout = 0으로 설정하여 강제 종료 시킨다. 주의 : 데이터 손실이 있을수 있음 
	//	if (true == bIsForce)
	//	{
	//		stLinger.l_onoff = 1;
	//	}

		//socketClose소켓의 데이터 송수신을 모두 중단 시킨다.
		shutdown(socket, SD_BOTH);

		//소켓 옵션을 설정한다.
		//setsockopt(socket, SOL_SOCKET, SO_LINGER, (char*)&stLinger, sizeof(stLinger));
				
		isConnected = false;

		// mLatestClosedTimeSec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
		//소켓 연결을 종료 시킨다.
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
		* AcceptEx는 생성한 Socket을 매개변수에 전달해줘야한다
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
		printf_s("AcceptCompletion : SessionIndex(%d)\n", mIndex);

		if (OnConnect(mIOCPHandle, socket) == false)
		{
			return false;
		}

		SOCKADDR_IN		stClientAddr;
		int nAddrLen = sizeof(SOCKADDR_IN);
		char clientIP[32] = { 0, };
		inet_ntop(AF_INET, &(stClientAddr.sin_addr), clientIP, 32 - 1);
		printf("클라이언트 접속 : IP(%s) SOCKET(%d)\n", clientIP, (int)socket);
		
		return true;
	}

	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		//socket과 pClientInfo를 CompletionPort객체와 연결시킨다.
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock()
			, iocpHandle_
			, (ULONG_PTR)(this), 0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
			return false;
		}

		return true;
	}

	bool BindRecv()
	{
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		//Overlapped I/O을 위해 각 정보를 셋팅해 준다.
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCK_RECVBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(socket,
			&(mRecvOverlappedEx.m_wsaBuf),
			1, // 1로하면 가장 큰 값인가?.. 한번에 받을 수 있는 데이터 최대량 부분
			// TCP 순차성때문에 가장 큰것으로 하기도함 

			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (mRecvOverlappedEx),
			NULL);

		//socket_error이면 client socket이 끊어진걸로 처리한다.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[에러] WSARecv()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	// 1개의 스레드에서만 호출해야 한다!
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
		printf("[송신 완료] bytes : %d\n", dataSize_);

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

		//socket_error이면 client socket이 끊어진걸로 처리한다.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{


			// printf("[에러] WSASend()함수 실패 : %d\n", WSAGetLastError());
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

	SOCKET			socket = INVALID_SOCKET;			//Cliet와 연결되는 소켓

	OverlappedEX	mAcceptContext;
	char mAcceptBuf[64];

	OverlappedEX	mRecvOverlappedEx;	//RECV Overlapped I/O작업을 위한 변수	
	char			mRecvBuf[MAX_SOCK_RECVBUF]; //데이터 버퍼

	std::mutex mSendLock;
	std::queue<OverlappedEX*> mSendDataqueue;
};