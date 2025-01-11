#pragma once

#include <thread>

namespace Beyond {

	class Thread
	{
	public:
		Thread(const eastl::string& name);

		template<typename Fn, typename... Args>
		void Dispatch(Fn&& func, Args&&... args)
		{
			m_Thread = std::jthread(func, std::forward<Args>(args)...);
			SetName(m_Name);
		}

		void SetName(const eastl::string& name);

		void Join();
		void Stop();

	private:
		eastl::string m_Name;
		std::jthread m_Thread;
	};

	class ThreadSignal
	{
	public:
		ThreadSignal(const eastl::string& name, bool manualReset = false);

		void Wait();
		void Signal();
		void Reset();
	private:
		void* m_SignalHandle = nullptr;
	};

}
