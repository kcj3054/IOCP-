#include "pch.h"
#include "Lock.h"

void Lock::WriteLock()
{
	//�ƹ��� ���� �� �����ϰ� ���� ���� �� �����ؼ� �������� ��´�. 
	//_lockFlag.load() & WRITE_THREAD_MASK -> ���� 16bit�� 0���� �а� 
	// FFF�� _lockFlag�� ��� �ɱ� ?
	// 
	const int lockTrheadId = (_lockFlag.load() & WRITE_THREAD_MASK) >> 16;
	
	//���� ������ ID�� �ٽ� WRITE LOCK�� �� �� ++�ϸ鼭 ����Ѵ� 
	/*
	* if(LThreadId == lockTrheadId) _writeCount++;
	*/

	//�ƹ��� ���� �� �����ϰ� ���� ���� �� �����ؼ� �������� ��´�.
	const int beginTick = ::GetTickCount64();
	// const int desired = ()
}

void Lock::WriteUnLock()
{

}

void Lock::ReadLock()
{
	//������ �����尡 �����ϰ� �ִٸ� ������ ���� . ->
	//������ �����尡  �����͸� ����, �Ŀ� �д� ���� �ƹ� �ǹ� ���⿡..

	//�ƹ��� �����ϰ� ���� ���� �� (�ƹ��� Write �ϰ����� ���� ��..)�� �����ؼ� ���� ī��Ʈ�� �ø���.
	// 
	while (true)
	{

	}
}

void Lock::ReadUnLock()
{

}
