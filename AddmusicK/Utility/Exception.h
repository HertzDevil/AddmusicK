#pragma once

#include <stdexcept>
#include "../MML/SourceFile.h"

namespace AMKd::Utility {

// base struct
struct Exception : public std::runtime_error
{
	template <typename T>
	Exception(T &&str, bool fatal = false) :
		std::runtime_error(std::forward<T>(str)), isFatal(fatal) { }
	const bool isFatal;
};

#define SUBCLASS_EXCEPTION(D, B) \
	struct D : public B \
	{ \
		using B::B; \
	};

// thrown for logical errors while compiling
SUBCLASS_EXCEPTION(MMLException, Exception)

// thrown when parsing fails
SUBCLASS_EXCEPTION(SyntaxException, MMLException)

// thrown when parsing succeeds but parameters are illegal
SUBCLASS_EXCEPTION(ParamException, MMLException)

// thrown for logical errors while inserting music / generating spc files
SUBCLASS_EXCEPTION(InsertException, Exception)

#undef SUBCLASS_EXCEPTION

} // namespace AMKd::Utility
