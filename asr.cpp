
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

//#!/usr/bin/python
//# -*- coding: UTF-8 -*-
// import urllib2
// import time
// import urllib
// import json
// import hashlib
// import base64
//
//
// def main():
//    f = open("1.pcm", 'rb')
//    file_content = f.read()
//    base64_audio = base64.b64encode(file_content)
//    body = urllib.urlencode({'audio': base64_audio})
//
//    url = 'http://api.xfyun.cn/v1/service/v1/iat'
//    api_key = '8a413009f6cfa9346692736688361bfa'
//    param = {"engine_type": "sms8k", "aue": "raw"}
//
//    x_appid = '5adf1c1e'
//    x_param = base64.b64encode(json.dumps(param).replace(' ', ''))
//    x_time = int(int(round(time.time() * 1000)) / 1000)
//    x_checksum = hashlib.md5(api_key + str(x_time) + x_param).hexdigest()
//    x_header = {'X-Appid': x_appid,
//                'X-CurTime': x_time,
//                'X-Param': x_param,
//                'X-CheckSum': x_checksum}
//    req = urllib2.Request(url, body, x_header)
//    result = urllib2.urlopen(req)
//    result = result.read()
//    print result
//    return
//
// if __name__ == '__main__':
//    main()

Result *Asr::xfAsr(string appId, string key, const char *pcmBuf, unsigned int pcmBufLen)
{

	Result *result = new Result();
	result->code = 0;
	result->success = false;
	result->text = "";

	auto olen = pcmBufLen * 8 / 6;
	auto out = new unsigned char[olen];
	memset(out, 0, olen);
	string body = "audio=";
	auto status = switch_b64_encode((unsigned char *)pcmBuf, pcmBufLen, out, olen);
	if (SWITCH_STATUS_SUCCESS != status) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "pcm数据base64加密失败\n");
		return result;
	}

	char *tBuf = new char[olen * 3];
	memset(tBuf, 0, olen * 3);
	//body += switch_url_encode((const char *)out, tBuf, olen);
	//body += (char *)out;
	cout << out << endl;
	int i = 0,j=0;
	char ch = *((char *)out + i);
	while (ch) {
		if (ch == '+') {
			strcpy(tBuf + j, "%2B");
			j += 3;
		} else if (ch == '/') {
			strcpy(tBuf + j, "%2F");
			j += 3;
		}
		else
		{
			tBuf[j] = ch;
			j++;
		}
		ch = *((char *)out + ++i);
	}
	body += tBuf;
	cout << body << endl;

	memset(out, 0, olen);
	string param = "{\"engine_type\":\"sms8k\",\"aue\":\"raw\"}";
	status = switch_b64_encode((unsigned char *)param.c_str(), param.size(), out, olen);
	if (SWITCH_STATUS_SUCCESS != status) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "param base64加密失败\n");
		return result;
	}
	param = (char *)out;

	auto time = getTimeStamp();

	string checkSum = key + time + param;
	char md5Ret[33] = {0};
	status = switch_md5_string(md5Ret, checkSum.c_str(), checkSum.size());
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
	switch_curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());

	string data;
	switch_curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
	switch_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);

	// switch_curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); //启用时会汇报所有的信息

	auto res = switch_curl_easy_perform(curl);

	if (CURLE_OK != res) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "发送请求失败，错误码：%d\n", res);
		result->code = res;
		return result;
	}

	long res_code = 0;
	res = switch_curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);

	if ((res == CURLE_OK) && (res_code == 200 || res_code == 201)) {
		result->text = data;
		cout << data << endl;
		result->success = true;
	}

	curl_easy_cleanup(curl);

	switch_curl_slist_free_all(headers);

	delete tBuf;
	delete out;

	return result;
}