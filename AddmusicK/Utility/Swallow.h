#pragma once

// // // vs2017 does not have fold-expressions
#define SWALLOW(x) do { \
		using _swallow = int[]; \
		(void)_swallow {0, ((x), 0)...}; \
	} while (false)
