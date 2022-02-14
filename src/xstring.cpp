#include "xstring.h"

#include <stdlib.h>
#include <string.h>


//////////////////////////
// string.h look-alikes //
//////////////////////////

// String duplicating
xstr_t xstrndup(const char* str, xstrlen_t length)
{
	xstr_t d = (xstr_t)malloc(sizeof(xstrlen_t) + length);

	if (length != 0)
		memcpy(d->str(), str, length);
	d->len = length;
	return d;
}
xstr_t xstrdup(const xstr_t str)
{
	return xstrndup(str->str(), str->len);
}
xstr_t xstrdup(const char* str)
{
	return xstrndup(str, strlen(str));
}

// String copying
xstr_t xstrcpy(xstr_t dest, const xstr_t src)
{
	size_t sz = dest->len;
	if (src->len < sz)
		sz = src->len;

	if (sz != 0)
		memcpy(dest->str(), src->str(), sz);
	dest->len = sz;
	return dest;
}
xstr_t xstrncpy(xstr_t dest, const xstr_t src, xstrlen_t size)
{
	xstrlen_t sz = size;
	if (src->len < sz)
		sz = src->len;
	
	if (sz != 0)
		memcpy(dest->str(), src->str(), sz);
	dest->len = sz;
	return dest;
}
xstr_t xstrcpy(xstr_t dest, const char* src)
{
	return xstrncpy(dest, src, strlen(src));
}
char* xstrncpy(char* dest, const xstr_t src, xstrlen_t size)
{
	xstrlen_t sz = size;
	if (src->len < sz)
		sz = src->len;

	if (sz != 0)
		memcpy(dest, src->str(), sz);
	return dest;
}
xstr_t xstrncpy(xstr_t dest, const char* src, xstrlen_t size)
{
	// Hmm, sketchy? I'd like to check dest's size to not over copy, but dest is likely to be uninitalized
	if(size != 0)
		memcpy(dest->str(), src, size);
	dest->len = size;
	return dest;

}

// String Comparison
int xstrcmp(const xstr_t l, const xstr_t r)
{
	// Equal length
	if (l->len == r->len)
		return memcmp(l->str(), r->str(), r->len);
	
	// Left is longer
	if (l->len > r->len)
		return 1;
	
	// Right is longer
	return -1;
}
int xstrcmp(const xstr_t l, const char* r)
{
	int i = 0;
	int len = l->len;
	for (const unsigned char* c = (const unsigned char*)r;;)
	{
		unsigned char lc = ((unsigned char*)l->str())[i];
		unsigned char rc = *c;

		// At the end yet?
		// Needed as xstrs are not null terminated
		bool eor = rc == 0;
		bool eol = i >= len;
		
		// Ends at the same time
		if (eol && eor)
			return 0;

		// Left is longer
		if (eol && !eor)
			return 1;

		// Right is longer
		if (eol && !eor)
			return -1;

		// Are these chars equal?
		if (lc > rc)
			return 1;
		if (lc < rc)
			return -1;

		i++;
		c++;
	}

	// Equal!
	return 0;
}
int xstrcmp(const char* l, const xstr_t r)
{
	int i = 0;
	int len = r->len;
	for (const unsigned char* c = (const unsigned char*)l;;)
	{
		unsigned char lc = *c;
		unsigned char rc = ((unsigned char*)r->str())[i];

		// At the end yet?
		// Needed as xstrs are not null terminated
		bool eor = rc == 0;
		bool eol = i >= len;

		// Ends at the same time
		if (eol && eor)
			return 0;

		// Left is longer
		if (eol && !eor)
			return 1;

		// Right is longer
		if (eol && !eor)
			return -1;

		// Are these chars equal?
		if (lc > rc)
			return 1;
		if (lc < rc)
			return -1;

		i++;
		c++;
	}

	// Equal	
	return 0;
}


/////////////////////////////
// Supporting string utils // 
/////////////////////////////


