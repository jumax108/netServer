#pragma once

#include <windows.h>
#include <crtdbg.h>
#include <DbgHelp.h>
#pragma comment(lib, "DbgHelp")

class CDump {

public:

	CDump();
	static void crash();
	
	static long __stdcall myExceptionFilter(PEXCEPTION_POINTERS exceptionPointer);
	static void setHandlerDump();
	static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t resevered);
	
	static int customReportHook(int repostType, char *msg, int* returnValue);
	static void myPureCallHandler();
	
private:

	static long _dumpCnt;

};
