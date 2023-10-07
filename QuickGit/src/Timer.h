#pragma once

#include <EASTL/chrono.h>

class Stopwatch
{
public:
	Stopwatch(const char* name)
		: m_Name(name), m_Start(eastl::chrono::high_resolution_clock::now())
	{
	}

	~Stopwatch()
	{
		m_Elapsed = eastl::chrono::duration<float>(eastl::chrono::high_resolution_clock::now() - m_Start).count();
		printf("%s: %.3fs\n", m_Name, m_Elapsed);
	}

private:
	const char* m_Name;
	eastl::chrono::high_resolution_clock::time_point m_Start;
	float m_Elapsed = 0.0f;
};
