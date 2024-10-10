#pragma once

#include "Define.h"
#include <queue>

//Ŭ���̾�Ʈ ������ ������� ����ü
class ClientInfo
{
public:
	ClientInfo() = default;


	void Init(const UINT32 index, HANDLE iocpHandle_)
	{
		ZeroMemory(&mRecvOverlappedEx, sizeof(OverlappedEX));
		//ZeroMemory(mRecvBuf, sizeof(mRecvBuf));  // mRecvBuf�� �ʱ�ȭ�Ͽ� ������ �߻����� �ʵ��� ��

		mIndex = index;
		mIOCPHandle = iocpHandle_;
	}

	UINT32 GetIndex() 
	{
		return mIndex; 
	}

	bool IsConnectd() 
	{ 
		return isConnected; 
	}
	
	SOCKET GetSock() 
	{ 
		return _socket; 
	}

	UINT64 GetLatestClosedTimeSec() 
	{ 
		return mLatestClosedTimeSec; 
	}

	char* RecvBuffer() 
	{ 
		return mRecvBuf; 
	}


	bool OnConnect(HANDLE iocpHandle_, SOCKET socket)
	{
		_socket = socket;
		isConnected = true;

		//I/O Completion Port��ü�� ������ �����Ų��.
		if (BindIOCompletionPort(iocpHandle_) == false)
		{
			std::cout << "BindIOCompletionPort false" << std::endl;
			return false;
		}

		if (BindRecv() == false)
		{
			return false;
		}
	}

	void Close(bool bIsForce = false)
	{
		//socketClose������ ������ �ۼ����� ��� �ߴ� ��Ų��.
		shutdown(_socket, SD_BOTH);
		isConnected = false;

		//���� ������ ���� ��Ų��.
		closesocket(_socket);		
		_socket = INVALID_SOCKET;
	}

	bool PostAccept(SOCKET listenSock_)
	{

		printf_s("PostAccept. client Index: %d\n", GetIndex());

		// mLatestClosedTimeSec  �ش� ���� ������ session ��Ų�� 
		mLatestClosedTimeSec = UINT32_MAX;

		_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_IP,
			NULL, 0, WSA_FLAG_OVERLAPPED);
		if (_socket == INVALID_SOCKET)
		{
			return false;
		}

		//socket �� ��� �ɼ� �߰� 
		int optval = 1;
		if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) == SOCKET_ERROR)
		{
			std::cout << "setsockopt(SO_REUSEADDR) ����: " << WSAGetLastError() << std::endl;
			closesocket(_socket);
			_socket = INVALID_SOCKET;
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
		* WSAAccept ��� AcceptEx�� ����Ͽ� ���� -> �񵿱�� ��ȯ 
		*/
		/*
		* AcceptEx�� ������ Socket�� �Ű������� ����������Ѵ�
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
		 createIOCompletionPort �κа� socket ���� 
		*/
		if (OnConnect(mIOCPHandle, _socket) == false)
		{
			return false;
		}
		return true;
	}

	bool BindIOCompletionPort(HANDLE iocpHandle_)
	{
		//socket�� pClientInfo�� CompletionPort��ü�� �����Ų��.
		auto hIOCP = CreateIoCompletionPort((HANDLE)GetSock(), iocpHandle_, (ULONG_PTR)(this), 0);

		if (hIOCP == INVALID_HANDLE_VALUE)
		{
			printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
			return false;
		}
		return true;
	}

	bool BindRecv()
	{
		if (!isConnected)  // ������ ����Ǿ����� Ȯ��
		{
			std::cout << "BIND RECV ERROR: Socket is not connected." << std::endl;
			return false;

		}
		DWORD dwFlag = 0;
		DWORD dwRecvNumBytes = 0;

		///mRecvBuf�� ������ �ִ� ��Ȳ���� mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;�� ���ϸ� ������ �߻� ��  
		// mRecvBuf�� ��ȿ�� �޸����� Ȯ��
		if (mRecvBuf == nullptr)
		{
			std::cout << "BIND RECV ERROR: mRecvBuf is null." << std::endl;
			return false;
		}
				
		//Overlapped I/O�� ���� �� ������ ������ �ش�.
		mRecvOverlappedEx.m_wsaBuf.len = MAX_SOCK_RECVBUF;
		mRecvOverlappedEx.m_wsaBuf.buf = mRecvBuf;
		mRecvOverlappedEx.m_eOperation = IOOperation::RECV;

		if (_socket == INVALID_SOCKET)
		{
			std::cout << "WSARecv�� invalid_socket ���" << std::endl;
			return false;
		}

		//WSARecv Error �߻� WW
		int nRet = WSARecv(_socket,
			&(mRecvOverlappedEx.m_wsaBuf),
			1, // 1���ϸ� ���� ū ���ΰ�?.. �ѹ��� ���� �� �ִ� ������ �ִ뷮 �κ�
			// TCP ������������ ���� ū������ �ϱ⵵�� 
			&dwRecvNumBytes,
			&dwFlag,
			(LPWSAOVERLAPPED) & (mRecvOverlappedEx),
			NULL);

		if (nRet != SOCKET_ERROR && dwRecvNumBytes > 0)
		{
			// ���ŵ� �������� ��ȿ���� Ȯ��
			if (dwRecvNumBytes <= sizeof(mRecvBuf))
			{
				// ��ȿ�� ������ ó��
			}
			else
			{
				std::cout << "Received data exceeds buffer size." << std::endl;
				return false;
			}
		}

		//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�., SOCKET_ERROR -> -1
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			//10057 Error 
			std::cout << "BIND RECV ERROR : " << WSAGetLastError() << std::endl;
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
		int nRet = WSASend(_socket,
			&(sendOverlappedEx->m_wsaBuf),
			1,
			&dwRecvNumBytes,
			0,
			(LPWSAOVERLAPPED)sendOverlappedEx,
			NULL);

		//socket_error�̸� client socket�� �������ɷ� ó���Ѵ�.
		if (nRet == SOCKET_ERROR && (WSAGetLastError() != ERROR_IO_PENDING))
		{
			printf("[����] WSASend()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		return true;
	}


private:

	INT32 mIndex = 0;
	HANDLE mIOCPHandle = INVALID_HANDLE_VALUE;

	UINT64 mLatestClosedTimeSec = 0;
	bool isConnected = false;

	SOCKET			_socket = INVALID_SOCKET;			//Cliet�� ����Ǵ� ����

	//overlappedEx�� accepContext�� �����ϰ�, recvOverlappedEx�� ���� ������ �ʿ��� �����Ѵ� 

	OverlappedEX	mAcceptContext;
	char mAcceptBuf[64];

	OverlappedEX	mRecvOverlappedEx;	//RECV Overlapped I/O�۾��� ���� ����	
	char			mRecvBuf[MAX_SOCK_RECVBUF]; //������ ����

	std::mutex mSendLock;
	std::queue<OverlappedEX*> mSendDataqueue;
};