#pragma once

#include "common.h"

#include "dump/headers/dump.h"
#pragma comment(lib, "lib/dump/dump")

#if defined(OBJECT_FREE_LIST_SAFE)
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

		_nextPtr = nullptr;

		#if defined(OBJECT_FREE_LIST_SAFE)
			_used = false;
			_underFlowCheck = (void*)0xF9F9F9F9F9F9F9F9;
			_overFlowCheck = (void*)0xF9F9F9F9F9F9F9F9;
		#endif
	}

	#if defined(OBJECT_FREE_LIST_SAFE)
		// f9로 초기화해서 언더플로우 체크합니다.
		void* _underFlowCheck;
	#endif

	// alloc 함수에서 리턴할 실제 데이터
	T _data;
	
	#if defined(OBJECT_FREE_LIST_SAFE)
		// f9로 초기화해서 오버플로우 체크합니다.
		void* _overFlowCheck;
	#endif

	// 할당할 다음 노드
	void* _nextPtr;

	#if defined(OBJECT_FREE_LIST_SAFE)
		// 소스 파일 이름
		const wchar_t* _allocSourceFileName;
		const wchar_t* _freeSourceFileName;

		// 소스 라인
		int _allocLine;
		int _freeLine;

		// 노드가 사용중인지 확인
		bool _used;
	#endif
};

template<typename T>
class CObjectFreeList
{
public:

	CObjectFreeList(bool runConstructor, bool runDestructor, int _capacity = 0);
	~CObjectFreeList();


	#if defined(OBJECT_FREE_LIST_SAFE)
		T* _allocObject(const wchar_t*, int);
	#else
		T* _allocObject();
	#endif
	
	#if defined(OBJECT_FREE_LIST_SAFE)
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

	// 데이터 포인터(stNode->data)에 이 값을 더하면 노드 포인터(stNode)가 된다 !
	unsigned __int64 _dataPtrToNodePtr;

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
	unsigned __int64 _nodeChangeCnt = 0;

	// 할당 시, 생성자 실행 여부를 나타냅니다.
	bool _runConstructor;

	// 해제 시, 소멸자 실행 여부를 나타냅니다.
	bool _runDestructor;

};

template <typename T>
CObjectFreeList<T>::CObjectFreeList(bool runConstructor, bool runDestructor, int size) {

	_totalAllocList = nullptr;
	_freePtr = nullptr;

	_capacity = size;
	_usedCnt = 0;

	_heap = HeapCreate(0, 0, 0);
	_runConstructor = runConstructor;
	_runDestructor = runDestructor;
	
	// 실제 노드와 노드의 data와의 거리 계산
	stAllocNode<T> tempNode;
	_dataPtrToNodePtr = (unsigned __int64)&tempNode - (unsigned __int64)&tempNode._data;

	if (size == 0) {
		return;
	}

	for(int nodeCnt = 0; nodeCnt < size; ++nodeCnt){

		// 미리 만들어놓을 개수만큼 노드를 만들어 놓음
		stAllocNode<T>* newNode = (stAllocNode<T>*)HeapAlloc(_heap, 0, sizeof(stAllocNode<T>));
		new (newNode) stAllocNode<T>;
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

	while(_totalAllocList != nullptr){
		stAllocNode<T>* freeNode = _totalAllocList->_ptr;
		HeapFree(_heap, 0, freeNode);
		_totalAllocList = _totalAllocList->_next;
	}

}

template<typename T>
T* CObjectFreeList<T>::_allocObject(
	#if defined(OBJECT_FREE_LIST_SAFE)
		const wchar_t* fileName, int line
	#endif
) {
	
	InterlockedIncrement(&_usedCnt);
	
	stAllocNode<T>* nextNode;
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
			new (allocNode) stAllocNode<T>;
		
			// 전체 alloc list에 추가
			// 소멸자에서 일괄적으로 메모리 해제하기 위함
			stSimpleListNode* totalAllocNode = (stSimpleListNode*)HeapAlloc(_heap, 0, sizeof(stSimpleListNode));
			stSimpleListNode* totalAllocList;

			do {

				totalAllocList = _totalAllocList;

				totalAllocNode->_ptr = allocNode;
				totalAllocNode->_next = totalAllocList;

			} while( InterlockedCompareExchange64((LONG64*)&_totalAllocList, (LONG64)totalAllocNode, (LONG64)totalAllocList) != (LONG64)totalAllocList );

			_capacity += 1;

			break;

		}

	} while(InterlockedCompareExchange64((LONG64*)&_freePtr, (LONG64)allocNode->_nextPtr, (LONG64)freePtr) != (LONG64)freePtr);
	
	#if defined(OBJECT_FREE_LIST_SAFE)
		// 노드를 사용중으로 체크함
		allocNode->_used = true;

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
	#if defined(OBJECT_FREE_LIST_SAFE)
		, const wchar_t* fileName, int line
	#endif
) {

	stAllocNode<T>* usedNode = (stAllocNode<T>*)(((char*)data) + _dataPtrToNodePtr);
	
	#if defined(OBJECT_FREE_LIST_SAFE)
		// 중복 free 체크
		if(usedNode->_used == false){
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
		usedNode->_used = false;
	#endif

	// 소멸자 실행
	if(_runDestructor == true){
		data->~T();
	}

	stAllocNode<T>* freeNode;

	void* freePtr;
	void* nextPtr;

	unsigned __int64 nodeChangeCnt = 0;

	_nodeChangeCnt += 1;

	do {

		// 원본 데이터 복사
		freePtr = _freePtr;
		nodeChangeCnt = _nodeChangeCnt;
			
		// 사용했던 노드의 next를 현재 top으로 변경
		usedNode->_nextPtr = toNode(freePtr);

		nextPtr = toPtr(nodeChangeCnt, usedNode);

	} while(InterlockedCompareExchange64((LONG64*)&_freePtr, (LONG64)nextPtr, (LONG64)freePtr) != (LONG64)freePtr);
	
	InterlockedDecrement(&_usedCnt);

	return 0;

}