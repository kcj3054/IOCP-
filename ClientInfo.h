#pragma once

#include "Define.h"
#include <iostream>

#include <stdio.h>
#include <mutex>
#include <queue>


//클라이언트 정보를 담기위한 구조체
class ClientInfo
{
public:
	ClientInfo() = default;


	void Init(const UINT32 index, HANDLE iocpHandle_)
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(OverlappedEX));
		ZeroMemory(mRecvBuf, sizeof(mRecvBuf));  // mRecvBuf를 초기화하여 문제가 발생하지 않도록 함

		mIndex = index;
		mIOCPHandle = iocpHandle_;
	}

	UINT32 GetIndex() { return mIndex; }

	bool IsConnectd() { return isConnected; }
	
	SOCKET GetSock() { return _socket; }

	// UINT64 GetLatestClosedTimeSec() { return mLatestClosedTimeSec; }

	char* RecvBuffer() { return mRecvBuf; }


	bool OnConnect(HANDLE iocpHandle_, SOCKET socket)
	{
		_socket = socket;
		isConnected = true;

		//I/O Completion Port객체와 소켓을 연결시킨다.
		if (BindIOCompletionPort(iocpHandle_) == false)
		{
			return false;
		}

		if (BindRecv() == false)
		{
			return false;
		}
	}

	void Close(bool bIsForce = false)
	{
		//socketClose소켓의 데이터 송수신을 모두 중단 시킨다.
		shutdown(_socket, SD_BOTH);
		isConnected = false;

		//소켓 연결을 종료 시킨다.
		closesocket(_socket);		
		_socket = INVALID_SOCKET;
	}

	bool PostAccept(SOCKET listenSock_)
	{
		_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP,
			NULL, 0, WSA_FLAG_OVERLAPPED);
		if (_socket == INVALID_SOCKET)
		{
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
		if (AcceptEx(listenSock_, _socket, mAcceptBuf, 0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, (LPWSAOVERLAPPED) & (mAcceptContext)) == false)
		{
			if (WSAGetLastError() != WSA_IO_PENDING)
			{
				std::cout << WSAGetLastError() << std::endl;
				return false;
			}
		}

		return true;
	}

	bool AcceptCompletion()
	{
		/*
		 createIOCompletionPort 부분과 socket 연결 
		*/
		if (OnConnect(mIOCPHandle, _socket) == false)
		{
			return false;
		}
		return true;
	}

	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		//socket과 pClientInfo를 CompletionPort객체와 연결시킨다.
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(), iocpHandle_, (ULONG_PTR)(this), 0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			return false;
		}
		return true;
	}

	bool BindRecv()
	{
		if (!isConnected)  // 소켓이 연결되었는지 확인
		{
			std::cout << "BIND RECV ERROR: Socket is not connected." << std::endl;
			return false;

		}
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		//Overlapped I/O을 위해 각 정보를 셋팅해 준다.
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCK_RECVBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		int nRet = WSARecv(_socket,
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
			//10057 Error 
			std::cout << "BIND RECV ERROR : " << WSAGetLastError() << std::endl;
			return false;
		}
		return true;
	}

	/*
	* 해당 부분 수정할 예정 
	*/
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
		int nRet = WSASend(_socket,
			&(sendOverlappedEx->m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED)sendOverlappedEx,
			NULL);

		//socket_error이면 client socket이 끊어진걸로 처리한다.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[에러] WSASend()함수 실패 : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}

	bool SetSocketOption()
	{
		int opt = 1;
		if (SOCKET_ERROR == setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int)))
		{
			printf_s("[DEBUG] TCP_NODELAY error: %d\n", GetLastError());
			return false;
		}

		opt = 0;
		if (SOCKET_ERROR == setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&opt, sizeof(int)))
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

	SOCKET			_socket = INVALID_SOCKET;			//Cliet와 연결되는 소켓

	//overlappedEx가 accepContext도 존재하고, recvOverlappedEx도 존재 정리할 필요하 존재한다 

	OverlappedEX	mAcceptContext;
	char mAcceptBuf[64];

	OverlappedEX	mRecvOverlappedEx;	//RECV Overlapped I/O작업을 위한 변수	
	char			mRecvBuf[MAX_SOCK_RECVBUF]; //데이터 버퍼

	std::mutex mSendLock;
	std::queue<OverlappedEX*> mSendDataqueue;
};