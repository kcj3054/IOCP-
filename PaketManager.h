#pragma once

#include <Windows.h>

#include "Packet.h"

#include <functional>
#include <thread>
#include <mutex>

#include <concurrent_queue.h>

class PaketManager
{
public:
	PaketManager() = default;
	~PaketManager();

	void Init(const int maxClinet);

	bool Run();

	void ReceivePacketData(const unsigned int clientIndex, const unsigned int size, char* data);

	void PushSystemPacket(PacketInfo packet);


private:
	void CreateClient();

	void EnqueuePacketData(const int clientIndex);

	PacketInfo DequePacketData();

	void ProcessPacket();

private:
	
	void ProcessUserConnect(UINT32 clientIndex, UINT16 packetSize, char* packet);

private:

	std::mutex lock;
	bool isRunProcessThread = false;
	std::thread processThread;

	using PROCESS_RECV_PACKET_FUNCTION = std::function<void(PaketManager&, UINT32, UINT16, char*)>; 

	std::unordered_map<int, PROCESS_RECV_PACKET_FUNCTION> recvFunctionDictionary;

	/*
	* 	std::deque<UINT32> mInComingPacketUserIndex;
	std::deque<PacketInfo> mSystemPacketQueue;
	�������� �ΰ��� �� ���������� �ϳ��� PacketQueue�� �ϸ� �� �ܼ������������� ?. .
	*/

	concurrency::concurrent_queue<UINT32> comingPacketUserIndex;
};

