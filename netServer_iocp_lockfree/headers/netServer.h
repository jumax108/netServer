#pragma once

#include <WinSock2.h>
#pragma comment(lib,"ws2_32")
#include <WS2tcpip.h>

#include <thread>
#include <new>
#include <windows.h>	

///////////////////////////////////////////////////////////////////
// lib
#include "objectFreeListTLS/headers/objectFreeListTLS.h"
#include "dump/headers/dump.h"
#include "log/headers/log.h"
#include "protocolBuffer/headers/protocolBuffer.h"
#include "packetPointer/headers/packetPointer.h"
#include "lockFreeStack/headers/lockFreeStack.h"
#include "lockFreeQueue/headers/lockFreeQueue.h"
#include "ringBuffer/headers/ringBuffer.h"

#pragma comment(lib, "lib/dump/dump")
#pragma comment(lib, "lib/log/log")
#pragma comment(lib, "lib/protocolBuffer/protocolBuffer")
#pragma comment(lib, "lib/packetPointer/packetPointer")
#pragma comment(lib, "lib/ringBuffer/ringBuffer")
///////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////
// header
#include "common.h"
#include "packetPointer_NetServer.h"
///////////////////////////////////////////////////////////////////


class CNetServer{

	struct stSession;

public:

	CNetServer();

	// �ʿ��� �޸𸮸� �Ҵ��ϰ� ������ �����մϴ�.
	void start(const wchar_t* serverIP, unsigned short serverPort,
			int createWorkerThreadNum, int runningWorkerThreadNum,
			unsigned short maxSessionNum, bool onNagle,
			unsigned int sendBufferSize, unsigned int recvBufferSize); 
	// ��� �޸𸮸� �����ϰ� ������ �����մϴ�.
	void stop(); 
	
	// sessionID �� �ش��ϴ� ���� �����մϴ�.
	void disconnect(unsigned __int64 sessionID); 
	// sessinoID �� �ش��ϴ� ���ǿ� ������ �����մϴ�.
	bool sendPacket(unsigned __int64 sessionID, CPacketPtr_Net packet);
	 
	// Ŭ���̾�Ʈ�� ������ �õ��� ���¿��� ȣ��˴ϴ�.
	// ��ȯ�� ���� ���� ������ ����մϴ�.
	// return true = ���� ��, ���� �ʱ�ȭ
	// return false = ������ ����
	virtual bool onConnectRequest(unsigned int ip, unsigned short port) = 0;
	// Ŭ���̾�Ʈ�� ������ �Ϸ��� ���¿��� ȣ��˴ϴ�.
	virtual void onClientJoin(unsigned int ip, unsigned short port, unsigned __int64 sessionID) = 0;
	// Ŭ���̾�Ʈ�� ������ �����Ǹ� ȣ��˴ϴ�.
	virtual void onClientLeave(unsigned __int64 sessionID) = 0;

	// Ŭ���̾�Ʈ���� �����͸� �����ϸ� ȣ��˴ϴ�.
	virtual void onRecv(unsigned __int64 sessionID, CPacketPointer pakcet) = 0;
	// Ŭ���̾�Ʈ���Լ� �����͸� ���޹����� ȣ��˴ϴ�.
	virtual void onSend(unsigned __int64 sessionID, int sendSize) = 0;

	// ���� ��Ȳ���� ȣ��˴ϴ�.
	virtual void onError(int errorCode, const wchar_t* errorMsg) = 0;

	inline int getSendTPS(){
		return _sendTPS;
	}
	inline int getRecvTPS(){
		return _recvTPS;
	}
	inline int getAcceptTPS(){
		return _acceptTPS;
	}
	// ���� �������� ���� ���� ��ȯ�մϴ�.
	inline unsigned __int64 getSessionCount(){
		return _sessionCnt;
	}

private:

	static HANDLE _heap;

	stSession* _sessionArr;

	unsigned __int64 _sessionNum;
	unsigned __int64 _sessionCnt;
	CLockFreeStack<int>* _sessionIndexStack;

	HANDLE _acceptThread;

	int _workerThreadNum;
	HANDLE* _workerThread;

	SOCKET _listenSocket;

	HANDLE _iocp;

	int _sendBufferSize;
	int _recvBufferSize;

	// logger
	CLog _log;

	// thread ������ event
	HANDLE _stopEvent;

	HANDLE _tpsCalcThread;
	int _sendCnt;
	int _recvCnt;
	int _acceptCnt;
	int _sendTPS;
	int _recvTPS;
	int _acceptTPS;


	void sendPost(stSession* session);
	void recvPost(stSession* session, bool ignoreReleased = false);

	static unsigned __stdcall completionStatusFunc(void* args);
	static unsigned __stdcall acceptFunc(void* args);
	static unsigned __stdcall tpsCalcFunc(void* args);

	void checkCompletePacket(stSession* session, CRingBuffer* recvBuffer);

	void release(unsigned __int64 sessionID);

	struct stSession{
		
		stSession(unsigned int sendQueueSize, unsigned int recvBufferSize);
		~stSession();

		// ID�� ���� 6����Ʈ�� ���� �޸𸮿� ���� ���� Ƚ��
		// ���� 2����Ʈ�� ���� �ε���
		unsigned __int64 _sessionID; // ���� ���� �߿��� ������ ���� ID

		CLockFreeQueue<CPacketPointer> _sendQueue;
		CRingBuffer _recvBuffer;
		
		OVERLAPPED _sendOverlapped;
		OVERLAPPED _recvOverlapped;

		CPacketPointer* _packets;

		SOCKET _sock;
	
		int _packetCnt;

		unsigned int _ip;
		unsigned short _port;

		// send�� 1ȸ�� �����ϱ� ���� �÷���
		bool _isSent;
		
		// �� 4����Ʈ�� ������ �÷��� ��ȭ�� ioCnt�� 0���� ���ÿ� üũ�ϱ� ����
		alignas(32) bool _released;
		private: unsigned char _dummy;
		public: unsigned short _ioCnt;

		bool _callDisconnect;



	};
};