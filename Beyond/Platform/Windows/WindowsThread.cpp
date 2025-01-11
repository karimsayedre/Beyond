#include "pch.h"
#include "Beyond/Core/Thread.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <Windows.h>

namespace Beyond {

	Thread::Thread(const eastl::string& name)
		: m_Name(name)
	{
	}

	void Thread::SetName(const eastl::string& name)
	{
		HANDLE threadHandle = m_Thread.native_handle();

		std::wstring wName(name.begin(), name.end());
		SetThreadDescription(threadHandle, wName.c_str());
		SetThreadAffinityMask(threadHandle, 8);
	}

	void Thread::Join()
	{
		m_Thread.join();
	}

	void Thread::Stop()
	{
		m_Thread.join();
	}

	ThreadSignal::ThreadSignal(const eastl::string& name, bool manualReset)
	{
		std::wstring str(name.begin(), name.end());
		m_SignalHandle = CreateEvent(NULL, (BOOL)manualReset, FALSE, str.c_str());
	}

	void ThreadSignal::Wait()
	{
		WaitForSingleObject(m_SignalHandle, INFINITE);
	}

	void ThreadSignal::Signal()
	{
		SetEvent(m_SignalHandle);
	}

	void ThreadSignal::Reset()
	{
		ResetEvent(m_SignalHandle);
	}

}
