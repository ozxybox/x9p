#pragma once
#include <stdint.h>

//////////////////////////////////////////////////////////////////
// 9P messages implement strings as 16 bit Pascal Strings       //
// These are not C Strings!                                     //
//                                                              //
// These strings should NEVER be assumed to be null-terminated! //
//////////////////////////////////////////////////////////////////

typedef uint16_t xstrlen_t;
#define XSTRLEN_MAX ((xstrlen_t)0xFFFF)


// xstr_t: Length prefixed strings
typedef struct __xstrinternaldata_t
{
	xstrlen_t len;
	char* str() { return reinterpret_cast<char*>(this) + sizeof(xstrlen_t); }

	// This is the size of this structure in bytes INCLUDING "len" and "str"
	uint32_t size() { return sizeof(xstrlen_t) + len; }

	// Used only by xstrconst_t
protected:
	constexpr __xstrinternaldata_t(xstrlen_t l) : len(l) {}
}* xstr_t;


// xstrconst_t: Compile time length prefixed strings
//              Just use XSTRL instead
template<xstrlen_t N>
struct xstrconst_t : private __xstrinternaldata_t
{
	constexpr xstrconst_t(xstrlen_t l) : __xstrinternaldata_t(l) {};
	constexpr xstr_t xstr() const { return (const xstr_t)(this); }

	char buf[N] = {};
};
template<>
struct xstrconst_t<0> : private __xstrinternaldata_t
{
	constexpr xstrconst_t(xstrlen_t l) : __xstrinternaldata_t(l) {};
	constexpr xstr_t xstr() const { return (const xstr_t)(this); }

	char buf = 0;
};

// N - 1, as we need to chop the \0 off
template<unsigned N>
constexpr xstrconst_t<N - 1> xstrliteral(const char(&str)[N])
{
	constexpr xstrlen_t len = N - 1;
	xstrconst_t<len> lit(len);

	for (int i = 0; i < len; i++)
		lit.buf[i] = str[i];

	return lit;
}

template<>
constexpr xstrconst_t<0> xstrliteral(const char(&str)[1])
{
	xstrconst_t<0> lit(0);
	return lit;
}

// Use this macro to get an xstr of a string literal
// This will create a static variable to hold the value, so
// your pointer should be valid past the scope of the function
// This should be seamless and performant on Release and still very good on Debug
#define XSTRL(str)                                  \
([]() noexcept -> const xstr_t {                    \
static constexpr auto s_strlit = xstrliteral(str);  \
static constexpr xstr_t s_strptr = s_strlit.xstr(); \
return s_strptr; }())




/////////////////////////
// string.h look-alike //
/////////////////////////

// String Duplicating //
xstr_t xstrndup(const  char* str, xstrlen_t length);
xstr_t xstrdup (const  char* str);
xstr_t xstrdup (const xstr_t str);

// String Copying //
xstr_t xstrcpy (xstr_t dest, const xstr_t src);
xstr_t xstrcpy (xstr_t dest, const  char* src);
xstr_t xstrncpy(xstr_t dest, const xstr_t src, xstrlen_t size);
 char* xstrncpy( char* dest, const xstr_t src, xstrlen_t size);
xstr_t xstrncpy(xstr_t dest, const  char* src, xstrlen_t size);

// String Copying - Does not check the dest's length //
xstr_t xstrncpy_nocheck(xstr_t dest, const xstr_t src);
xstr_t xstrncpy_nocheck(xstr_t dest, const xstr_t src, xstrlen_t size);

// String Comparison //
int xstrcmp(const xstr_t l, const xstr_t r);
int xstrcmp(const xstr_t l, const  char* r);
int xstrcmp(const  char* l, const xstr_t r);


// String searching //

// Return a pointer to the first occurrence of "character" in "str"
// Return null if "character" could not be found
char* xstrchr(const xstr_t str, char character);

// Return a pointer to the last occurrence of "character" in "str"
// Return null if "character" could not be found
char* xstrrchr(const xstr_t str, char character);


/////////////////////////////
// String conversion utils //
/////////////////////////////

// Converts an integer "d" into a string of base "radix"
// Supports radix 1 to 36
xstr_t xstrfromd(int d, int radix);
size_t xstrnfromd(int d, int radix, xstr_t out, size_t avail);


// Converts a string "str" of base "radix" into an integer at "out"
// Supports radix 1 to 36
// Returns true on a successful parse or false on failure
bool xstrtod(xstr_t str, int radix, int* out);


/////////////////////////////
// Supporting string utils // 
/////////////////////////////

xstr_t xstrsplit(const char* str, char delimiter, int* segments);

// Converts a xstr to a cstr
char* xstrcstr(const xstr_t str);

// Next string stored right after this one in memory
// Needed for pulling off a 9P message, but should NOT be used anywhere else!
xstr_t xstrnext(const xstr_t str);

// Lets us know if our 9P messages are trying to lie to us by checking if str goes past end
bool xstrsafe(const xstr_t str, const void* end);

// FNV-1a Hash
size_t xstrhash  (const xstr_t str);
size_t xstrihash (const xstr_t str);
size_t xstrnhash (const xstr_t str, xstrlen_t len);
size_t xstrnihash(const xstr_t str, xstrlen_t len);

// The const char* versions exist for compatibility with C strings
size_t xstrhash (const char* str);
size_t xstrnhash(const char* str, xstrlen_t len);


// Utils for STD containers
struct xstrhash_t
{
	using is_transparent = void;

	size_t operator()(const xstr_t& str) const;
	size_t operator()(const  char*& str) const;
};
struct xstrequality_t
{
	bool operator() (const xstr_t& l, const xstr_t& r) const;
	bool operator() (const  char*& l, const  char*& r) const;
	bool operator() (const xstr_t& l, const  char*& r) const;
	bool operator() (const  char*& l, const xstr_t& r) const;
};


// Should get moved to a proper logging system soon
void xstrprint(const xstr_t str);


// Standard slash delimited filesystem paths to 9P walks
void xstrwalkfrompath(const char* path, int& nwalk, xstr_t& walk);
