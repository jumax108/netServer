#pragma once

#include <typeinfo>

#include "objectFreeList\headers\objectFreeList.h"

#include "common.h"

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
#include "log\headers\log.h"
#pragma comment(lib, "lib/log/log")
#endif

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
#define allocObjectTLS() _allocObject(__FILEW__, __LINE__)
#define freeObjectTLS(ptr) _freeObject(ptr, __FILEW__, __LINE__)
#else
#define allocObjectTLS() _allocObject()
#define freeObjectTLS(ptr) _freeObject(ptr)
#endif

template <typename T>
struct stAllocChunk;

template <typename T>
struct stAllocTlsNode;

template <typename T>
class CObjectFreeListTLS {

public:

	CObjectFreeListTLS(bool runConstructor, bool runDestructor);
	~CObjectFreeListTLS();

	T* _allocObject(
#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
		const wchar_t*, int
#endif
	);
	void _freeObject(T* object
#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
		, const wchar_t*, int
#endif
	);

	unsigned int getCapacity();
	unsigned int getUsedCount();

private:

	// 각 스레드에서 활용할 청크를 들고있는 tls의 index
	unsigned int _allocChunkTlsIdx;

	// 모든 스레드가 공통적으로 접근하는 free list 입니다.
	// 이곳에서 T type의 node를 큰 덩어리로 들고옵니다.
	CObjectFreeList<stAllocChunk<T>>* _centerFreeList;

	// T type에 대한 생성자 호출 여부
	bool _runConstructor;

	// T type에 대한 소멸자 호출 여부
	bool _runDestructor;

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	// 청크 리스트 배열의 인덱스를 들고 있는 tls index
	unsigned int _chunkListTlsIdx;

	struct stSimpleList {
		stAllocChunk<T>* _chunk;
		stSimpleList* _next;
	};

	int _chunkListIndex;
	stSimpleList* _chunkListArr[objectFreeListTLS::MAX_THREAD_NUM];

	HANDLE _heap;

	CLog log;
#endif
};

template <typename T>
CObjectFreeListTLS<T>::CObjectFreeListTLS(bool runConstructor, bool runDestructor) {


	///////////////////////////////////////////////////////////////////////
	// 중앙 처리용 freeList 생성
	_centerFreeList = new CObjectFreeList<stAllocChunk<T>>(runConstructor, runDestructor);
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// tls 할당 받음
	_allocChunkTlsIdx = TlsAlloc();
	if (_allocChunkTlsIdx == TLS_OUT_OF_INDEXES) {
		CDump::crash();
	}

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	_chunkListTlsIdx = TlsAlloc();
	if (_chunkListTlsIdx == TLS_OUT_OF_INDEXES) {
		CDump::crash();
	}
#endif
	///////////////////////////////////////////////////////////////////////

	_runConstructor = runConstructor;
	_runDestructor = runDestructor;

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)

	_heap = HeapCreate(0, 0, 0);

	_chunkListIndex = 0;

	for (int chunkListCnt = 0; chunkListCnt < objectFreeListTLS::MAX_THREAD_NUM; ++chunkListCnt) {
		// 청크 리스트를 tls에서 획득하고 저장하는데 계속 null이면 index를 계속 증가시킬 것이기 때문에 더미 노드를 넣어줌
		_chunkListArr[chunkListCnt] = (stSimpleList*)HeapAlloc(_heap, HEAP_ZERO_MEMORY, sizeof(stSimpleList));
	}

	log.setDirectory(L"objectFreeListTLS_Log");
	log.setPrintGroup(LOG_GROUP::LOG_DEBUG);

#endif

}

