#pragma once

#include <Windows.h>

#include "objectFreeListTLS/headers/objectFreeListTLS.h"

static constexpr unsigned __int64 _useCntMask	= 0xFFFFF80000000000;
static constexpr unsigned __int64 _pointerMask	= 0x000007FFFFFFFFFF;

template <typename T>
class CLockFreeQueue{

public:

	CLockFreeQueue();

	void push(T data);
	bool pop(T* data);

	unsigned __int64 getSize();

private:

	struct stNode{
		stNode(){
			_next = nullptr;
		}
		void* _next;
		T _data;
	};
	
	void* _head;
	void* _tail;

	unsigned __int64 _size;

	unsigned __int64 _nodeChangeCnt;

	CObjectFreeList<stNode> _nodeFreeList;

};

template <typename T>
CLockFreeQueue<T>::CLockFreeQueue():
	_nodeFreeList(false, false){

	stNode* node = _nodeFreeList.allocObject();
	_head = node;
	_tail = node;

	_nodeChangeCnt = 0;
	_size = 0;
}

template <typename T>
void CLockFreeQueue<T>::push(T data){
		
	stNode* newNode = _nodeFreeList.allocObject();
	newNode->_data = data;
	newNode->_next = nullptr;

	void* newPtr;
	void* tail;
	void* tailNextPtr;

	stNode* tailNode;
		
	InterlockedAdd64((LONG64*)&_nodeChangeCnt, 0x0000080000000000);
	newPtr = (void*)(_nodeChangeCnt | (unsigned __int64)newNode);

	do{
		
		// tail�� next�� null�� ������ ����
		do{

			tail = _tail;
			tailNode = (stNode*)((unsigned __int64)tail & _pointerMask);
			tailNextPtr = tailNode->_next;

			if(tailNextPtr == nullptr){
				break;
			}
						

		}while(InterlockedCompareExchange64((LONG64*)&_tail, (LONG64)tailNextPtr, (LONG64)tail) != (LONG64)tail);


		tail = _tail;
		tailNode = (stNode*)((unsigned __int64)tail & _pointerMask);

	} while( InterlockedCompareExchange64((LONG64*)&tailNode->_next, (LONG64)newPtr, (LONG64)nullptr ) != (LONG64)nullptr );
	
	// tail�� �����ߴ� ��尡 �ٸ� �����忡 ���� ����, ���Ҵ�, ���� tail�� next�� �ٰ� tail�� ���� �����ع�����
	// �Ʒ� interlock �����ϰԵ� (_tail�� ����Ǿ��⿡)
	InterlockedCompareExchange64((LONG64*)&_tail, (LONG64)newPtr, (LONG64)tail);
	
	InterlockedIncrement64((LONG64*)&_size);

}

template <typename T>
bool CLockFreeQueue<T>::pop(T* data){
	
	
	unsigned __int64 size = InterlockedDecrement64((LONG64*)&_size);
	if(size < 0){
		InterlockedIncrement64((LONG64*)&_size);
		return false;
	}

	void* popPtr;
	void* head;

	stNode* popNode;
	stNode* headNode;

	T popData;

	do{
		
		head = _head;
		headNode = (stNode*)((unsigned __int64)head & _pointerMask);
			
		popPtr = headNode->_next;
		if(popPtr == nullptr){
			// haedNode ȹ�� ���� �ٸ� �����忡�� head�� free �� ���Ҵ� �޾Ƽ� next�� nullptr�� �ʱ�ȭ�� �� ����
			head = nullptr;
			continue;
		}
		
		popNode = (stNode*)((unsigned __int64)popPtr & _pointerMask);
		popData = popNode->_data;

	}while(  InterlockedCompareExchange64((LONG64*)&_head, (LONG64)popPtr, (LONG64)head) != (LONG64)head);
	
	*data = popData;

	_nodeFreeList.freeObject(headNode);

	return true;
}

template <typename T>
unsigned __int64 CLockFreeQueue<T>::getSize(){
	return _size;
}