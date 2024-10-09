#pragma once
/*
*  RW SPINLOCK
* 
* ->32bit flag�� �̿��Ѵ�.
* 
* [WWWWWWWW][WWWWWWWW][RRRRRRRR][RRRRRRRR]
* 
* W : Write Flag (Exclusive Lock Owner ThreadID)
* R : ReadFlag (shared Lock Count)
*/

/*
* // ���� �����尡 Write�� �� �Ŀ��� Read�� �� �� �ִ�
* Write�� ���� ���¿��� Read�� ���� ���� �ִ�.
*	-> Write�� ���� ���¿��� 
* 
* 
* Read�� ���� ���¿��� Write�� ���� ���� ����. // Read�� ��Ҵٴ� ���� ���� �����尡 Read�� �ϰ��ִ� ��Ȳ �� �����ִ� �̷��� ���¿��� 
* Write�� �Ѵٴ� ���� ���� ���� ��ų �� �ִ�. , �ִ��� ���� ���� ���̶�� W->R�� ���� ���� �´� 
*/
#include <atomic>

class Lock
{
public:
	enum : unsigned int
	{
		ACQUIRE_TIMEOUT_TIC = 10000,
		MAX_SPIN_COUNT = 5000,
		WRITE_THREAD_MASK = 0xFFFF'0000,
		READ_COUNT_MASK = 0xFFFF'0000,
		EMPTY_FLAG = 0x0000'0000
	};

public:
	void WriteLock();
	void WriteUnLock();
	void ReadLock();
	void ReadUnLock();

private:
	std::atomic<unsigned int> _lockFlag = EMPTY_FLAG;
	int _writeCount = 0;
};

