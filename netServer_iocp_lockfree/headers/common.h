#pragma once

#pragma pack(1)
struct stHeader{
	unsigned char code;
	unsigned short size;
	unsigned char randKey;
	unsigned char checkSum;
};
#pragma pack()


//#define PACKET_PTR_LAN_DEBUG

namespace netServer {

	constexpr int MAX_PACKET = 100;

	constexpr unsigned __int64 SESSION_INDEX_MASK = 0x000000000000FFFF;
	constexpr unsigned __int64 SESSION_ALLOC_COUNT_MASK = 0xFFFFFFFFFFFF0000;

	constexpr unsigned char PACKET_KEY = 0x32;
};