template <typename T>
CObjectFreeListTLS<T>::~CObjectFreeListTLS() {

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)

	log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"───────────────────────────────────────────");

	wchar_t typeName[50];
	size_t convertSize;
	mbstowcs_s(&convertSize, typeName, typeid(T).name(), 50);
	log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"type: %s", typeName);

	for (int listCnt = 0; listCnt < objectFreeListTLS::MAX_THREAD_NUM; ++listCnt) {
		stSimpleList* list = _chunkListArr[listCnt];
		while (list->_chunk != nullptr) {

			stAllocChunk<T>* chunk = list->_chunk;
			stAllocTlsNode<T>* nodeArr = chunk->_nodes;

			for (int dataCnt = 0; dataCnt < objectFreeListTLS::CHUNK_SIZE; ++dataCnt) {

				stAllocTlsNode<T>* node = &nodeArr[dataCnt];

				if (nodeArr[dataCnt]._allocated == true) {

					log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"alloc file name: %s", node->_allocSourceFileName);
					log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"alloc line: %d", node->_allocLine);
					log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"data address: 0x%016I64X", &node->_data);
					log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"data(%d bytes): ", sizeof(T));

					wchar_t byteLine[25];
					wchar_t* lineWritePoint = byteLine;
					for (int byteCnt = 0; byteCnt < sizeof(T); byteCnt++) {

						swprintf_s(lineWritePoint, 4, L"%02X ", *(((unsigned char*)(&node->_data) + byteCnt)));
						lineWritePoint += 3;

						if ((byteCnt + 1) % 8 == 0 || byteCnt + 1 == sizeof(T)) {
							log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"%s", byteLine);
							lineWritePoint = byteLine;
						}
					}

					log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"");
				}

			}

			list = list->_next;

		}
	}

	log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"───────────────────────────────────────────");

	TlsFree(_chunkListTlsIdx);
	HeapDestroy(_heap);

#endif

	TlsFree(_allocChunkTlsIdx);
	delete _centerFreeList;
}

template <typename T>
typename T* CObjectFreeListTLS<T>::_allocObject(
#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	const wchar_t* fileName,
	int line
#endif
) {

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	///////////////////////////////////////////////////////////////////////
	// 청크 리스트 획득
	stSimpleList* list = nullptr;
	int chunkListIndex = (int)TlsGetValue(_chunkListTlsIdx);
	if (chunkListIndex == 0) {
		int index = InterlockedIncrement((LONG*)&_chunkListIndex);
		TlsSetValue(_chunkListTlsIdx, (void*)index);
		chunkListIndex = index;
	}
	list = _chunkListArr[chunkListIndex];
	///////////////////////////////////////////////////////////////////////
#endif

///////////////////////////////////////////////////////////////////////
// 청크 획득
	stAllocChunk<T>* chunk = (stAllocChunk<T>*)TlsGetValue(_allocChunkTlsIdx);
	if (chunk == nullptr) {
		chunk = _centerFreeList->allocObject();
		chunk->init();
		TlsSetValue(_allocChunkTlsIdx, chunk);

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
		stSimpleList* node = (stSimpleList*)HeapAlloc(_heap, HEAP_ZERO_MEMORY, sizeof(stSimpleList));
		node->_chunk = chunk;
		node->_next = list;
		list = node;
		_chunkListArr[chunkListIndex] = node;
#endif

	}
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// 청크에서 노드 획득 & 초기화
	stAllocTlsNode<T>* allocNode = chunk->_allocNode++;
#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	allocNode->_allocated = true;
	allocNode->_allocSourceFileName = fileName;
	allocNode->_allocLine = line;
#endif
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// 노드에서 T type 포인터 획득
	T* allocData = &allocNode->_data;

	if (_runConstructor == true) {
		new (allocData) T();
	}
	///////////////////////////////////////////////////////////////////////
	
	///////////////////////////////////////////////////////////////////////
	// 청크를 모두 사용했다면 새로 할당받기
	if (allocNode == chunk->_nodeEnd) {

		chunk = _centerFreeList->allocObject();
		TlsSetValue(_allocChunkTlsIdx, chunk);

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
		stSimpleList* node = (stSimpleList*)HeapAlloc(_heap, HEAP_ZERO_MEMORY, sizeof(stSimpleList));
		node->_chunk = chunk;
		node->_next = list;
		_chunkListArr[chunkListIndex] = node;
#endif

	}
	///////////////////////////////////////////////////////////////////////

	return allocData;

}

