#include "GameServer.h"
#include <string>
#include <iostream>



const UINT16 SERVER_PORT = 7700;
const UINT16 MAX_CLIENT = 100;		//총 접속할수 있는 클라이언트 수
const UINT32 MAX_IO_WORKER_THREAD = 4;  //쓰레드 풀에 넣을 쓰레드 수

int main()
{

	GameServer server;

	//소켓을 초기화
	server.Init(MAX_IO_WORKER_THREAD);

	//소켓과 서버 주소를 연결하고 등록 시킨다.
	server.BindandListen(SERVER_PORT);

	server.Run(MAX_CLIENT);
	while (true)
	{
		;
	}
	return 0;
}

