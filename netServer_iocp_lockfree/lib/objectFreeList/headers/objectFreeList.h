#pragma once

#include <typeinfo>

#include "common.h"

#include "dump/headers/dump.h"
#pragma comment(lib, "lib/dump/dump")

#if defined(OBJECT_FREE_LIST_DEBUG)
	#include "log/headers/log.h"
	#pragma comment(lib, "lib/log/log")
#endif

#if defined(OBJECT_FREE_LIST_DEBUG)
	#define allocObject() _allocObject(__FILEW__, __LINE__)
	#define freeObject(x) _freeObject(x, __FILEW__, __LINE__)
#else
	#define allocObject() _allocObject()
	#define freeObject(x) _freeObject(x)
#endif

#define toNode(ptr) ((stAllocNode<T>*)((unsigned __int64)ptr & 0x000007FFFFFFFFFF))
#define toPtr(cnt, pNode) ((void*)((unsigned __int64)pNode | (cnt << 43)))

template<typename T>
struct stAllocNode {
	stAllocNode() {
		init();
	}
	~stAllocNode(){
	}

	void init(){
		
		_nextPtr = nullptr;

		#if defined(OBJECT_FREE_LIST_DEBUG)
			_allocated = false;
			_underFlowCheck = (void*)0xF9F9F9F9F9F9F9F9;
			_overFlowCheck = (void*)0xF9F9F9F9F9F9F9F9;

			_allocSourceFileName = nullptr;
			_freeSourceFileName = nullptr;
			
			_allocLine = 0;
			_freeLine = 0;

			_allocated = false;
		#endif

		_callDestructor = false;
	}

	#if defined(OBJECT_FREE_LIST_DEBUG)
		// f9로 초기화해서 언더플로우 체크합니다.
		void* _underFlowCheck;
	#endif

	// alloc 함수에서 리턴할 실제 데이터
	T _data;
	
	#if defined(OBJECT_FREE_LIST_DEBUG)
		// f9로 초기화해서 오버플로우 체크합니다.
		void* _overFlowCheck;
	#endif

	// 할당할 다음 노드
	void* _nextPtr;

	#if defined(OBJECT_FREE_LIST_DEBUG)
		// 소스 파일 이름
		const wchar_t* _allocSourceFileName;
		const wchar_t* _freeSourceFileName;

		// 소스 라인
		int _allocLine;
		int _freeLine;

		// 노드가 사용중인지 확인
		bool _allocated;
	#endif

	// 할당 이후 소멸자가 호출되었는지 여부
	// 할당 이후 사용자가 반환하여 소멸자가 호출되었다면
	// 클래스가 소멸할 때, 소멸자가 호출되면 안된다.
	bool _callDestructor;
};

template<typename T>
class CObjectFreeList
{
public:

	CObjectFreeList(bool runConstructor, bool runDestructor, int _capacity = 0);
	~CObjectFreeList();


	#if defined(OBJECT_FREE_LIST_DEBUG)
		T* _allocObject(const wchar_t*, int);
	#else
		T* _allocObject();
	#endif
	
	#if defined(OBJECT_FREE_LIST_DEBUG)
		int _freeObject(T* data, const wchar_t*, int);
	#else
		int _freeObject(T* data);
	#endif

	inline unsigned int getCapacity() { return _capacity; }
	inline unsigned int getUsedCount() { return _usedCnt; }

private:

	// 메모리 할당, 해제를 위한 힙
	HANDLE _heap;

	// 사용 가능한 노드를 리스트의 형태로 저장합니다.
	// 할당하면 제거합니다.
	void* _freePtr;

	// 전체 노드 개수
	unsigned int _capacity;

	// 현재 할당된 노드 개수
	unsigned int _usedCnt;

	// 메모리 정리용
	// 단순 리스트
	struct stSimpleListNode {
		stAllocNode<T>* _ptr;
		stSimpleListNode* _next;
	};

	// freeList 소멸자에서 메모리 정리용으로 사용합니다.
	// new한 포인터들
	stSimpleListNode* _totalAllocList;
		
