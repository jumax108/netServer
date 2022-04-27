#pragma once

#include <Windows.h>
#include <intrin.h>

#include "common.h"

#include "protocolBuffer/headers/protocolBuffer.h"
#pragma comment(lib, "lib/protocolBuffer/protocolBuffer")


struct stPacket{

	stPacket():_buffer(PROTOCOL_BUFFER_SIZE){
		_incoded = false;
		_ref = 0;

		#if defined(PACKET_PTR_DEBUG)
			returnAdr = nullptr;
		#endif

	}

	CProtocolBuffer _buffer;
	bool _incoded;
	int _ref;

	void* _packetPtr;

	#if defined(PACKET_PTR_DEBUG)
		void* returnAdr;
	#endif
};

class CPacketPointer{

public:

	CPacketPointer(CPacketPointer& ptr);
	CPacketPointer& operator=(CPacketPointer& ptr);

	CPacketPointer();
	~CPacketPointer();

	inline char* getBufStart(){ 
		return _packet->_buffer.getBufStart();
	}
	inline int getPacketSize(){
		return _packet->_buffer.getUsedSize();
	}
	inline char* getRearPtr(){
		return _packet->_buffer.getRearPtr();
	}
	inline char* getFrontPtr(){
		return _packet->_buffer.getFrontPtr();
	}
	inline void moveRear(int size){
		_packet->_buffer.moveRear(size);
	}
	inline void moveFront(int size){
		_packet->_buffer.moveFront(size);
	}

	bool checkBufferSize(int size);
	inline void setBufferSize(int size){	
		_packet->_buffer.resize(size, false);
	}
	
	void incRef();
	void decRef();

	virtual void incoding(){}
	virtual void decoding(){}
	virtual void setHeader(){}

	void putData(int size, unsigned char* buf);
	void popData(int size, unsigned char* buf);
	
	static unsigned __int64 getPacketPoolUsage();

#pragma region("operator<<")
	CPacketPointer& operator<<(char data);
	CPacketPointer& operator<<(unsigned char data);

	CPacketPointer& operator<<(wchar_t data);
	CPacketPointer& operator<<(short data);
	CPacketPointer& operator<<(unsigned short data);

	CPacketPointer& operator<<(int data);
	CPacketPointer& operator<<(unsigned int data);

	CPacketPointer& operator<<(__int64 data);
	CPacketPointer& operator<<(unsigned __int64 data);

	CPacketPointer& operator<<(float data);
	CPacketPointer& operator<<(double data);
#pragma endregion

#pragma region("operator>>")
	CPacketPointer& operator>>(char& data);
	CPacketPointer& operator>>(unsigned char& data);

	CPacketPointer& operator>>(wchar_t& data);
	CPacketPointer& operator>>(short& data);
	CPacketPointer& operator>>(unsigned short& data);

	CPacketPointer& operator>>(int& data);
	CPacketPointer& operator>>(unsigned int& data);

	CPacketPointer& operator>>(__int64& data);
	CPacketPointer& operator>>(unsigned __int64& data);

	CPacketPointer& operator>>(float& data);
	CPacketPointer& operator>>(double& data);
#pragma endregion

	stPacket* _packet;

public:

	static CObjectFreeListTLS<stPacket> _freeList;

};