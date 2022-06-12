#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "xstring.h"

// Note: A large part of this is byte endian dependent.
//       I do not think it matters at all.
//       Don't fix it.
//



#define CHECK(cond) if(!(cond)) return false;

#define TEST(funcname) {#funcname, test_##funcname},
struct test_t
{
	const char* name;
	bool(*func)();
};


////////////////////////////////////////
// Extra Data for Unit Test Functions //
////////////////////////////////////////


// 0xFFFF as a string from base 2 to base 36
static const char* s_strdbasetest[] = {
	"\020\0001111111111111111",
	"\013\00010022220020",
	"\010\00033333333",
	"\007\0004044120",
	"\007\0001223223",
	"\006\000362031",
	"\006\000177777",
	"\006\000108806",
	"\005\00065535",
	"\005\00045268",
	"\005\00031B13",
	"\005\00023AA2",
	"\005\00019C51",
	"\005\00014640",
	"\004\000FFFF",
	"\004\000D5D0",
	"\004\000B44F",
	"\004\0009AA4",
	"\004\00083GF",
	"\004\00071CF",
	"\004\000638J",
	"\004\00058K8",
	"\004\0004HIF",
	"\004\00044LA",
	"\004\0003IOF",
	"\004\00038O6",
	"\004\0002RGF",
	"\004\0002JQO",
	"\004\0002COF",
	"\004\0002661",
	"\004\0001VVV",
	"\004\0001R5U",
	"\004\0001MNH",
	"\004\0001IHF",
	"\004\0001EKF",
};

static const xstr_t* s_xstrdbasetest = (const xstr_t*)s_strdbasetest;


/////////////////////////
// Unit Test Functions //
/////////////////////////

bool test_XSTRL()
{
	CHECK(memcmp(XSTRL(""),     "\000\000",     2) == 0);
	CHECK(memcmp(XSTRL("1"),    "\001\0001",    3) == 0);
	CHECK(memcmp(XSTRL("12"),   "\002\00012",   4) == 0);
	CHECK(memcmp(XSTRL("123"),  "\003\000123",  5) == 0);
	CHECK(memcmp(XSTRL("1234"), "\004\0001234", 6) == 0);
	return true;
}


bool test_xstrcmp()
{
	CHECK(xstrcmp("o",   (const xstr_t)("\001\000o"))   == 0);
	CHECK(xstrcmp("on",  (const xstr_t)("\002\000on"))  == 0);
	CHECK(xstrcmp("one", (const xstr_t)("\003\000one")) == 0);
	return true;
}

bool test_xstrfromd()
{
	xstr_t str;

	// Positive Base 10
	str = xstrfromd(0xFFFF, 10);
	CHECK(memcmp(str, "\005\00065535", 7) == 0);
	free(str);

	// Positive Base 16
	str = xstrfromd(0xFFFF, 16);
	CHECK(memcmp(str, "\004\000FFFF", 6) == 0);
	free(str);


	// Negative Base 10
	str = xstrfromd(-0xFFFF, 10);
	CHECK(memcmp(str, "\006\000-65535", 8) == 0);
	free(str);

	// Negative Base 16
	str = xstrfromd(-0xFFFF, 16);
	CHECK(memcmp(str, "\005\000-FFFF", 7) == 0);
	free(str);


	// Zero Base 10
	str = xstrfromd(0, 10);
	CHECK(memcmp(str, "\001\0000", 3) == 0);
	free(str);

	// Zero Base 16
	str = xstrfromd(0, 16);
	CHECK(memcmp(str, "\001\0000", 3) == 0);
	free(str);


	// Base 1
	str = xstrfromd(8, 1);
	CHECK(memcmp(str, "\010\00000000000", 10) == 0);
	free(str);



	// Test Bases 2 to 36
	for (int i = 0; i < 35; i++)
	{
		str = xstrfromd(0xFFFF, i + 2);
		CHECK(xstrcmp(s_xstrdbasetest[i], str) == 0);
		free(str);
	}

	return true;
}

bool checkZeros(char* buf, size_t size)
{
	xstr_t str = reinterpret_cast<xstr_t>(buf);
	for (int i = str->size(); i < size; i++)
		if (buf[i]) return false;
	return true;
}

bool test_xstrnfromd()
{

	char buf[32];
	xstr_t str = reinterpret_cast<xstr_t>(&buf[0]);
	int r;


	// Positive
	memset(&buf[0], 0, sizeof(buf));
	r = xstrnfromd(0xFFFF, 10, str, sizeof(buf));
	CHECK(r == 7);
	CHECK(memcmp(str, "\005\00065535", 7) == 0);
	CHECK(checkZeros(&buf[0], sizeof(buf)));

	// Negative
	memset(&buf[0], 0, sizeof(buf));
	r = xstrnfromd(-0xFFFF, 10, str, sizeof(buf));
	CHECK(r == 8);
	CHECK(memcmp(str, "\006\000-65535", 8) == 0);
	CHECK(checkZeros(&buf[0], sizeof(buf)));


	// TODO: Add more truncate tests

	return true;
}