	// 리스트 변경 횟수
	// ABA 문제를 회피하기 위해 사용합니다.
	unsigned __int64 _nodeChangeCnt;

	// 할당 시, 생성자 실행 여부를 나타냅니다.
	bool _runConstructor;

	// 해제 시, 소멸자 실행 여부를 나타냅니다.
	bool _runDestructor;

	#if defined(OBJECT_FREE_LIST_DEBUG)
		CLog log;
	#endif

};

template <typename T>
CObjectFreeList<T>::CObjectFreeList(bool runConstructor, bool runDestructor, int size) {
	
	_heap = HeapCreate(0, 0, 0);

	_freePtr = nullptr;
	_totalAllocList = nullptr;
	
	_capacity = size;
	_usedCnt = 0;

	_runConstructor = runConstructor;
	_runDestructor = runDestructor;
	
	_nodeChangeCnt = 0;

	#if defined(OBJECT_FREE_LIST_DEBUG)
		log.setDirectory(L"objectFreeList_Log");
		log.setPrintGroup(LOG_GROUP::LOG_DEBUG);
	#endif

	if (size == 0) {
		return;
	}

	for(int nodeCnt = 0; nodeCnt < size; ++nodeCnt){

		// 미리 만들어놓을 개수만큼 노드를 만들어 놓음
		stAllocNode<T>* newNode = (stAllocNode<T>*)HeapAlloc(_heap, 0, sizeof(stAllocNode<T>));

		// T type에 대한 생성자 호출 여부를 결정
		if(runConstructor == false) {
			new (newNode) stAllocNode<T>;
		} else {
			newNode->init();
		}

		newNode->_nextPtr = _freePtr;
		_freePtr = newNode;

		{
			// 전체 alloc list에 추가
			// 소멸자에서 일괄적으로 메모리 해제하기 위함
			stSimpleListNode* totalAllocNode = (stSimpleListNode*)HeapAlloc(_heap, 0, sizeof(stSimpleListNode));

			totalAllocNode->_ptr = newNode;
			totalAllocNode->_next = _totalAllocList;

			_totalAllocList = totalAllocNode;

		}

	}


}

template <typename T>
CObjectFreeList<T>::~CObjectFreeList() {
	
	#if defined(OBJECT_FREE_LIST_DEBUG)
		log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"────────────────────────────────");
		wchar_t typeName[50];
		size_t convertSize;
		mbstowcs_s(&convertSize, typeName, typeid(T).name(), 50);
		log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"type: %s", typeName);
	#endif

	while(_totalAllocList != nullptr){
		stSimpleListNode* allocListNode = _totalAllocList;
		stAllocNode<T>* freeNode = allocListNode->_ptr;

		#if defined(OBJECT_FREE_LIST_DEBUG)
			if(freeNode->_allocated == true){
				
				log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"alloc file name: %s", freeNode->_allocSourceFileName);
				log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"alloc line: %d", freeNode->_allocLine);
				log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"data(%d bytes): ", sizeof(T));

				wchar_t byteLine[25];
				wchar_t* lineWritePoint = byteLine;
				for(int byteCnt = 0; byteCnt < sizeof(T); byteCnt++){

					swprintf_s(lineWritePoint, 4, L"%02X ", *( ((unsigned char*)(&freeNode->_data) + byteCnt)) );
					lineWritePoint += 3;

					if((byteCnt+1) % 8 == 0 || byteCnt+1 == sizeof(T)){
						log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"%s", byteLine);
						lineWritePoint = byteLine;
					}
				}

				log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"");

			}
		#endif

		if(freeNode->_callDestructor == false){
			freeNode->~stAllocNode();
		}
		HeapFree(_heap, 0, freeNode);
		_totalAllocList = allocListNode->_next;
		HeapFree(_heap, 0, allocListNode);
	}
	
	#if defined(OBJECT_FREE_LIST_DEBUG)
		log(L"memoryLeak.txt", LOG_GROUP::LOG_DEBUG, L"────────────────────────────────");
	#endif

	HeapDestroy(_heap);

}

