#pragma once
#include <stdint.h>

//////////////////////////////////////////////////////////////////
// 9P messages implement strings as 16 bit Pascal Strings       //
// These are not C Strings!                                     //
//                                                              //
// These strings should NEVER be assumed to be null-terminated! //
//////////////////////////////////////////////////////////////////

typedef uint16_t xstrlen_t;
typedef struct 
{
	xstrlen_t len;
	char* str() { return reinterpret_cast<char*>(this) + sizeof(xstrlen_t); }

	// This is the size of this structure in bytes INCLUDING "len" and "str"
	uint32_t size() { return sizeof(xstrlen_t) + len; }
}* xstr_t;

template<unsigned N>
inline
#if defined(__cpp_consteval)
consteval
#else
constexpr
#endif
auto _xstr_literal(const char(&str)[N])
{
	struct
	{
		xstrlen_t len = N - 1;
		char buf[N - 1];

		xstr_t xstr() { return reinterpret_cast<xstr_t>(this); }
	} lit;

	for (xstrlen_t i = 0; i < N - 1; i++)
		lit.buf[i] = str[i];

	return lit;
}

// Use this macro to get an xstr of a string literal
#define XSTR_L(str) _xstr_literal(str).xstr()



/////////////////////////
// string.h look-alike //
/////////////////////////

// String Duplicating
xstr_t xstrndup(const char* str, xstrlen_t length);
xstr_t xstrdup( const char* str);
xstr_t xstrdup(const xstr_t str);

// String Copying
xstr_t xstrcpy( xstr_t dest, const xstr_t src);
xstr_t xstrncpy(xstr_t dest, const xstr_t src, xstrlen_t size);
xstr_t xstrcpy(xstr_t dest,	 const char*  src);
char*  xstrncpy(char* dest,  const xstr_t src, xstrlen_t size);
xstr_t xstrncpy(xstr_t dest, const char* src,  xstrlen_t size);

// String Comparison
int xstrcmp( const xstr_t l, const xstr_t r);
int xstrcmp( const xstr_t l, const  char* r);
int xstrcmp( const  char* l, const xstr_t r);


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
// The const char* versions exist for compatibility with C strings
size_t xstrhash( const xstr_t str);
size_t xstrnhash(const xstr_t str, xstrlen_t len);
size_t xstrnhash(const  char* str, xstrlen_t len);
size_t xstrhash( const  char* str);


// Utils for STD containers
struct xstrhash_t
{
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