bool test_xstrtod()
{
	int d = 0;
	bool b = false;

	// Positive Base 10
	d = INT_MAX;
	b = xstrtod((const xstr_t)"\005\00065535", 10, &d);
	CHECK(b && d == 0xFFFF);

	// Positive Base 16
	d = INT_MAX;
	b = xstrtod((const xstr_t)"\004\000FFFF", 16, &d);
	CHECK(b && d == 0xFFFF);


	// Negative Base 10
	d = INT_MAX;
	b = xstrtod((const xstr_t)"\006\000-65535", 10, &d);
	CHECK(b && d == -0xFFFF);

	// Negative Base 16
	d = INT_MAX;
	b = xstrtod((const xstr_t)"\005\000-FFFF", 16, &d);
	CHECK(b && d == -0xFFFF);


	// Zero Base 10
	d = INT_MAX;
	b = xstrtod((const xstr_t)"\001\0000", 10, &d);
	CHECK(b && d == 0);

	// Zero Base 16
	d = INT_MAX;
	b = xstrtod((const xstr_t)"\001\0000", 16, &d);
	CHECK(b && d == 0);
	

	// Base 1
	d = INT_MAX;
	b = xstrtod((const xstr_t)"\010\00000000000", 1, &d);
	CHECK(b && d == 8);


	// Test Bases 2 to 36
	for (int i = 0; i < 35; i++)
	{
		d = INT_MAX;
		b = xstrtod(s_xstrdbasetest[i], i + 2, &d);
		CHECK(b && d == 0xFFFF);
	}
	

	return true;
}


bool test_xstrtod_xstrfromd()
{
	const int count = 0xFFFF;

	xstr_t str;
	int d;
	bool success;

	// Bases 2 to 36
	for (int b = 2; b <= 36; b++)
	{
		for (int i = 0; i <= count; i++)
		{
			// Positive
			d = INT_MAX;
			str = xstrfromd(i, b);
			success = xstrtod(str, b, &d);

			free(str);

			CHECK(success && d == i);

			// Negative
			d = INT_MAX;
			str = xstrfromd(-i, b);
			success = xstrtod(str, b, &d);

			free(str);

			CHECK(success && d == -i);
		}
	}


	return true;

}

bool test_xstrtod_xstrnfromd()
{
	const int count = 0xFFFF;
	
	const int sz = 32;
	char buf[sz];
	xstr_t str = reinterpret_cast<xstr_t>(&buf[0]);
	
	int d, r;
	bool success;

	// Bases 2 to 36
	for (int b = 2; b <= 36; b++)
	{
		for (int i = 0; i <= count; i++)
		{
			// Positive
			d = INT_MAX;
			memset(&buf[0], 0, sz);

			r = xstrnfromd(i, b, str, sz);
			success = xstrtod(str, b, &d);

			CHECK(r == str->size());
			CHECK(success && d == i);
			CHECK(checkZeros(&buf[0], sz));


			// Negative
			d = INT_MAX;
			memset(&buf[0], 0, sz);

			r = xstrnfromd(-i, b, str, sz);
			success = xstrtod(str, b, &d);

			CHECK(r == str->size());
			CHECK(success && d == -i);
			CHECK(checkZeros(&buf[0], sz));
		}
	}


	return true;

}


bool test_xstrhash()
{
	size_t hash = xstrhash((xstr_t)"\011\000123hello!");
	
	CHECK(xstrhash((xstr_t)"\011\000123Hello!") != hash);
	CHECK(xstrhash((xstr_t)"\011\000123HEllo!") != hash);
	CHECK(xstrhash((xstr_t)"\011\000123HELlo!") != hash);
	CHECK(xstrhash((xstr_t)"\011\000123HELLo!") != hash);
	CHECK(xstrhash((xstr_t)"\011\000123HELLO!") != hash);
	CHECK(xstrhash((xstr_t)"\011\000123hELLO!") != hash);
	CHECK(xstrhash((xstr_t)"\011\000123heLLO!") != hash);
	CHECK(xstrhash((xstr_t)"\011\000123helLO!") != hash);
	CHECK(xstrhash((xstr_t)"\011\000123hellO!") != hash);

	CHECK(xstrhash((xstr_t)"\011\000123hello!") == hash);

	return true;
}

