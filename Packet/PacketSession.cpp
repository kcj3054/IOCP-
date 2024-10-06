#include "PacketSession.h"

void PacketSession::RegisteHandler(int packetID, PacketHandler handler)
{
	_handlers[packetID] = handler;
}

/*
* �� ��Ȯ�� ������ ��Ŷ�� ���� �� OnRecvPacket�� ȣ�� ��
*/
int PacketSession::OnRecvPacket(char* buffer, int len)
{
    int32_t packetId = 0 /* ��Ŷ ID ���� ���� */;
    auto iter = _handlers.find(packetId);
    if (iter != _handlers.end())
    {
        iter->second(shared_from_this(), buffer, len);
    }
    else
    {
        // �⺻ ó�� ����
    }

    return 0;
}
