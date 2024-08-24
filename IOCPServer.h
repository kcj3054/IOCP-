#pragma once
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "mswsock.lib")

#include "ClientInfo.h"
#include "Define.h"
#include <thread>
#include <vector>
#include <iostream>


/*
* iocp���� Ǯ���ϴ� ���ҵ��� Ŀ�ο����Ѵ� 
*/

/*
* �񵿱�� ���� �Լ����� �ϴ� ������ return. 
* ��� �б� ���Ⱑ �߻��ϸ� WorkerThread ���� GetQueuedCompletion~�� Ǯ���� ��. 
*/
class IOCPServer
{
public:
	IOCPServer() = default;
	
	virtual ~IOCPServer()
	{
		//������ ����� ������.
		WSACleanup();		
	}

	//������ �ʱ�ȭ�ϴ� �Լ�
	bool Init(const UINT32 maxIOWorkerThreadCount_)
	{
		WSADATA wsaData;
		
		int nRet = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (0 != nRet)
		{
			printf("[����] WSAStartup()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		//���������� TCP , Overlapped I/O ������ ����
		mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);

		if (mListenSocket == INVALID_SOCKET)
		{
			printf("[����] socket()�Լ� ���� : %d\n", WSAGetLastError());
			return false;
		}

		MaxIOWorkerThreadCount = maxIOWorkerThreadCount_;

		printf("���� �ʱ�ȭ ����\n");
		return true;
	}
		
	//������ �ּ������� ���ϰ� �����Ű�� ���� ��û�� �ޱ� ���� ������ ����ϴ� �Լ�
	bool BindandListen(int nBindPort)
	{
		SOCKADDR_IN		stServerAddr;
		stServerAddr.sin_family = AF_INET;
		stServerAddr.sin_port = htons(nBindPort); //���� ��Ʈ�� �����Ѵ�.		
		//� �ּҿ��� ������ �����̶� �޾Ƶ��̰ڴ�.
		//���� ������� �̷��� �����Ѵ�. ���� �� �����ǿ����� ������ �ް� �ʹٸ�
		//�� �ּҸ� inet_addr�Լ��� �̿��� ������ �ȴ�.
		stServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		//������ ������ ���� �ּ� ������ cIOCompletionPort ������ �����Ѵ�.
		if (bind(mListenSocket, (SOCKADDR*)&stServerAddr, sizeof(SOCKADDR_IN)) != 0)
		{
			return false;
		}

		//���� ��û�� �޾Ƶ��̱� ���� cIOCompletionPort������ ����ϰ� 
		//���Ӵ��ť�� 5���� ���� �Ѵ�.

		if (listen(mListenSocket, 5) != 0)
		{
			return false;
		}

		//CompletionPort��ü ���� ��û�� �Ѵ�.
		mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, MaxIOWorkerThreadCount);

		// IOCP�� �����ϴ� �κ��� �ƴ϶�, IOCP�� �� SOCKET�� �����ϴ� �κ��̴�. 
		auto iocpHandle = CreateIoCompletionPort((HANDLE)mListenSocket, mIOCPHandle, (UINT32)0, 0);
		if (iocpHandle == nullptr)
		{
			return false;
		}

		return true;
	}

	//���� ��û�� �����ϰ� �޼����� �޾Ƽ� ó���ϴ� �Լ�
	bool StartServer(const UINT32 maxClientCount)
	{
		CreateClient(maxClientCount);
		
		//CreateAccepterThread, CreateWokerThread ���� �ٲ����ν� ��Ʈ����.. 

		CreateAccepterThread();

		//���ӵ� Ŭ���̾�Ʈ �ּ� ������ ������ ����ü
		CreateWokerThread();
		return true;
	}

	//�����Ǿ��ִ� �����带 �ı��Ѵ�.
	void DestroyThread()
	{
		mIsWorkerRun = false;
		CloseHandle(mIOCPHandle);
		
		for (auto& workerThread : mIOWorkerThreads)
		{
			if (workerThread.joinable())
			{
				workerThread.join();
			}
		}
		
		//Accepter �����带 �����Ѵ�.
		isAccepterRun = false;
		closesocket(mListenSocket);
		
		if (mAccepterThread.joinable())
		{
			mAccepterThread.join();
		}		
	}

	bool SendMsg(const UINT32 sessionIndex_, const UINT32 dataSize_, char* pData)
	{
		auto pClient = GetClientInfo(sessionIndex_);
		return pClient->SendMsg(dataSize_, pData);
	}
	
