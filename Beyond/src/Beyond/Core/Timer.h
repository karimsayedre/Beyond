#pragma once

#include <chrono>
#include <unordered_map>

#include "Log.h"

namespace Beyond {

	class Timer
	{
	public:
		BEY_FORCE_INLINE Timer() { Reset(); }
		BEY_FORCE_INLINE void Reset() { m_Start = std::chrono::high_resolution_clock::now(); }
		BEY_FORCE_INLINE float Elapsed() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_Start).count() * 0.001f * 0.001f; }
		BEY_FORCE_INLINE float ElapsedMillis() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_Start).count() * 0.001f; }
		BEY_FORCE_INLINE auto ElapsedMillis2() { return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - m_Start).count(); }
	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;
	};
	
	class ScopedTimer
	{
	public:
		explicit ScopedTimer(eastl::string name)
			: m_Name(std::move(name))
		{
		}

		template<typename FormatStr, typename... Args>
		ScopedTimer(FormatStr&& format, Args&&... args)
		{
			fmt::v10::string_view fmtString;

			if constexpr (std::is_same_v<FormatStr, eastl::string>)
			{
				fmtString = fmt::v10::string_view(format.c_str(), format.size());
			}
			else
			{
				fmtString = fmt::v10::string_view(format);
			}

			auto formattedMessage = fmt::v10::vformat(fmtString, fmt::make_format_args(args...));
			m_Name = eastl::string{ formattedMessage.c_str(), formattedMessage.size() };

			m_Timer = Timer();
		}

		~ScopedTimer()
		{
			float time = m_Timer.ElapsedMillis();
			BEY_CORE_TRACE_TAG("TIMER", "{0} - {1}ms", m_Name, time);
		}
	private:
		eastl::string m_Name;
		Timer m_Timer;
	};

	class PerformanceProfiler
	{
	public:
		struct PerFrameData
		{
			float Time = 0.0f;
			uint32_t Samples = 0;

			PerFrameData() = default;
			PerFrameData(float time) : Time(time) {}

			operator float() const { return Time; }
			inline PerFrameData& operator+=(float time)
			{
				Time += time;
				return *this;
			}
		};
	public:
		void SetPerFrameTiming(const char* name, float time)
		{
			std::scoped_lock<std::mutex> lock(m_PerFrameDataMutex);

			if (!m_PerFrameData.contains(name))
				m_PerFrameData[name] = 0.0f;

			PerFrameData& data = m_PerFrameData[name];
			data.Time += time;
			data.Samples++;
		}

		void Clear()
		{
			std::scoped_lock<std::mutex> lock(m_PerFrameDataMutex);
			m_PerFrameData.clear();
		}

		const std::unordered_map<const char*, PerFrameData>& GetPerFrameData() const { return m_PerFrameData; }
	private:
		std::unordered_map<const char*, PerFrameData> m_PerFrameData;
		inline static std::mutex m_PerFrameDataMutex;
	};

	class ScopePerfTimer
	{
	public:
		ScopePerfTimer(const char* name, PerformanceProfiler* profiler)
			: m_Name(name), m_Profiler(profiler) {}

		~ScopePerfTimer()
		{
			float time = m_Timer.ElapsedMillis();
			m_Profiler->SetPerFrameTiming(m_Name, time);
		}
	private:
		const char* m_Name;
		PerformanceProfiler* m_Profiler;
		Timer m_Timer;
	};

#if 1
#define BEY_SCOPE_PERF(name)\
	ScopePerfTimer timer__LINE__(name, Application::Get().GetPerformanceProfiler())

#define BEY_SCOPE_TIMER(...)\
	ScopedTimer timer__LINE__(__VA_ARGS__)
#else
#define BEY_SCOPE_PERF(name)
#define BEY_SCOPE_TIMER(...)
#endif
}
