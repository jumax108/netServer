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

	// 필요한 메모리를 할당하고 서버를 시작합니다.
	void start(const wchar_t* serverIP, unsigned short serverPort,
			int createWorkerThreadNum, int runningWorkerThreadNum,
			unsigned short maxSessionNum, bool onNagle,
			unsigned int sendBufferSize, unsigned int recvBufferSize); 
	// 모든 메모리를 정리하고 서버를 종료합니다.
	void stop(); 
	
	// sessionID 에 해당하는 연결 해제합니다.
	void disconnect(unsigned __int64 sessionID); 
	// sessinoID 에 해당하는 세션에 데이터 전송합니다.
	bool sendPacket(unsigned __int64 sessionID, CPacketPtr_Net packet);
	 
	// 클라이언트가 접속을 시도한 상태에서 호출됩니다.
	// 반환된 값에 따라 연결을 허용합니다.
	// return true = 연결 후, 세션 초기화
	// return false = 연결을 끊음
	virtual bool onConnectRequest(unsigned int ip, unsigned short port) = 0;
	// 클라이언트가 접속을 완료한 상태에서 호출됩니다.
	virtual void onClientJoin(unsigned int ip, unsigned short port, unsigned __int64 sessionID) = 0;
	// 클라이언트의 연결이 해제되면 호출됩니다.
	virtual void onClientLeave(unsigned __int64 sessionID) = 0;

	// 클라이언트에게 데이터를 전송하면 호출됩니다.
	virtual void onRecv(unsigned __int64 sessionID, CPacketPointer pakcet) = 0;
	// 클라이언트에게서 데이터를 전달받으면 호출됩니다.
	virtual void onSend(unsigned __int64 sessionID, int sendSize) = 0;

	// 에러 상황에서 호출됩니다.
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
	// 현재 접속중인 세션 수를 반환합니다.
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

	// thread 정리용 event
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

		// ID의 하위 6바이트는 세션 메모리에 대한 재사용 횟수
		// 상위 2바이트는 세션 인덱스
		unsigned __int64 _sessionID; // 서버 가동 중에는 고유한 세션 ID

		CLockFreeQueue<CPacketPointer> _sendQueue;
		CRingBuffer _recvBuffer;
		
		OVERLAPPED _sendOverlapped;
		OVERLAPPED _recvOverlapped;

		CPacketPointer* _packets;

		SOCKET _sock;
	
		int _packetCnt;

		unsigned int _ip;
		unsigned short _port;

		// send를 1회로 제한하기 위한 플래그
		bool _isSent;
		
		// 총 4바이트로 릴리즈 플래그 변화와 ioCnt가 0인지 동시에 체크하기 위함
		alignas(32) bool _released;
		private: unsigned char _dummy;
		public: unsigned short _ioCnt;

		bool _callDisconnect;



	};
};