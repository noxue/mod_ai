#define MOD_EXPORTS

#include "asr.h"
#include "webrtc_vad.h"
#include <iostream>
#include <stdio.h>
#include <switch.h>

#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "FreeSwitchCore.lib")
#pragma comment(lib, "vad.lib")
#endif

using namespace std;

typedef struct {
	switch_media_bug_t *bug;
	switch_core_session_t *session;
	char *pcmBuf;			  // 保存要发送识别的数据内容
	int pcmBufLen;			  //数据长度
	bool isSpeakStart;		  // 是否已经开始说话
	short muteFrames;		  // 静音的数据包数目
	string destination_number; // 被呼叫号码
	string uuid; // 通话的uuid
} switch_no_t;

#define PCM_MAXBUF (sizeof(char) * 160 * 50 * 100)
#define MUTE_FRAMES 35 // 静音多少个包断开说话

VadInst *inst = WebRtcVad_Create();

extern "C" {
SWITCH_MODULE_LOAD_FUNCTION(mod_ai_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ai_shutdown);
SWITCH_MODULE_DEFINITION(mod_ai, mod_ai_load, mod_ai_shutdown, NULL);

SWITCH_STANDARD_APP(ai_app);
SWITCH_STANDARD_API(asr_start);
SWITCH_STANDARD_API(asr_stop);
SWITCH_STANDARD_API(uuid_play);
SWITCH_STANDARD_API(uuid_pause);
SWITCH_STANDARD_API(uuid_stop);
SWITCH_STANDARD_API(uuid_asr);
static switch_bool_t read_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_ai_load)
{
	// 初始化静音检测模块
	WebRtcVad_Init(inst);
	WebRtcVad_set_mode(inst, 3);

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_application_interface_t *app_interface;
	SWITCH_ADD_APP(app_interface, "ai", "ai机器人", "希高智能电话机器人", ai_app, "", SAF_NONE);

	switch_api_interface_t *api_interface;
	SWITCH_ADD_API(api_interface, "asr_start", "开始语音识别", asr_start, "");
	SWITCH_ADD_API(api_interface, "asr_stop", "停止语音识别", asr_start, "");
	SWITCH_ADD_API(api_interface, "uuid_play", "发送语音", uuid_play, "<uuid> <path>");
	SWITCH_ADD_API(api_interface, "uuid_pause", "暂停发送语音", uuid_pause, "<uuid> <on|off>");
	SWITCH_ADD_API(api_interface, "uuid_stop", "停止当前在发的语音", uuid_stop, "<uuid>");

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(ai_app)
{
	switch_ivr_park_session(session);

	switch_status_t status;

	switch_no_t *pvt;
	

	if (!(pvt = (switch_no_t *)switch_core_session_alloc(session, sizeof(switch_no_t)))) { return; }


	pvt->session = session;
	pvt->pcmBuf = (char *)switch_core_session_alloc(session, PCM_MAXBUF);
	pvt->pcmBufLen = 0;
	pvt->muteFrames = 0;

	auto channel = switch_core_session_get_channel(session);
	pvt->destination_number = switch_channel_get_variable(channel, "channel_name");
	pvt->destination_number = pvt->destination_number.substr(pvt->destination_number.find_last_of('/')+1);

	pvt->uuid = switch_channel_get_variable(channel, "uuid");

	if ((status = switch_core_media_bug_add(session, "asr_read", NULL, read_callback, pvt, 0,
											SMBF_READ_REPLACE | SMBF_NO_PAUSE | SMBF_ONE_ONLY, &(pvt->bug))) !=
		SWITCH_STATUS_SUCCESS) {
		return;
	}
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_ai_shutdown)
{
	WebRtcVad_Free(inst);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(asr_start) { return SWITCH_STATUS_SUCCESS; }

SWITCH_STANDARD_API(asr_stop) { return SWITCH_STATUS_SUCCESS; }

SWITCH_STANDARD_API(uuid_play)
{
	char *argv[2] = {0};
	int argc = 0;
	char myCmd[512] = {0};

	if (!zstr(cmd)) {
		strncpy(myCmd, cmd, 512);
		argc = switch_separate_string(myCmd, ' ', argv, sizeof(argv) / sizeof(argv[0]));
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "没有指定参数\n");
		return SWITCH_STATUS_FALSE;
	}

	if (argc != 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "参数个数不正确，需要两个参数\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_core_session_t *uuidSession = switch_core_session_locate(argv[0]);
	if (uuidSession == nullptr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "没有找到对应的session\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "uuid:%s\tpath:%s\n", argv[0], argv[1]);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "已找到session\n");

	switch_channel_t *channel = switch_core_session_get_channel(uuidSession);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_file_handle_t fh = {0};

	if (channel == nullptr) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "没有找到对应的channel\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "已找到channel\n");
	switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");

	switch_core_session_rwunlock(uuidSession);

	status = switch_ivr_play_file(uuidSession, &fh, argv[1], NULL);
	switch_assert(!(fh.flags & SWITCH_FILE_OPEN));

	switch (status) {
	case SWITCH_STATUS_SUCCESS:
	case SWITCH_STATUS_BREAK:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE PLAYED");
		break;
	case SWITCH_STATUS_NOTFOUND:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "FILE NOT FOUND");
		break;
	default:
		switch_channel_set_variable(channel, SWITCH_CURRENT_APPLICATION_RESPONSE_VARIABLE, "PLAYBACK ERROR");
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

#define PAUSE_SYNTAX "<uuid> <on|off>"
SWITCH_STANDARD_API(uuid_pause)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[4] = {0};
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 2 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", PAUSE_SYNTAX);
	} else {
		char *uuid = argv[0];
		char *dest = argv[1];

		if ((psession = switch_core_session_locate(uuid))) {
			switch_channel_t *channel = switch_core_session_get_channel(psession);

			switch_file_handle_t *fh = (switch_file_handle_t *)switch_channel_get_private(channel, "__fh");
			if (fh == nullptr) { return SWITCH_STATUS_FALSE; }
			if (!strcasecmp(dest, "on")) {
				switch_set_flag(fh, SWITCH_FILE_PAUSE);
			} else {
				switch_clear_flag(fh, SWITCH_FILE_PAUSE);
			}

			switch_core_session_rwunlock(psession);

		} else {
			stream->write_function(stream, "-ERR No such channel!\n");
		}
	}

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

#define STOP_SYNTAX "<uuid>"
SWITCH_STANDARD_API(uuid_stop)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[1] = {0};
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (zstr(cmd) || argc < 1 || zstr(argv[0])) {
		stream->write_function(stream, "-USAGE: %s\n", STOP_SYNTAX);
	} else {
		char *uuid = argv[0];

		if ((psession = switch_core_session_locate(uuid))) {
			switch_channel_t *channel = switch_core_session_get_channel(psession);
			switch_file_handle_t *fh = (switch_file_handle_t *)switch_channel_get_private(channel, "__fh");
			if (fh == nullptr) { return SWITCH_STATUS_FALSE; }

			switch_channel_set_flag(channel, CF_BREAK);
			switch_core_session_rwunlock(psession);

		} else {
			stream->write_function(stream, "-ERR No such channel!\n");
		}
	}
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(uuid_asr) { return SWITCH_STATUS_SUCCESS; }

static switch_bool_t read_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	static int word = 0;
	switch_no_t *pvt = (switch_no_t *)user_data;
	if (nullptr == pvt) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "user_data为空\n");
		return SWITCH_FALSE;
	}

	if (SWITCH_ABC_TYPE_INIT == type) {
		if (bug == nullptr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "bug不能为空\n");
			return SWITCH_FALSE;
		}
		switch_core_session_t *sess = switch_core_media_bug_get_session(bug);
		if (nullptr == sess) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "获取session失败\n");
			return SWITCH_FALSE;
		}

		char filename[512] = {0};
		sprintf(filename, "d:/%s.pcm", switch_core_session_get_uuid(sess));
		FILE *fp = fopen(filename, "wb+");
		if (!fp) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "创建文件失败：%s\n", filename);
			return SWITCH_FALSE;
		}
		switch_channel_t *channel = switch_core_session_get_channel(sess);
		if (channel == nullptr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "获取channel失败\n");
			return SWITCH_FALSE;
		}
		switch_channel_set_private(channel, "record_fp", fp);

	} else if (SWITCH_ABC_TYPE_READ_REPLACE == type) {
		if (bug == nullptr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "bug不能为空\n");
			return SWITCH_FALSE;
		}
		switch_core_session_t *sess = switch_core_media_bug_get_session(bug);
		if (nullptr == sess) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "获取session失败\n");
			return SWITCH_FALSE;
		}
		switch_channel_t *channel = switch_core_session_get_channel(sess);
		if (channel == nullptr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "获取channel失败\n");
			return SWITCH_FALSE;
		}
		FILE *fp = (FILE *)switch_channel_get_private(channel, "record_fp");
		if (fp == nullptr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "userdata参数为空\n");
			return SWITCH_FALSE;
		}

		switch_frame_t *frame;
		if ((frame = switch_core_media_bug_get_read_replace_frame(bug))) {

			char *data = (char *)frame->data;
			int dataLen = frame->datalen;
			switch_core_media_bug_set_read_replace_frame(bug, frame);

			int status = WebRtcVad_Process(inst, frame->rate, (short *)(frame->data), dataLen / 2);
			if (status == -1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
								  "vad error: channel:%d\trate:%d\tsamples:%d\tpcmLen:%d\n", frame->channels,
								  frame->rate, frame->samples, dataLen);
			} else if (1 == status) {

				memcpy(pvt->pcmBuf + pvt->pcmBufLen, data, dataLen);
				pvt->pcmBufLen += dataLen;

				if (!pvt->isSpeakStart) {
					switch_event_t *event = NULL;
					if (switch_event_create(&event, SWITCH_EVENT_CUSTOM) == SWITCH_STATUS_SUCCESS) {

						event->subclass_name = strdup("asr::start_speak");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Name", event->subclass_name);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Unique-ID",
													   pvt->uuid.c_str());
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Destination-Number",
													   pvt->destination_number.c_str());
						switch_event_fire(&event);
					}
				}

				pvt->isSpeakStart = true;
				pvt->muteFrames = 0;

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "|%d||||||||||||||||||||||||||||%d\n", word,
								  pvt->pcmBufLen);
			} else {
				pvt->muteFrames++;
				if (pvt->muteFrames > MUTE_FRAMES) { pvt->isSpeakStart = false; }

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "|%d\t%d\tmute:%d\n", word, pvt->pcmBufLen,
								  pvt->muteFrames);
			}

			// 静音50个包，并且有0.2秒以上的语音才写入
			if (pvt->isSpeakStart && pvt->muteFrames >= MUTE_FRAMES && pvt->pcmBufLen >= dataLen * 10) {
				/*Asr asr;
				auto ret = asr.xfAsr("5adf1c1e", "8a413009f6cfa9346692736688361bfa", pvt->pcmBuf, pvt->pcmBufLen);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "asr\tword:%d\tcode:%d\tmsg:%s\n", word,
								  ret->code, ret->text);
				delete ret;*/

				fwrite(pvt->pcmBuf, pvt->pcmBufLen, 1, fp);
				pvt->pcmBufLen = 0;
				word++;

				{

					switch_event_t *event = NULL;
					if (switch_event_create(&event, SWITCH_EVENT_CUSTOM) == SWITCH_STATUS_SUCCESS) {
						event->subclass_name = strdup("asr::end_speak");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Event-Name", event->subclass_name);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Unique-ID",
													   pvt->uuid.c_str());
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Caller-Destination-Number",
													   pvt->destination_number.c_str());
						switch_event_fire(&event);
					}
				}
			}

			/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,
							 "record\t=>type:%d\tlen:%d\tchannels:%d\trate:%d\tsamples:%d\n", type, dataLen,
							 frame->channels, frame->rate, frame->samples);*/

			return SWITCH_TRUE;
		}
	} else if (SWITCH_ABC_TYPE_CLOSE == type) {
		if (bug == nullptr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "bug不能为空\n");
			return SWITCH_FALSE;
		}
		switch_core_session_t *sess = switch_core_media_bug_get_session(bug);
		if (nullptr == sess) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "获取session失败\n");
			return SWITCH_FALSE;
		}
		switch_channel_t *channel = switch_core_session_get_channel(sess);
		if (channel == nullptr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "获取channel失败\n");
			return SWITCH_FALSE;
		}
		FILE *fp = (FILE *)switch_channel_get_private(channel, "record_fp");
		if (fp == nullptr) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "userdata参数为空\n");
			return SWITCH_FALSE;
		}
		fclose(fp);
	}

	return SWITCH_TRUE;
}