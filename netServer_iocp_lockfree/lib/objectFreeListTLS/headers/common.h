#pragma once

//#define OBJECT_FREE_LIST_TLS_DEBUG

namespace objectFreeListTLS{
	// T type data 주소에 이 값을 더하면 node 주소가 됩니다.
	#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
		constexpr __int64 DATA_TO_NODE_PTR = -8;
	#else
		constexpr __int64 DATA_TO_NODE_PTR = 0;
	#endif

	// 하나의 청크에서 사용할 T type 노드의 수 입니다.
	constexpr int CHUNK_SIZE = 50000;

	#if defined(OBJECT_FREE_LIST_TLS_DEBUG)
		constexpr unsigned __int64 UNDERFLOW_CHECK_VALUE = 0xAABBCCDDDDCCBBAA;
		constexpr unsigned __int64 OVERFLOW_CHECK_VALUE  = 0xFFEEDDCCCCDDEEFF;
	#endif

	#if defined(OBJECT_FREE_LIST_TLS_DEBUG)

		// alloc object에 접근하는 최대 스레드 수
		constexpr int MAX_THREAD_NUM = 500;

	#endif

};