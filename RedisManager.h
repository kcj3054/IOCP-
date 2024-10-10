#pragma once

#include "pch.h"

#include "thirdparty/CRedisConn.h"

#include "RedisDefine.h"

class RedisManager
{
public:
	void Init(std::string ip, uint32_t port, uint32_t threadCount)
	{
		//Redis ���� 
		if (_connection.connect(ip, port) == false)
		{
			std::cout << "Redis connect ����" << std::endl;
			
			return;
		}

		//atomic
		_isRun = true;

		//Redis���� Task ó�� �� WorkerThread ���� 
		for (int i = 0; i < threadCount; ++i)
		{
			// ĸó�� this! 
			workerThreads.emplace_back([this]() {
				TaskProcessThread();
				});
		}
	}

private:
	
	// Redis���� Task ó�� �� WorkerThread
	void TaskProcessThread()
	{
		std::cout << "Redis worker Thread ����\n"; 

		while (_isRun)
		{
			if (auto optTask = TakeRequestTask(); optTask.has_value())
			{
				auto task = optTask.value();

				// switch case�� if���� �ƴ� unordered_map���� ���� ���̰� ����� ���� ���⿡ ������ 
				// if(task.packetID == )
			
				//todo ���⼭ invoke.! 
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	std::optional<RedisTask> TakeRequestTask()
	{
		std::lock_guard guard(_reqLock);

		if (_requestTask.empty())
		{
			return std::nullopt;
		}
		
		auto task = _requestTask.front();
		_requestTask.pop_front();

		return task;
	}

	void PushResponse(RedisTask task)
	{
		std::scoped_lock<std::mutex> guard(_resLock);

		_responseTask.push_back(task);
	}
	
private:

	//���� Redis�� WorkerThread�� ���� ���� bool 
	std::atomic<bool>	_isRun{ false };

	// thread -> jthread ���  join (x)
	std::vector<std::jthread> workerThreads;

	std::mutex _reqLock;
	std::mutex _resLock;
	std::deque<RedisTask> _requestTask;
	std::deque<RedisTask> _responseTask;

	//hiRedis
	RedisCpp::CRedisConn _connection;

	//redis Task�� ���� std::function 
	std::unordered_map <RedisPacketID, std::is_function<void(RedisTask&)>> _taskCallback;
};

