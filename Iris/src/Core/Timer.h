#pragma once

#include "Base.h"

#include <chrono>

namespace Iris {

	class Timer
	{
	public:
		IR_FORCE_INLINE Timer() { Reset(); }
		IR_FORCE_INLINE void Reset() { m_Start = std::chrono::high_resolution_clock::now(); }
		IR_FORCE_INLINE float Elapsed() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_Start).count() * 0.001f * 0.001f; }
		IR_FORCE_INLINE float ElapsedMillis() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_Start).count() * 0.001f; }
	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;

	};
}