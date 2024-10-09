#include "../pch.h"
#include "PaketManager.h"

// #include "../RedisManager.h"

PaketManager::~PaketManager()
{
	isRunProcessThread = false;

	if (processThread.joinable())
	{
		processThread.join();
	}
}

void PaketManager::Init(const int maxClinet)
{
	//Handler ���
	// recvFunction

	//User������ ���� 
	// CreateComponent -> client
	recvFunctionDictionary[(int)PacketID::LOGIN_REQUEST] = &PaketManager::ProcessUserConnect;


	isRunProcessThread = true;
	processThread = std::thread(&PaketManager::ProcessPacket, this);


	//===================Redis=====================================
	// _redisManager = std::make_unique<RedisManager>();
}

bool PaketManager::Run()
{
	//RedisManager ���� , todo : �ϵ� �ڵ� �κ� �����ϱ� 
	// _redisManager->Init("127.0.0.1", 6379, 10);

	return false;
}

void PaketManager::ReceivePacketData(const unsigned int clientIndex, const unsigned int size, char* data)
{
	//RingBuffer�� �̿��Ͽ� Packet���� �� Queue�� Packet�� ���� 
	// -> UserManager�鵵 Packet�� ������ �ְ�, auto pUser -> SetPacketData()�� ��Ŷ���� ����

	//todo : ���� ���⼭ lock�� �� �ʿ䰡 ������? ... -> Engine�ܿ��� lock�� ����ϴ� ���� ��õ 
	EnqueuePacketData(clientIndex);
}

void PaketManager::PushSystemPacket(PacketInfo packet)
{

}

void PaketManager::CreateClient()
{
	// UserManager ���� �� -> UserManager Init;;
}

//�ش� �Լ��� Enqueue�� ��Ŷ�� ������ �ϼ��� ��Ŷ 
void PaketManager::EnqueuePacketData(const int clientIndex)
{
	std::lock_guard<std::mutex> lockGuard(lock);

}

PacketInfo PaketManager::DequePacketData()
{
	return PacketInfo();
}

void PaketManager::ProcessPacket()
{
	while (isRunProcessThread)
	{
		//redis������ db�� ��Ŷ ó������, �Ϲ� ��Ʈ��ũ Packe ó������ ������ �߿��ϴ� 
		if (auto packetData = DequePacketData(); packetData.packetID != 0)
		{
			//���⼭ �ٷ� recvFunctionDictionary.find�� ã�� �͵� ���� �� 
			//end�����ϴ� packet�̶�� �ٷ� ���� 

			if (auto contentsPacket = recvFunctionDictionary.find(packetData.packetID); contentsPacket != recvFunctionDictionary.end())
			{
				contentsPacket->second(*this, packetData.cliendtIndex, packetData.dataszie, packetData.packet);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}

void PaketManager::ProcessUserConnect(UINT32 clientIndex, UINT16 packetSize, char* packet)
{

}
