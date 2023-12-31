#pragma once

#include <EASTL/string.h>
#include <EASTL/memory.h>
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>
#include <EASTL/map.h>
#include <EASTL/hash_set.h>
#include <EASTL/stack.h>
#include <EASTL/queue.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/sort.h>

#include <filesystem>

#include <git2.h>

#include "Log.h"

namespace QuickGit::Allocation
{
	size_t GetSize();
}