template<typename T>
T* CObjectFreeList<T>::_allocObject(
	#if defined(OBJECT_FREE_LIST_DEBUG)
		const wchar_t* fileName, int line
	#endif
) {
	
	InterlockedIncrement(&_usedCnt);
	
	stAllocNode<T>* allocNode;

	void* freePtr;
	void* nextPtr;

	unsigned __int64 nodeChangeCnt;
	
	_nodeChangeCnt += 1;

	do {
		// 원본 데이터 복사
		freePtr = _freePtr;
		nodeChangeCnt = _nodeChangeCnt;

		allocNode = toNode(freePtr);

		if (allocNode == nullptr) {

			// 추가 할당
			allocNode = (stAllocNode<T>*)HeapAlloc(_heap, 0, sizeof(stAllocNode<T>));
			if(_runConstructor == false) {
				new (allocNode) stAllocNode<T>;
			} else {
				allocNode->init();
			}
		
			#if defined(OBJECT_FREE_LIST_DEBUG)
				// 전체 alloc list에 추가
				// 소멸자에서 일괄적으로 메모리 해제하기 위함
				stSimpleListNode* totalAllocNode = (stSimpleListNode*)HeapAlloc(_heap, 0, sizeof(stSimpleListNode));
				stSimpleListNode* totalAllocList;

				do {

					totalAllocList = _totalAllocList;

					totalAllocNode->_ptr = allocNode;
					totalAllocNode->_next = totalAllocList;

				} while( InterlockedCompareExchange64((LONG64*)&_totalAllocList, (LONG64)totalAllocNode, (LONG64)totalAllocList) != (LONG64)totalAllocList );
			#endif
			_capacity += 1;

			break;

		}

		nextPtr = allocNode->_nextPtr;

	} while(InterlockedCompareExchange64((LONG64*)&_freePtr, (LONG64)nextPtr, (LONG64)freePtr) != (LONG64)freePtr);
	
	// 소멸자 호출 여부 초기화
	allocNode->_callDestructor = false;

	#if defined(OBJECT_FREE_LIST_DEBUG)
		// 노드를 사용중으로 체크함
		allocNode->_allocated = true;

		// 할당 요청한 소스파일과 소스라인을 기록함
		allocNode->_allocSourceFileName = fileName;
		allocNode->_allocLine = line;
	#endif
	
	T* data = &allocNode->_data;

	// 생성자 실행
	if(_runConstructor == true){
		new (data) T();
	}

	return data;
}

template <typename T>
int CObjectFreeList<T>::_freeObject(T* data	
	#if defined(OBJECT_FREE_LIST_DEBUG)
		, const wchar_t* fileName, int line
	#endif
) {

	stAllocNode<T>* usedNode = (stAllocNode<T>*)(((char*)data) + objectFreeList::DATA_PTR_TO_NODE_PTR);
	
	#if defined(OBJECT_FREE_LIST_DEBUG)
		// 중복 free 체크
		if(usedNode->_allocated == false){
			CDump::crash();
		}

		// 오버플로우 체크
		if((unsigned __int64)usedNode->_overFlowCheck != 0xF9F9F9F9F9F9F9F9){
			CDump::crash();
		}

		// 언더플로우 체크
		if((unsigned __int64)usedNode->_underFlowCheck != 0xF9F9F9F9F9F9F9F9){
			CDump::crash();
		}

		// 노드의 사용중 플래그를 내림
		usedNode->_allocated = false;
	#endif

	// 소멸자 실행
	if(_runDestructor == true){
		data->~T();
		usedNode->_callDestructor = true;
	}

	void* freePtr;
	void* nextPtr;

	unsigned __int64 nodeChangeCnt;

	_nodeChangeCnt += 1;

	do {

		// 원본 데이터 복사
		freePtr = _freePtr;
		nodeChangeCnt = _nodeChangeCnt;
			
		// 사용했던 노드의 next를 현재 top으로 변경
		usedNode->_nextPtr = freePtr;

		nextPtr = toPtr(nodeChangeCnt, usedNode);

	} while(InterlockedCompareExchange64((LONG64*)&_freePtr, (LONG64)nextPtr, (LONG64)freePtr) != (LONG64)freePtr);
	
	InterlockedDecrement(&_usedCnt);

	return 0;

}