bool test_xstrihash()
{
	size_t hash = xstrihash((xstr_t)"\011\000123hello!");

	CHECK(xstrihash((xstr_t)"\011\000123Hello!") == hash);
	CHECK(xstrihash((xstr_t)"\011\000123HEllo!") == hash);
	CHECK(xstrihash((xstr_t)"\011\000123HELlo!") == hash);
	CHECK(xstrihash((xstr_t)"\011\000123HELLo!") == hash);
	CHECK(xstrihash((xstr_t)"\011\000123HELLO!") == hash);
	CHECK(xstrihash((xstr_t)"\011\000123hELLO!") == hash);
	CHECK(xstrihash((xstr_t)"\011\000123heLLO!") == hash);
	CHECK(xstrihash((xstr_t)"\011\000123helLO!") == hash);
	CHECK(xstrihash((xstr_t)"\011\000123hellO!") == hash);
	CHECK(xstrihash((xstr_t)"\011\000123hello!") == hash);

	return true;
}


bool test_xstrnhash()
{
	size_t hash = xstrnhash((xstr_t)"\011\000123hello!", 6);
	
	CHECK(xstrnhash((xstr_t)"\011\000123Hello!", 6) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123HEllo!", 6) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123HELlo!", 6) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123HELLo!", 6) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123HELLO!", 6) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hELLO!", 6) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123heLLO!", 6) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123helLO!", 6) == hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hellO!", 6) == hash);
	CHECK(xstrnhash((xstr_t)"\011\000123helLO_", 6) == hash);

	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 9) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 8) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 7) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 6) == hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 5) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 4) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 3) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 2) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 1) != hash);
	CHECK(xstrnhash((xstr_t)"\011\000123hello!", 0) != hash);

	return true;
}

bool test_xstrnihash()
{
	size_t hash = xstrnihash((xstr_t)"\011\000123hello!", 6);

	CHECK(xstrnihash((xstr_t)"\011\000123Hello!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123HEllo!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123HELlo!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123HELLo!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123HELLO!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hELLO!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123heLLO!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123helLO!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hellO!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 6) == hash);

	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 9) != hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 8) != hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 7) != hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 6) == hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 5) != hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 4) != hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 3) != hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 2) != hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 1) != hash);
	CHECK(xstrnihash((xstr_t)"\011\000123hello!", 0) != hash);

	return true;
}

bool test_xstrcstr()
{
	char* c = xstrcstr((xstr_t)"\011\000123hello!");
	bool res = memcmp("123hello!", c, 10) == 0;
	free(c);
	CHECK(res);

	return true;
}


////////////////////////////////
// End of Unit Test Functions //
////////////////////////////////



static test_t s_unittests[] = {
	TEST(XSTRL)
	TEST(xstrcmp)
	TEST(xstrfromd)
	TEST(xstrnfromd)
	TEST(xstrtod)
	TEST(xstrtod_xstrfromd)
	TEST(xstrtod_xstrnfromd)
	TEST(xstrhash)
	TEST(xstrihash)
	TEST(xstrnhash)
	TEST(xstrnihash)
	TEST(xstrcstr)
/*
xstrndup
xstrcpy
xstrncpy
xstrchr
xstrrchr
xstrsplit
xstrnext
xstrhash_t
xstrequality_t
xstrprint
xstrwalkfrompath
*/
};

static int s_unittestcount = sizeof(s_unittests) / sizeof(test_t);

void list_tests()
{
	puts("\tall");
	for (int i = 0; i < s_unittestcount; i++)
	{
		putchar('\t');
		puts(s_unittests[i].name);
	}
}

bool run_test(test_t* selected)
{
	printf(".... : \"%s\"", selected->name);

	// Run the test
	bool success = selected->func();

	if (success)
		printf("\rPASS\n");
	else
		printf("\rFAIL\n");

	return success;
}

int main(int argc, const char** args)
{
	if (argc < 2)
	{
		puts("Expecting a test as an argument!");
		list_tests();
		return 1;
	}

	int fails = 0;
	if (strcmp("all", args[1]) == 0)
	{
		// Run all tests
		for (int i = 0; i < s_unittestcount; i++)
		{
			fails += run_test(&s_unittests[i]) ? 0 : 1;
		}
		return fails;
	}
	else
	{
		// Run a specific test

		// Search for the selected test
		test_t* selected = 0;
		for (int i = 0; i < s_unittestcount; i++)
		{
			if (strcmp(s_unittests[i].name, args[1]) == 0)
			{
				selected = &s_unittests[i];
				break;
			}
		}

		// Did we find our pick?
		if (selected == 0)
		{
			puts("Unknown test!");
			list_tests();
			return 1;
		}

		// Run it
		bool success = run_test(selected);
		return success ? 0 : 1;
	}
}

