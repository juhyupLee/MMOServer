

#include "NetworkSession.h"

#include "Log.h"
#include "MMOGameLib.h"

bool NetworkSession::Disconnect()
{
	_bIOCancel = true;
	IO_Cancel();
	return true;
}

void NetworkSession::IO_Cancel()
{
	//-----------------------------------------------
	// Overlapped Pointer가 NULL일시 Send ,Recv 둘다 IO취소한다
	//-----------------------------------------------
	CancelIoEx((HANDLE)_Socket, NULL);
}

bool NetworkSession::SendPost()
{
	int loopCount = 0;

	do
	{
		loopCount++;

		if (0 == InterlockedExchange(&_SendFlag, 1))
		{
			//--------------------------------------------------------
			// Echo Count가 증가한 범인
			//--------------------------------------------------------
			if (_SendQ.GetQCount() <= 0)
			{
				InterlockedExchange(&_SendFlag, 0);
				continue;
			}
			//--------------------------------------------------
			// IOCount와 이세션이 WSARecv or WSASend 이후 로그를 위해 Session에 접근할수있기 때문에
			// 참조카운트용으로 하나 더 증가시킨다.
			//--------------------------------------------------
			InterlockedAdd((LONG*)&_IOCount, 2);
			//--------------------------------------------------

			if (_IOCount <= 0)
			{
				CRASH();
			}

			if (_SessionStatus == eSessionStatus::RELEASE)
			{
				CRASH();
			}

			////-----------------------------------------------------------------------------------------------------------------------
			//// SendQ에 있는 LanPackt* 포인터들을 뽑아서 WSABUF를 세팅해준다
			////-----------------------------------------------------------------------------------------------------------------------
			WSABUF wsaSendBuf[NetworkSession::DEQ_PACKET_ARRAY_SIZE];

			int bufCount = 0;

			NetPacket* deQPacket = nullptr;

			if (_DeQArraySize > 0)
			{
				CRASH();

			}
			while (_SendQ.DeQ(&deQPacket))
			{
				if (deQPacket == nullptr)
				{
					CRASH();
				}
				if (_DeQArraySize > NetworkSession::DEQ_PACKET_ARRAY_SIZE - 1)
				{
					CRASH();
				}

				if (deQPacket->GetPayloadSize() <= 0)
				{
					CRASH();
				}
				wsaSendBuf[_DeQArraySize].buf = deQPacket->GetBufferPtr();
				wsaSendBuf[_DeQArraySize].len = deQPacket->GetFullPacketSize();
				_SendByte += wsaSendBuf[_DeQArraySize].len;

				_DeQPacketArray[_DeQArraySize] = deQPacket;
				_DeQArraySize++;
			}
			//------------------------------------------------------
			//   Send 송신바이트 체크하기
			//------------------------------------------------------
			if (_SendByte <= 0)
			{
				CRASH();
			}

			ZeroMemory(&_SendOL, sizeof(_SendOL));

			if (_IOCount <= 0)
			{
				CRASH();
			}

			//------------------------------------------------------------------
			// 	IO Cancel 이 실행됬다면, 입출력을 걸지않고, IOCount를 낮추고 Return한다
			//  로그를위한 IOCount +1  WSASend를 위한 +1 
			//------------------------------------------------------------------
			if (_bIOCancel)
			{
				for (int i = 0; i < 2; ++i)
				{
					if (0 == InterlockedDecrement(&_IOCount))
					{
						_bReleaseReady = true;
					}
				}

				return false;
			}


			int sendRtn = WSASend(_Socket, wsaSendBuf, _DeQArraySize, NULL, 0, &_SendOL, NULL);

			if (_IOCount <= 0)
			{
				CRASH();
			}

			if (sendRtn == SOCKET_ERROR)
			{
				int errorCode = WSAGetLastError();

				if (errorCode != WSA_IO_PENDING)
				{
					_IOFail = true;
					MMOGameLib::SpecialErrorCodeCheck(errorCode);

					//---------------------------------------------------------
					// WSASend를 걸기위해 증가시킨 IOCount를 감소시킨다.
					//---------------------------------------------------------
					int tempIOCount = InterlockedDecrement(&_IOCount);

					if (0 == tempIOCount)
					{
						_bReleaseReady = true;
					}
				}
			}

			//---------------------------------------------------------
			// Log를 위해 올렷던 IOCount를 감소시키고 끝낸다. (Return)
			//---------------------------------------------------------
			int tempIOCount = InterlockedDecrement(&_IOCount);
			if (0 == tempIOCount)
			{
				_bReleaseReady = true;
			}
			return true;
		}
		else
		{
			break;
		}


	} while (_SendQ.GetQCount() > 0);

	return true;

}

//bool NetworkSession::SendPacket(NetPacket* packet)
//{
//	(*packet).HeaderSettingAndEncoding();
//
//	if (packet->GetPayloadSize() <= 0)
//	{
//		CRASH();
//	}
//	if (!_SendQ.EnQ(packet))
//	{
//		_LOG->WriteLog(SERVER_NAME, SysLog::eLogLevel::LOG_LEVEL_ERROR, L"SendQ 총갯수 초과(LockFreeQ Qcount 초과함)");
//		CRASH();
//	}
//
//	return true;
//}
//
//void NetworkSession::SendUnicast(NetPacket* packet)
//{
//	packet->IncrementRefCount();
//
//	if (!SendPacket(packet))
//	{
//		if (packet->DecrementRefCount() == 0)
//		{
//			packet->Free(packet);
//		}
//		return;
//	}
//
//	if (packet->DecrementRefCount() == 0)
//	{
//		packet->Free(packet);
//	}
//}