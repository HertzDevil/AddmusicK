#pragma once

#include <stdexcept>
#include "../MML/SourceFile.h"

namespace AMKd::Utility {

// base class

class Exception : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

// thrown for logical errors while compiling

class MMLException : public Exception
{
public:
	using Exception::Exception;
	MMLException &Fatal() {
		fatal_ = true;
		return *this;
	}
	bool IsFatal() const {
		return fatal_;
	}

private:
	bool fatal_ = false;
};

// thrown when parsing fails

class SyntaxException : public MMLException
{
public:
	using MMLException::MMLException;
};

// thrown when parsing succeeds but parameters are illegal

class ParamException : public MMLException
{
public:
	using MMLException::MMLException;
};

// thrown for logical errors while inserting music / generating spc files

class InsertException : public Exception
{
public:
	using Exception::Exception;
};

} // namespace AMKd::Utility
