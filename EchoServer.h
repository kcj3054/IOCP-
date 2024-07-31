#pragma once



#include "IOCPServer.h"
#include "Packet.h"
#include "packetType.h"

#include <concurrent_queue.h>

#include <functional>
#include <unordered_map>
#include <vector>
#include <deque>
#include <thread>
#include <mutex>


class EchoServer : public IOCPServer
{
public:
	EchoServer() = default;
	virtual ~EchoServer() = default;
	
	void RegisterPackets()
	{
		/*mPacketHandlers[PACKET_TYPE_LOGIN] = [this](const PacketData& packet) { HandleLogin(packet); };
		mPacketHandlers[PACKET_TYPE_LOGOUT] = [this](const PacketData& packet) { HandleLogout(packet); };
		mPacketHandlers[PACKET_TYPE_MESSAGE] = [this](const PacketData& packet) { HandleMessage(packet); };*/
	}

	virtual void OnConnect(const UINT32 clientIndex_) final
	{
		printf("[OnConnect] Ŭ���̾�Ʈ: Index(%d)\n", clientIndex_);
	}

	virtual void OnClose(const UINT32 clientIndex_) final 
	{
		printf("[OnClose] Ŭ���̾�Ʈ: Index(%d)\n", clientIndex_);
	}
	
	//������ ���� ��Ŷ�� ���� ������ ���� ����.. 
	
	//
	virtual void OnReceive(const UINT32 clientIndex_, const UINT32 size_, char* pData_) final
	{
		printf("[OnReceive] Ŭ���̾�Ʈ: Index(%d), dataSize(%d)\n", clientIndex_, size_);

		PacketData packet;
		packet.Set(clientIndex_, size_, pData_);

		mPacketDataQueue.push(packet); // ConcurrentQueue�� ����Ͽ� lock�κ� ���� 

		// std::lock_guard<std::mutex> guard(mLock);
		// mPacketDataQueue.push_back(packet);
	}

	/*
	* Packetó���� ������� IO ��� ��������� �и�.
	*/

	void Run(const UINT32 maxClient)
	{
		mIsRunProcessThread = true;
		mProcessThread = std::thread([this]() { ProcessPacket(); });

		StartServer(maxClient);
	}

	void End()
	{
		mIsRunProcessThread = false;

		if (mProcessThread.joinable())
		{
			mProcessThread.join();
		}

		DestroyThread();
	}

private:
	void ProcessPacket()
	{
		while (mIsRunProcessThread)
		{
			auto packetData = DequePacketData();
			if (packetData.DataSize != 0)
			{
				SendMsg(packetData.SessionIndex, packetData.DataSize, packetData.pPacketData);
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	//concurrentqueue ��� 
	PacketData DequePacketData()
	{
		PacketData packetData;
		if (mPacketDataQueue.try_pop(packetData))
		{
			return packetData;
		}
		else
		{
			return PacketData();
		}
	}

	/*PacketData DequePacketData()
	{
		PacketData packetData;

		std::lock_guard<std::mutex> guard(mLock);
		if (mPacketDataQueue.empty())
		{
			return PacketData();
		}

		packetData.Set(mPacketDataQueue.front());

		mPacketDataQueue.front().Release();
		mPacketDataQueue.pop_front();

		return packetData;
	}*/

private:

	bool mIsRunProcessThread = false;

	std::thread mProcessThread;

	std::mutex mLock;
	
	// std::deque<PacketData> mPacketDataQueue;
	concurrency::concurrent_queue<PacketData> mPacketDataQueue;

	std::unordered_map<int, std::function<void(const PacketData&)>> mPacketHandlers;
};