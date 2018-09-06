
#include "asr.h"
#include <iostream>

using namespace std;

string getTimeStamp()
{
	auto tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
	auto timestamp = tmp.count();

	ostringstream os;
	os << long(timestamp / 1000);
	istringstream is(os.str());
	string ret;
	is >> ret;

	return ret;
}

size_t writeMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	auto mem = (string *)data;
	*mem += (char *)ptr;
	return realsize;
}



wchar_t *ANSIToUnicode(const char *str)
{
	int textlen;
	wchar_t *result;
	textlen = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
	result = (wchar_t *)malloc((textlen + 1) * sizeof(wchar_t));
	memset(result, 0, (textlen + 1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, str, -1, (LPWSTR)result, textlen);
	return result;
}

char *UnicodeToANSI(const wchar_t *str)
{
	char *result;
	int textlen;
	textlen = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
	result = (char *)malloc((textlen + 1) * sizeof(char));
	memset(result, 0, sizeof(char) * (textlen + 1));
	WideCharToMultiByte(CP_ACP, 0, str, -1, result, textlen, NULL, NULL);
	return result;
}

wchar_t *UTF8ToUnicode(const char *str)
{
	int textlen;
	wchar_t *result;
	textlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	result = (wchar_t *)malloc((textlen + 1) * sizeof(wchar_t));
	memset(result, 0, (textlen + 1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, str, -1, (LPWSTR)result, textlen);
	return result;
}

char *UnicodeToUTF8(const wchar_t *str)
{
	char *result;
	int textlen;
	textlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL);
	result = (char *)malloc((textlen + 1) * sizeof(char));
	memset(result, 0, sizeof(char) * (textlen + 1));
	WideCharToMultiByte(CP_UTF8, 0, str, -1, result, textlen, NULL, NULL);
	return result;
}
/*宽字符转换为多字符Unicode - ANSI*/
char *w2m(const wchar_t *wcs)
{
	int len;
	char *buf;
	len = wcstombs(NULL, wcs, 0);
	if (len == 0) return NULL;
	buf = (char *)malloc(sizeof(char) * (len + 1));
	memset(buf, 0, sizeof(char) * (len + 1));
	len = wcstombs(buf, wcs, len + 1);
	return buf;
}
/*多字符转换为宽字符ANSI - Unicode*/
wchar_t *m2w(const char *mbs)
{
	int len;
	wchar_t *buf;
	len = mbstowcs(NULL, mbs, 0);
	if (len == 0) return NULL;
	buf = (wchar_t *)malloc(sizeof(wchar_t) * (len + 1));
	memset(buf, 0, sizeof(wchar_t) * (len + 1));
	len = mbstowcs(buf, mbs, len + 1);
	return buf;
}

char *ANSIToUTF8(const char *str) { return UnicodeToUTF8(ANSIToUnicode(str)); }

char *UTF8ToANSI(const char *str) { return UnicodeToANSI(UTF8ToUnicode(str)); }


/*编码代码
 * const unsigned char * sourcedata， 源数组
 * char * base64 ，码字保存
 */
int base64_encode(const unsigned char *sourcedata, int sourcedata_len, char *base64)
{
	const char *base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const char padding_char = '=';
	int i = 0, j = 0;
	unsigned char trans_index = 0;		   // 索引是8位，但是高两位都为0
	const int datalength = sourcedata_len; // strlen((const char*)sourcedata);
	for (; i < datalength; i += 3) {
		// 每三个一组，进行编码
		// 要编码的数字的第一个
		trans_index = ((sourcedata[i] >> 2) & 0x3f);
		base64[j++] = base64char[(int)trans_index];
		// 第二个
		trans_index = ((sourcedata[i] << 4) & 0x30);
		if (i + 1 < datalength) {
			trans_index |= ((sourcedata[i + 1] >> 4) & 0x0f);
			base64[j++] = base64char[(int)trans_index];
		} else {
			base64[j++] = base64char[(int)trans_index];

			base64[j++] = padding_char;

			base64[j++] = padding_char;

			break; // 超出总长度，可以直接break
		}
		// 第三个
		trans_index = ((sourcedata[i + 1] << 2) & 0x3c);
		if (i + 2 < datalength) { // 有的话需要编码2个
			trans_index |= ((sourcedata[i + 2] >> 6) & 0x03);
			base64[j++] = base64char[(int)trans_index];

			trans_index = sourcedata[i + 2] & 0x3f;
			base64[j++] = base64char[(int)trans_index];
		} else {
			base64[j++] = base64char[(int)trans_index];

			base64[j++] = padding_char;

			break;
		}
	}

	base64[j] = '\0';

	return 0;
}


int URLEncode(const char *str, const int strSize, char *result, const int resultSize)
{
	int i;
	int j = 0; // for result index
	char ch;

	if ((str == NULL) || (result == NULL) || (strSize <= 0) || (resultSize <= 0)) { return 0; }

	for (i = 0; (i < strSize) && (j < resultSize); ++i) {
		ch = str[i];
		if (((ch >= 'A') && (ch < 'Z')) || ((ch >= 'a') && (ch < 'z')) || ((ch >= '0') && (ch < '9'))) {
			result[j++] = ch;
		} else if (ch == ' ') {
			result[j++] = '+';
		} else if (ch == '.' || ch == '-' || ch == '_' || ch == '*') {
			result[j++] = ch;
		} else {
			if (j + 3 < resultSize) {
				sprintf(result + j, "%%%02X", (unsigned char)ch);
				j += 3;
			} else {
				return 0;
			}
		}
	}

	result[j] = '\0';
	return j;
}

Result *Asr::xfAsr(string appId, string key, const char *pcmBuf, unsigned int pcmBufLen)
{

	Result *result = new Result();
	result->code = 0;
	result->success = false;
	result->text = "";
	auto olen = pcmBufLen * 2;
	auto out = new char[olen*8/6];
	memset(out, 0, olen);
	string body = "audio=";
	
	base64_encode((unsigned char*)pcmBuf, pcmBufLen, out);

	// 转utf-8编码
	auto data_base64_utf8_str = ANSIToUTF8((const char*)(out));
	auto file_temp_buffer_size = strlen(data_base64_utf8_str);
	memset(out, 0, olen);
	memcpy(out, data_base64_utf8_str, file_temp_buffer_size);
	free(data_base64_utf8_str);

	

	char *tBuf = new char[olen * 3];
	memset(tBuf, 0, olen * 3);

	URLEncode(out, strlen(out), tBuf, olen * 3);

	body += tBuf;

	memset(out, 0, olen);
	string param = "{\"engine_type\":\"sms8k\",\"aue\":\"raw\"}";
	base64_encode((unsigned char *)param.c_str(), param.size(), out);
	param = (char *)out;

	auto time = getTimeStamp();

	string checkSum = key + time + param;
	char md5Ret[33] = {0};
	auto status = switch_md5_string(md5Ret, checkSum.c_str(), checkSum.size());
	if (SWITCH_STATUS_SUCCESS != status) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "checkSum MD5加密失败\n");
		return result;
	}
	checkSum = md5Ret;

	auto curl = switch_curl_easy_init();
	switch_curl_slist_t *headers = nullptr;

	headers = switch_curl_slist_append(headers, "Content-Type:application/x-www-form-urlencoded; charset=utf-8");

	appId = "X-Appid: " + appId;
	headers = switch_curl_slist_append(headers, appId.c_str());

	time = "X-CurTime: " + time;
	headers = switch_curl_slist_append(headers, time.c_str());

	param = "X-Param: " + param;
	headers = switch_curl_slist_append(headers, param.c_str());

	checkSum = "X-CheckSum: " + checkSum;
	headers = switch_curl_slist_append(headers, checkSum.c_str());

	switch_curl_easy_setopt(curl, CURLOPT_URL, "http://api.xfyun.cn/v1/service/v1/iat");
	switch_curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	switch_curl_easy_setopt(curl, CURLOPT_POST, 1L);

	switch_curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

	string ret;
	switch_curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ret);
	switch_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);


	auto res = switch_curl_easy_perform(curl);

	if (CURLE_OK != res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "发送请求失败，错误码：%d\n", res);
		result->code = res;
		return result;
	}

	long res_code = 0;
	res = switch_curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);

	if ((res == CURLE_OK) && (res_code == 200 || res_code == 201)) {

		cJSON *cjsRoot = cJSON_Parse(ret.c_str());
		cJSON *code = cJSON_GetObjectItem(cjsRoot, "code");
		cJSON *desc = cJSON_GetObjectItem(cjsRoot, "desc");
		cJSON *data = cJSON_GetObjectItem(cjsRoot, "data");
		result->text = data->valuestring;
		result->success = true;
	}

	curl_easy_cleanup(curl);

	switch_curl_slist_free_all(headers);

	delete tBuf;
	delete out;

	return result;
}