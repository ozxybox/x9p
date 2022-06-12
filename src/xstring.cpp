#include "xstring.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>


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

// String copying - Does not check dest length
xstr_t xstrncpy_nocheck(xstr_t dest, const xstr_t src)
{
	size_t sz = src->len;
	if (sz != 0)
		memcpy(dest->str(), src->str(), sz);
	dest->len = sz;
	return dest;
}

xstr_t xstrncpy_nocheck(xstr_t dest, const xstr_t src, xstrlen_t size)
{
	xstrlen_t sz = size;
	if (src->len < sz)
		sz = src->len;

	if (sz != 0)
		memcpy(dest->str(), src->str(), sz);
	dest->len = sz;
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
		unsigned char lc;
		unsigned char rc = *c;

		// At the end yet?
		// Needed as xstrs are not null terminated
		bool eor = rc == 0;
		bool eol = i >= len;

		// We don't want to access outside of our bounds
		if (eol)
			lc = 0;
		else
			lc = ((unsigned char*)l->str())[i];

		
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
		unsigned char rc;

		// At the end yet?
		// Needed as xstrs are not null terminated
		bool eol = lc == 0;
		bool eor = i >= len;

		// We don't want to access outside of our bounds
		if (eor)
			rc = 0;
		else
			rc = ((unsigned char*)r->str())[i];


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

// String searching
char* xstrchr(const xstr_t str, char character)
{
	char* s = str->str();
	for (int i = 0; i < str->len; i++)
	{
		if (s[i] == character)
			return s + i;
	}
	return 0;
}
char* xstrrchr(const xstr_t str, char character)
{
	char* s = str->str();
	for (int i = str->len - 1; i >= 0; i--)
	{
		if (s[i] == character)
			return s + i;
	}
	return 0;
}

/////////////////////////////
// String conversion utils //
/////////////////////////////



static size_t xstrfromd_decimals(int d, int radix)
{
	// Zero will always be 0
	if (d == 0)
		return 1;

	// Radix 1 is like tally marks. Each increment is another char
	if (radix == 1)
		return d;
	
	// ceil of the log base radix of d 
	return ceil(log1p(d) / log(radix));
}


static void xstrfromd_copyinto(int d, int radix, int decimals, xstr_t s)
{
	const char* radix_pallet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		
	if (radix > 36)
		radix = 36;
	else if (radix < 1)
		radix = 1;

	int f = d;

	char* c = s->str() + s->len - 1;
	for (int i = 0; i < decimals; i++)
	{
		int v = f % radix;
		f /= radix;

		*c = radix_pallet[v];
		c--;
	}

}

xstr_t xstrfromd(int d, int radix)
{
	// Make sure it's positive
	bool negative;
	if ((negative = d < 0))
		d *= -1;

	size_t decimals = xstrfromd_decimals(d, radix);
	
	// We need one more char for a negative
	size_t chars = decimals + (negative ? 1 : 0);

	assert(chars <= XSTRLEN_MAX);

	xstr_t s = (xstr_t)malloc(sizeof(xstrlen_t) + chars);
	s->len = chars;

	// Insert the sign
	if (negative)
		s->str()[0] = '-';

	xstrfromd_copyinto(d, radix, decimals, s);

	return s;
}


size_t xstrnfromd(int d, int radix, xstr_t out, size_t avail)
{
	// Make sure it's positive
	bool negative;
	if ((negative = d < 0))
		d *= -1;

	size_t decimals = xstrfromd_decimals(d, radix);

	// We need one more char for a negative
	size_t chars = decimals + (negative ? 1 : 0);

	assert(chars <= XSTRLEN_MAX);

	// If we were told to write to null, stop now and return how many bytes we would write if we could
	if (out)
	{
		if (avail > sizeof(xstrlen_t))
		{
			// How long is our output going to be?
			avail -= sizeof(xstrlen_t);
			if (chars > avail)
			{
				out->len = avail;
				decimals = avail - (negative ? 1 : 0); // TODO: Check negative truncated numbers!
			}
			else
				out->len = chars;

			// Insert the sign
			if (negative)
				out->str()[0] = '-';

			// TODO: Check truncated numbers!
			xstrfromd_copyinto(d, radix, decimals, out);
		}
		else if (avail == sizeof(xstrlen_t))
		{
			// If we only have the exact amount of bytes to write the length, mark it as 0 and return how much we'd like to write
			out->len = 0;
		}
		// Else, we do not have enough bytes a string. Stop and return how much we'd need
	}

	// Return how many byes we'd like to write normally
	return sizeof(xstrlen_t) + chars;
}



bool xstrtod(xstr_t str, int radix, int* out)
{
	if (str->len == 0) return false;

	char* s = str->str();
	int negative = s[0] == '-';

	if (negative && str->len == 1) return false;


	int d = 0;
	if (radix < 1 || radix > 36)
	{
		// Invalid radix
		return false;
	}
	else if (radix == 1)
	{
		// Radix 1 is like having tally marks.
		// Anything can go as a tally, and the count is all that matters
		if (out)
			*out = str->len;
		return true;
	}
	else if (radix <= 10)
	{
		// Numbers only

		for (int i = negative; i < str->len; i++)
		{
			char c = s[i];

			// 0123456789
			unsigned char v = c - '0';
			if (v >= radix)
				return false;

			d = d * radix + v;
		}
	}
	else if (radix <= 36)
	{
		// Numbers and letters

		for (int i = negative; i < str->len; i++)
		{
			char c = s[i];
			
			// 0123456789abcdefghijklmnopqrstuvwxyz
			// 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ

			unsigned char v = c - '0';
			if (v >= 10)
			{
				// Allow both upper and lower case
				if (c >= 'a')
					v = c - 'a';
				else
					v = c - 'A';

				if (v >= radix - 10)
					return false;

				v += 10;
			}


			d = d * radix + v;
		}
	}

	if (negative) d *= -1;

	if (out) *out = d;

	return true;
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




template<bool CaseInvariant, bool UseLen>
size_t _xstrhash(const xstr_t str, xstrlen_t len = 0)
{
	char* s = str->str();
	
	// Are we in xstrnhash mode?
	if constexpr (UseLen)
	{
		if (len > str->len)
			len = str->len;
	}
	else
	{
		len = str->len;
	}

	// Hash of all chars in the string
	size_t hash = BASIS;
	for (xstrlen_t i = 0; i < len; i++)
	{
		char c;
		if constexpr (CaseInvariant)
			c = toupper(s[i]);
		else
			c = s[i];

		hash = (hash ^ c) * PRIME;
	}
	return hash;
}

size_t xstrhash (const xstr_t str) { return _xstrhash<false, false>(str); }
size_t xstrihash(const xstr_t str) { return _xstrhash<true,  false>(str); }

size_t xstrnhash (const xstr_t str, xstrlen_t len) { return _xstrhash<false, true>(str, len); }
size_t xstrnihash(const xstr_t str, xstrlen_t len) { return _xstrhash<true,  true>(str, len); }


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


void xstrwalkfrompath(const char* path, int& nwalk, xstr_t& walk)
{
	// Count up how many bytes we need
	int sections = 1;
	int sz = 0;
	for (const char* c = path; *c; c++)
	{
		char k = *c;
		if (k == '/' || k == '\\')
			sections++;
		else
			sz++;
	}


	// Make space for our output
	size_t psz = sz + sections * sizeof(xstrlen_t);
	char* arr = (char*)malloc(psz);
	xstr_t s = (xstr_t)arr;


	// Copy into arr
	int welem = 0;
	const char* start = path, * c = start;
	for (; ; c++)
	{
		char k = *c;
		if (k == '/' || k == '\\' || k == '\0')
		{
			xstrlen_t len = c - start;
			
			if (len == 0 || (len == 1 && *start == '.'))
			{
				// Do not transmit!
				start = c + 1;

				if (k == '\0')
					break;
				continue;
			}
			else
			{
				// Copy it in
				s->len = len;
				memcpy(s->str(), start, len);
				s = xstrnext(s);
				start = c + 1;

				welem++;
			}
		}

		if (k == '\0')
			break;
	}

	nwalk = welem;
	walk = (xstr_t)arr;
}