xstr_t xstrsplit(const char* str, char delimiter, int* segments)
{
	// Count up how many bytes we need
	int sections = 1;
	int sz = 0;
	for (const char* c = str; *c; c++)
	{
		if (*c == delimiter)
			sections++;
		else
			sz++;
	}

	// Make space for our output
	char* arr = (char*)malloc(sz + sections * sizeof(xstrlen_t));

	// Copy into arr
	xstr_t s = (xstr_t)arr;
	const char* start = str, * c = start;
	for (; *c; c++)
	{
		if (*c == delimiter)
		{
			xstrlen_t len = c - start;
			s->len = len;
			memcpy(s->str(), start, len);
			s = xstrnext(s);
			start = c + 1;
		}
	}
	// Copy in the last section
	xstrlen_t len = c - start;
	if (len != 0)
	{
		s->len = len;
		memcpy(s->str(), start, len);
	}

	if (segments) *segments = sections;
	return (xstr_t)arr;
}

char* xstrcstr(const xstr_t str)
{
	char* cstr = (char*)malloc(str->len + 1);
	memcpy(cstr, str->str(), str->len);
	cstr[str->len] = 0;
	return cstr;
}

xstr_t xstrnext(const xstr_t str)
{
	return reinterpret_cast<xstr_t>(str->str() + str->len);
}

bool xstrsafe(const xstr_t str, const void* end)
{
	const char* cend = reinterpret_cast<const char*>(end);
	return reinterpret_cast<const char*>(str) + sizeof(xstrlen_t) <= end && str->str() + str->len <= cend;
}

/////////////
// Hashing //
/////////////

// FNV-1a hash
// We've got diffent values on different platforms
// It might be useful to just stick with 32-bit if hashes need to be wired over the network
#if _WIN64 || __x86_64__ || __ppc64__
static const size_t BASIS = 0xcbf29ce484222325;
static const size_t PRIME = 0x100000001b3;
#else
static const size_t BASIS = 0x811c9dc5;
static const size_t PRIME = 0x01000193;
#endif
size_t xstrhash(const xstr_t str)
{
	char* s = str->str();
	xstrlen_t len = str->len;

	// Hash of all chars in the string
	size_t hash = BASIS;
	for (xstrlen_t i = 0; i < len; i++)
		hash = (hash ^ s[i]) * PRIME;
	return hash;
}
size_t xstrnhash(const xstr_t str, xstrlen_t len)
{
	char* s = str->str();
	if (len > str->len) len = str->len;

	// Hash of all chars in the string
	size_t hash = BASIS;
	for (xstrlen_t i = 0; i < len; i++)
		hash = (hash ^ s[i]) * PRIME;
	return hash;
}

// C String compatibility
size_t xstrhash(const char* str)
{
	// Hash of all chars in the string, stop on null
	size_t hash = BASIS;
	for (const char* c = str; *c; c++)
		hash = (hash ^ *c) * PRIME;
	return hash;
}
size_t xstrnhash(const char* str, xstrlen_t len)
{
	// Hash of all chars in the string, stop on null
	size_t hash = BASIS;
	for (xstrlen_t i = 0; str[i] && i < len; i++)
		hash = (hash ^ str[i]) * PRIME;
	return hash;
}


// Utils for STD containers
size_t xstrhash_t::operator()(const xstr_t& str) const { return xstrhash(str); }
size_t xstrhash_t::operator()(const  char*& str) const { return xstrhash(str); }
bool xstrequality_t::operator()(const xstr_t& l, const xstr_t& r) const { return xstrcmp(l, r) == 0; }
bool xstrequality_t::operator()(const  char*& l, const  char*& r) const { return  strcmp(l, r) == 0; }
bool xstrequality_t::operator()(const xstr_t& l, const  char*& r) const { return xstrcmp(l, r) == 0; }
bool xstrequality_t::operator()(const  char*& l, const xstr_t& r) const { return xstrcmp(l, r) == 0; }


#include <stdio.h>
void xstrprint(const xstr_t str)
{
	if (!str) return;

	fwrite(str->str(), sizeof(char), str->len, stdout);
}