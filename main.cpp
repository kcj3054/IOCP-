#include "EchoServer.h"
#include <string>
#include <iostream>



const UINT16 SERVER_PORT = 7700;
const UINT16 MAX_CLIENT = 100;		//�� �����Ҽ� �ִ� Ŭ���̾�Ʈ ��
const UINT32 MAX_IO_WORKER_THREAD = 4;  //������ Ǯ�� ���� ������ ��

int main()
{

	EchoServer server;

	//������ �ʱ�ȭ
	server.Init(MAX_IO_WORKER_THREAD);

	//���ϰ� ���� �ּҸ� �����ϰ� ��� ��Ų��.
	server.BindandListen(SERVER_PORT);

	server.Run(MAX_CLIENT);
	while (true)
	{
		;
	}
	return 0;
}