	virtual void OnConnect(const UINT32 clientIndex_) {}

	virtual void OnClose(const UINT32 clientIndex_) {}

	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) {}

private:
	/*
	* �̸� ������ �θ� ������尡 �پ���. 
	*/
	void CreateClient(const UINT32 maxClientCount)
	{
		for (UINT32 i = 0; i < maxClientCount; ++i)
		{
			auto client = new ClientInfo;
			client->Init(i, mIOCPHandle);

			mClientInfos.push_back(client);
		}
	}

	//WaitingThread Queue���� ����� ��������� ����
	void CreateWokerThread()
	{
		unsigned int uiThreadId = 0;
		//WaingThread Queue�� ��� ���·� ���� ������� ���� ����Ǵ� ���� : (cpu���� * 2) + 1 
		
		printf("[CreateWokerThread] : %d", MaxIOWorkerThreadCount);

		for (int i = 0; i < MaxIOWorkerThreadCount; i++)
		{
			mIOWorkerThreads.emplace_back([this](){ WokerThread(); });			
		}

		printf("WokerThread ����..\n");
	}
	
	//������� �ʴ� Ŭ���̾�Ʈ ���� ����ü�� ��ȯ�Ѵ�.
	ClientInfo* GetEmptyClientInfo()
	{
		for (auto& client : mClientInfos)
		{
			if (client->IsConnectd() == false)
			{
				return client;
			}
		}

		return nullptr;
	}

	/*
	* clientSession�� �̸� �����δ�, sessionIndex�� ���� Client ������ ������ �� �� �ִ� 
	*/
	ClientInfo* GetClientInfo(const UINT32 sessionIndex)
	{
		return mClientInfos[sessionIndex];		
	}


	// CreateAccepterThread�� WorkerThread�� ���� �δ� ���� 
	/*
	* Accept�� �� ���Ḹ ����ϵ��� �и��ϰ� �̿ܿ� IO�� ��Ʈ��ũó���κе��� WorkertThread�� �и��ϸ�
	* ȿ�����ִ�. 
	*/
	//accept��û�� ó���ϴ� ������ ����
	bool CreateAccepterThread()
	{
		mAccepterThread = std::thread([this]() { AccepterThread(); });
		
		printf("AccepterThread ����..\n");
		return true;
	}
		  		
	//Overlapped I/O�۾��� ���� �Ϸ� �뺸�� �޾� �׿� �ش��ϴ� ó���� �ϴ� �Լ�
	void WokerThread()
	{
		//CompletionKey�� ���� ������ ����
		ClientInfo* pClientInfo = nullptr;
		//�Լ� ȣ�� ���� ����
		BOOL bSuccess = TRUE;
		//Overlapped I/O�۾����� ���۵� ������ ũ��
		DWORD dataTransferred = 0;
		//I/O �۾��� ���� ��û�� Overlapped ����ü�� ���� ������
		LPOVERLAPPED lpOverlapped = NULL;

		while (mIsWorkerRun)
		{
			/*
			* ���⿡ ��� ������ �ð��� ���ٸ� �ý����� ������ ������ GetQueuedCompletionStatus�� �װ��� ����
			* ����
			*/
			// ���⼭ GetQueuedCompletionStatus iocp�� Ư���� ���δ�. 
			// While�� ��� ���鼭 GetQueuedCompletionStatus�� ȣ���ϴ� ���� �ƴ϶�, 
			// GetQueuedCompletionStatus�� ���� �Ϸ�� �̺�Ʈ�� �ִ� ���� �ƴ϶�� �ش� �Լ� ȣ��ο� 
			// blocking �Լ�ó�� ����ϰ��־ while�ȿ� ���� thread sleep ������ �ʿ䰡 ����. 

			/*
			* dataTransferred -> WSARecv�� ���Ͽ� �߻��� ���̶�� �о���� ��,
			* WSASend�� ���Ͽ� �߻��� ���̶�� ������ ����Ʈ ���� ����Ų��. 
			*/

			/*
			* CompletionKey
			* ->  ��������� �����Ͽ��� ���� �ѱ� �� CreateIOCompletion... 
			* -> 
			*/
			bSuccess = GetQueuedCompletionStatus(mIOCPHandle,
				&dataTransferred,					// ������ ���۵� ����Ʈ
				(PULONG_PTR)&pClientInfo,		// CompletionKey
				&lpOverlapped,				// Overlapped IO ��ü
				INFINITE);					// ����� �ð�

			auto overlappedEx = (OverlappedEX*)lpOverlapped;

			//========================== �߰��� �Լ� ===================
			//==========================BattleNetGameServer=====================

			//Accpet�� ���� ������ �Ʒ� ����ó���Ϸ��� AcceptThread�� ���� �и��ؾ��� 
			//if (lpOverlapped != nullptr && dataTransferred == 0)
			//{
			//	//������ ���� , recv �� ���� 0�� �̻��� ���̰� send�� ���� ��Ŷ�� �Ⱥ��� ���� ���� �ȵȴ� 
			//	//closeSocket. 
			//	
			//	//EnqueueClose(pClientInfo)
			//	CloseSocket(pClientInfo);
			//}


			//recv �� 0byte�� ��� ó�� 
			//clienetsession ���� 
			if (overlappedEx->m_eOperation == IOOperation::RECV && dataTransferred == 0)
			{
				std::cout << "zero byte recv -> close socket" << std::endl;
				CloseSocket(pClientInfo);
			}

			// GetQueuedCompletionStatus���� �˰Ե� m_eOperation ��û ���� 
			switch (overlappedEx->m_eOperation)
			{
				// Accept�� Worker���� AcceptThread���� ����ؼ� ó���ϴ� ������ ���� 
				// 
			case IOOperation::ACCEPT:
				//miocphandle�� �� ��������  ?.. 
				pClientInfo = GetClientInfo(overlappedEx->SessionIndex);

				if (pClientInfo->AcceptCompletion())
				{
					InterlockedIncrement(&clientCount);
					OnConnect(pClientInfo->GetIndex());
				}
				else
				{
					std::cout << "AcceptCompletion ����" << std::endl;

					CloseSocket(pClientInfo, true);
				}
				break;

			case IOOperation::RECV:
				OnReceive(pClientInfo->GetIndex(), dataTransferred, pClientInfo->RecvBuffer());
				
				//================= BattleNetGameServer===================
				// Recv �� ������ �б⸦ ���� �񵿱� �б� �õ� ..
				// PostRecv

				pClientInfo->BindRecv();
				break;

			case IOOperation::SEND:
				//================= BattleNetGameServer===================
				// Send �� ������ �б⸦ ���� �񵿱� �б� �õ� ..
				// 

				//�ش� �κп��� �̷������� ���� 
				/*
				* TCP Ư���� �ϳ��� ��Ŷ������ �����Ͱ� �̵��ϴ� ���� �ƴ϶�.. 
				* ��Ȯ�ϳ��� ��Ŷ�� ���۵ǵ��� üũ�ϴ� �κ��� �̷������Ѵ�. �׺κ��� SendCompleted..
				* ���ϴ� ����Ʈ ����, ������ ����Ʈ �� Ȯ��
				*/
				pClientInfo->SendCompleted(dataTransferred);

				//PostSendReset(pClientInfo, dataTransferred)
				break;

			default:
				printf("Client Index(%d)���� ���ܻ�Ȳ\n", pClientInfo->GetIndex());
				break;
			}

		}
	}

	//������� ������ �޴� ������
	void AccepterThread()
	{
		while (isAccepterRun)
		{
			for (auto client : mClientInfos)
			{
				if (client->IsConnectd())
				{
					continue;
				}

				client->PostAccept(mListenSocket);
			}
			
			std::this_thread::sleep_for(std::chrono::milliseconds(32));
		}
	}

	
	//������ ������ ���� ��Ų��.
	void CloseSocket(ClientInfo* pClientInfo, bool bIsForce = false)
	{
		auto clientIndex = pClientInfo->GetIndex();

		pClientInfo->Close(bIsForce);
		
		OnClose(clientIndex);
	}



	UINT32 MaxIOWorkerThreadCount = 0;

	//Ŭ���̾�Ʈ ���� ���� ����ü
	std::vector<ClientInfo*> mClientInfos;

	//Ŭ���̾�Ʈ�� ������ �ޱ����� ���� ����
	SOCKET		mListenSocket = INVALID_SOCKET;
	
	//���� �Ǿ��ִ� Ŭ���̾�Ʈ ��
	long			clientCount = 0;
	
	//IO Worker ������
	std::vector<std::thread> mIOWorkerThreads;

	//Accept ������
	std::thread	mAccepterThread;

	//CompletionPort��ü �ڵ�
	HANDLE		mIOCPHandle = INVALID_HANDLE_VALUE;
	
	//�۾� ������ ���� �÷���
	bool		mIsWorkerRun = true;

	//���� ������ ���� �÷���
	bool		isAccepterRun = true;
};