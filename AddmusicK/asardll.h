#pragma once
#ifndef asarfunc
#define asarfunc extern
#endif

#define expectedapiversion 200

//#include <stdbool.h>

//These structures are returned from various functions.
struct errordata {
	const char * fullerrdata;
	const char * rawerrdata;
	const char * block;
	const char * filename;
	int line;
	const char * callerfilename;
	int callerline;
};

struct labeldata {
	const char * name;
	int location;
};

struct definedata {
	const char * name;
	const char * contents;
};

//Returns the version, in the format major*10000+minor*100+bugfix*1. This means that 1.2.34 would be
// returned as 10234.
asarfunc int (*asar_version)();

//Returns the API version, format major*100+minor. Minor is incremented on backwards compatible
// changes; major is incremented on incompatible changes. Does not have any correlation with the
// Asar version.
//It's not very useful directly, since asar_init() verifies this automatically.
//Calling this one also sets a flag that makes asar_init not instantly return false; this is so
// programs expecting an older API won't do anything unexpected.
asarfunc int (*asar_apiversion)();

//Initializes Asar. Call this before doing anything.
//If it returns false, something went wrong, and you may not use any other Asar functions. This is
//either due to not finding the library, or not finding all expected functions in the library.
bool asar_init();

//Clears out all errors, warnings and printed statements, and clears the file cache. Not really
// useful, since asar_patch() already does this.
asarfunc bool (*asar_reset)();

//Applies a patch. The first argument is a filename (so Asar knows where to look for incsrc'd
// stuff); however, the ROM is in memory.
//This function assumes there are no headers anywhere, neither in romdata nor the sizes. romlen may
// be altered by this function; if this is undesirable, set romlen equal to buflen.
//The return value is whether any errors appeared (false=errors, call asar_geterrors for details).
// If there is an error, romdata and romlen will be left unchanged.
asarfunc bool (*asar_patch)(const char * patchloc, char * romdata, int buflen, int * romlen);

//Returns the maximum possible size of the output ROM from asar_patch(). Giving this size to buflen
// guarantees you will not get any buffer too small errors; however, it is safe to give smaller
// buffers if you don't expect any ROMs larger than 4MB or something.
asarfunc int (*asar_maxromsize)();

//Frees all of Asar's structures and unloads the module. Only asar_init may be called after calling
// this; anything else will lead to segfaults.
void asar_close();

//Get a list of all errors.
//All pointers from these functions are valid only until the same function is called again, or until
// asar_patch, asar_reset or asar_close is called, whichever comes first. Copy the contents if you
// need it for a longer time.
asarfunc const struct errordata * (*asar_geterrors)(int * count);

//Get a list of all warnings.
asarfunc const struct errordata * (*asar_getwarnings)(int * count);

//Get a list of all printed data.
asarfunc const char * const * (*asar_getprints)(int * count);

//Get a list of all labels.
asarfunc const struct labeldata * (*asar_getalllabels)(int * count);

//Get the ROM location of one label. -1 means "not found".
asarfunc int (*asar_getlabelval)(const char * name);

//Gets the value of a define.
asarfunc const char * (*asar_getdefine)(const char * name);

//Gets the values and names of all defines.
asarfunc const struct definedata * (*asar_getalldefines)(int * count);

//Parses all defines in the parameter. The parameter controls whether it'll learn new defines in
// this string if it finds any. Note that it may emit errors.
asarfunc const char * (*asar_resolvedefines)(const char * data, bool learnnew);

//Parses a string containing math. It automatically assumes global scope (no namespaces), and has
// access to all functions and labels from the last call to asar_patch. Remember to check error to
// see if it's successful (NULL) or if it failed (non-NULL, contains a descriptive string). It does
// not affect asar_geterrors.
asarfunc double (*asar_math)(const char * math, const char ** error);
