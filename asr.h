#pragma once

#include <curl/curl.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <string>
#include <switch.h>
#include <switch_curl.h>
#include <stdlib.h>
#include <cctype>
#include <iomanip>

using namespace std;

typedef struct {
	bool success;
	int code;
	string text;
} Result;

class Asr
{
	public:
	Result* xfAsr(string appId, string key, const char *pcmBuf, unsigned int pcmBufLen);
};