template <typename T>
void CObjectFreeListTLS<T>::_freeObject(T* object
#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	, const wchar_t* fileName,
	int line
#endif
) {

	///////////////////////////////////////////////////////////////////////
	// 할당했던 노드 획득
	stAllocTlsNode<T>* allocatedNode = (stAllocTlsNode<T>*)((unsigned __int64)object + objectFreeListTLS::DATA_TO_NODE_PTR);
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// object 정상 사용 여부 체크
#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	if (allocatedNode->_underflowCheck != objectFreeListTLS::UNDERFLOW_CHECK_VALUE) {
		// underflow
		CDump::crash();
	}
	if (allocatedNode->_overflowCheck != objectFreeListTLS::OVERFLOW_CHECK_VALUE) {
		// overflow
		CDump::crash();
	}
	if (allocatedNode->_allocated == false) {
		// 할당되지 않은 노드 해제 시도
		CDump::crash();
	}
	allocatedNode->_allocated = false;
#endif
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// 해제 요청한 소스 파일 & 라인 저장
#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	allocatedNode->_freeSourceFileName = fileName;
	allocatedNode->_freeLine = line;
#endif
	///////////////////////////////////////////////////////////////////////


	///////////////////////////////////////////////////////////////////////
	// 소멸자 호출
	if (_runDestructor == true) {
		object->~T();
	}
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// 노드에서 청크 획득
	stAllocChunk<T>* chunk = allocatedNode->_afflicatedChunk;
	///////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////
	// 청크의 모든 요소가 사용 완료(할당 후 반환)되었다면 청크를 반환
	if (InterlockedDecrement((LONG*)&chunk->_leftFreeCnt) == 0) {
		chunk->init();
		_centerFreeList->freeObject(chunk);
	}
	///////////////////////////////////////////////////////////////////////
}

template <typename T>
unsigned int CObjectFreeListTLS<T>::getUsedCount() {

	return _centerFreeList->getUsedCount();

}

template <typename T>
unsigned int CObjectFreeListTLS<T>::getCapacity() {

	return _centerFreeList->getCapacity();

}

template <typename T>
struct stAllocTlsNode {

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	unsigned __int64 _underflowCheck;
#endif

	T _data;

#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	unsigned __int64 _overflowCheck;

	const wchar_t* _allocSourceFileName;
	const wchar_t* _freeSourceFileName;

	int _allocLine;
	int  _freeLine;

	bool _allocated;

#endif

	// 노드가 소속되어있는 청크
	stAllocChunk<T>* _afflicatedChunk;

	void init(stAllocChunk<T>* afflicatedChunk);
};

template <typename T>
void stAllocTlsNode<T>::init(stAllocChunk<T>* afflicatedChunk) {
#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
	_underflowCheck = objectFreeListTLS::UNDERFLOW_CHECK_VALUE;
	_overflowCheck = objectFreeListTLS::OVERFLOW_CHECK_VALUE;

	_allocSourceFileName = nullptr;
	_freeSourceFileName = nullptr;

	_allocLine = 0;
	_freeLine = 0;

	_allocated = false;
#endif

	_afflicatedChunk = afflicatedChunk;
}


template <typename T>
struct stAllocChunk {

public:

	stAllocChunk();

	stAllocTlsNode<T>* _allocNode;
	int _leftFreeCnt;

	alignas(64) stAllocTlsNode<T> _nodes[objectFreeListTLS::CHUNK_SIZE];
	stAllocTlsNode<T>* _nodeEnd;
	int _nodeNum;

	void init();

};

template<typename T>
stAllocChunk<T>::stAllocChunk() {

	_nodeNum = objectFreeListTLS::CHUNK_SIZE;
	_nodeEnd = _nodes + _nodeNum - 1;

	for (int nodeCnt = 0; nodeCnt < _nodeNum; ++nodeCnt) {
		_nodes[nodeCnt].init(this);
	}

	init();

}

template<typename T>
void stAllocChunk<T>::init() {

	this->_allocNode = this->_nodes;
	this->_leftFreeCnt = this->_nodeNum;

}