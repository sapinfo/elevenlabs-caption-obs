/*
 * ElevenLabs Captions for OBS
 * Real-time speech-to-text captions using ElevenLabs Scribe v2 API
 *
 * Based on SonioxCaptionPlugin architecture
 * Key differences from Soniox:
 *   - WebSocket URL: wss://api.elevenlabs.io/v1/speech-to-text/realtime
 *   - Auth: xi-api-key header (not in config JSON)
 *   - Audio: JSON + base64 (not raw binary PCM)
 *   - Config: query parameters (not initial JSON message)
 *   - Responses: message_type field (not tokens array)
 *   - Supports pcm_48000 natively (no downsampling needed)
 */

#include <obs-module.h>
#include <plugin-support.h>

#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>

using json = nlohmann::json;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

// --- Base64 encoder (for audio chunks) ---
static const char b64_table[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const uint8_t *data, size_t len)
{
	std::string result;
	result.reserve(((len + 2) / 3) * 4);

	for (size_t i = 0; i < len; i += 3) {
		uint32_t n = static_cast<uint32_t>(data[i]) << 16;
		if (i + 1 < len)
			n |= static_cast<uint32_t>(data[i + 1]) << 8;
		if (i + 2 < len)
			n |= static_cast<uint32_t>(data[i + 2]);

		result.push_back(b64_table[(n >> 18) & 0x3F]);
		result.push_back(b64_table[(n >> 12) & 0x3F]);
		result.push_back((i + 1 < len) ? b64_table[(n >> 6) & 0x3F] : '=');
		result.push_back((i + 2 < len) ? b64_table[n & 0x3F] : '=');
	}
	return result;
}

// --- Source data structure ---
struct elevenlabs_caption_data {
	obs_source_t *source;
	obs_source_t *text_source;

	// Hotkey
	obs_hotkey_id hotkey_id{OBS_INVALID_HOTKEY_ID};

	// WebSocket
	std::unique_ptr<ix::WebSocket> websocket;
	std::atomic<bool> connected{false};
	std::atomic<bool> captioning{false};
	std::atomic<bool> stopping{false};

	// Audio capture
	obs_source_t *audio_source{nullptr};
	std::string audio_source_name;

	// Caption state
	std::mutex text_mutex;
	std::string committed_buffer;
	std::string partial_text;
	int turn_count{0};

	// Settings
	int font_size{48};
	std::string font_face{"Apple SD Gothic Neo"};
	std::string api_key;
	std::string language{"ko"};

	// VAD settings
	float vad_threshold{0.4f};
	float vad_silence_secs{0.4f};
};

// --- Update text display ---
static void update_text_display(elevenlabs_caption_data *data, const char *text)
{
	if (!data->text_source)
		return;

	obs_data_t *font = obs_data_create();
	obs_data_set_string(font, "face", data->font_face.c_str());
	obs_data_set_int(font, "size", data->font_size);
	obs_data_set_string(font, "style", "Regular");
	obs_data_set_int(font, "flags", 0);

	obs_data_t *s = obs_data_create();
	obs_data_set_string(s, "text", text);
	obs_data_set_obj(s, "font", font);
	obs_source_update(data->text_source, s);

	obs_data_release(font);
	obs_data_release(s);
}

// --- Audio capture callback ---
// ElevenLabs accepts pcm_48000, so no downsampling needed.
// Audio is sent as base64-encoded JSON instead of raw binary.
static void audio_capture_callback(void *param, obs_source_t *, const struct audio_data *audio,
				   bool muted)
{
	auto *data = static_cast<elevenlabs_caption_data *>(param);

	if (!data->captioning || !data->connected || !data->websocket || muted)
		return;
	if (!audio->data[0] || audio->frames == 0)
		return;

	// OBS: float32, 48000Hz -> ElevenLabs: pcm_s16le, 48000Hz (no downsampling!)
	const float *src = reinterpret_cast<const float *>(audio->data[0]);
	uint32_t frames = audio->frames;

	std::vector<int16_t> pcm16(frames);
	for (uint32_t i = 0; i < frames; i++) {
		float sample = src[i];
		if (sample > 1.0f)
			sample = 1.0f;
		if (sample < -1.0f)
			sample = -1.0f;
		pcm16[i] = static_cast<int16_t>(sample * 32767.0f);
	}

	// Base64 encode the PCM data
	std::string b64 = base64_encode(reinterpret_cast<const uint8_t *>(pcm16.data()),
					pcm16.size() * sizeof(int16_t));

	// Send as JSON message
	json msg;
	msg["message_type"] = "input_audio_chunk";
	msg["audio_base_64"] = b64;
	msg["commit"] = false;
	msg["sample_rate"] = 48000;

	data->websocket->send(msg.dump());
}

// --- Handle ElevenLabs response messages ---
static void handle_elevenlabs_message(elevenlabs_caption_data *data, const std::string &msg_str)
{
	try {
		json resp = json::parse(msg_str);

		std::string msg_type = resp.value("message_type", "");

		// Session started
		if (msg_type == "session_started") {
			std::string session_id = resp.value("session_id", "");
			obs_log(LOG_INFO, "Session started: %s", session_id.c_str());
			update_text_display(data, "Listening...");
			return;
		}

		// Partial transcript (real-time interim)
		if (msg_type == "partial_transcript") {
			std::string text = resp.value("text", "");
			if (text.empty())
				return;

			std::lock_guard<std::mutex> lock(data->text_mutex);
			data->partial_text = text;

			std::string display = data->committed_buffer + text;
			while (!display.empty() && display.front() == ' ')
				display.erase(display.begin());

			update_text_display(data, display.c_str());
			return;
		}

		// Committed transcript (finalized)
		if (msg_type == "committed_transcript" ||
		    msg_type == "committed_transcript_with_timestamps") {
			std::string text = resp.value("text", "");
			if (text.empty())
				return;

			std::lock_guard<std::mutex> lock(data->text_mutex);

			data->turn_count++;
			obs_log(LOG_INFO, "[Turn %d] %s", data->turn_count, text.c_str());

			// Reset: committed text becomes the new display, clear partial
			data->committed_buffer = text;
			data->partial_text.clear();

			update_text_display(data, text.c_str());
			return;
		}

		// Error messages
		if (resp.contains("error")) {
			std::string err = resp.value("error", "Unknown error");
			obs_log(LOG_ERROR, "ElevenLabs error (%s): %s", msg_type.c_str(),
				err.c_str());
			update_text_display(data, ("Error: " + err).c_str());
			return;
		}

	} catch (const std::exception &e) {
		obs_log(LOG_WARNING, "JSON parse error: %s", e.what());
	}
}

// --- Stop captioning ---
static void stop_captioning(elevenlabs_caption_data *data)
{
	if (!data->captioning)
		return;

	data->stopping = true;
	data->captioning = false;
	data->connected = false;

	if (data->audio_source) {
		obs_source_remove_audio_capture_callback(data->audio_source, audio_capture_callback,
							 data);
		obs_source_release(data->audio_source);
		data->audio_source = nullptr;
	}

	if (data->websocket) {
		data->websocket->stop();
		data->websocket.reset();
	}

	data->stopping = false;
	update_text_display(data, "ElevenLabs Captions Ready!");
	obs_log(LOG_INFO, "Captioning stopped");
}

// --- Build WebSocket URL with query parameters ---
static std::string build_ws_url(elevenlabs_caption_data *data)
{
	std::string url = "wss://api.elevenlabs.io/v1/speech-to-text/realtime";
	url += "?model_id=scribe_v2_realtime";
	url += "&audio_format=pcm_48000";
	url += "&commit_strategy=vad";
	url += "&language_code=" + data->language;

	// VAD parameters
	char buf[64];
	snprintf(buf, sizeof(buf), "%.1f", data->vad_silence_secs);
	url += "&vad_silence_threshold_secs=";
	url += buf;

	snprintf(buf, sizeof(buf), "%.1f", data->vad_threshold);
	url += "&vad_threshold=";
	url += buf;

	return url;
}

// --- Start captioning ---
static void start_captioning(elevenlabs_caption_data *data)
{
	if (data->captioning)
		stop_captioning(data);

	if (data->api_key.empty()) {
		update_text_display(data, "[Enter API Key first!]");
		return;
	}

	obs_source_t *audio_src = obs_get_source_by_name(data->audio_source_name.c_str());
	if (!audio_src) {
		update_text_display(data, "[Select Audio Source!]");
		return;
	}

	update_text_display(data, "Connecting...");

	{
		std::lock_guard<std::mutex> lock(data->text_mutex);
		data->committed_buffer.clear();
		data->partial_text.clear();
		data->turn_count = 0;
	}

	// 1. Setup WebSocket
	data->websocket = std::make_unique<ix::WebSocket>();
	data->websocket->setUrl(build_ws_url(data));

	// Set auth header
	ix::WebSocketHttpHeaders headers;
	headers["xi-api-key"] = data->api_key;
	data->websocket->setExtraHeaders(headers);

	// Auto-reconnect on disconnect
	data->websocket->enableAutomaticReconnection();
	data->websocket->setMinWaitBetweenReconnectionRetries(3000);
	data->websocket->setMaxWaitBetweenReconnectionRetries(30000);

	data->websocket->setOnMessageCallback([data](const ix::WebSocketMessagePtr &msg) {
		switch (msg->type) {
		case ix::WebSocketMessageType::Open:
			obs_log(LOG_INFO, "ElevenLabs WebSocket connected");
			data->connected = true;
			// No config message needed - all config is in URL query params
			break;

		case ix::WebSocketMessageType::Message:
			handle_elevenlabs_message(data, msg->str);
			break;

		case ix::WebSocketMessageType::Error:
			obs_log(LOG_ERROR, "WS error: %s (status=%d)", msg->errorInfo.reason.c_str(),
				msg->errorInfo.http_status);
			data->connected = false;
			update_text_display(data, ("Error: " + msg->errorInfo.reason).c_str());
			break;

		case ix::WebSocketMessageType::Close:
			obs_log(LOG_INFO, "WS closed (code=%d, reason=%s)", msg->closeInfo.code,
				msg->closeInfo.reason.c_str());
			data->connected = false;
			if (!data->stopping && data->captioning) {
				std::string reason = msg->closeInfo.reason.empty()
							    ? "Connection closed"
							    : msg->closeInfo.reason;
				update_text_display(data, reason.c_str());
			}
			break;

		default:
			break;
		}
	});

	// 2. Register audio capture (connected=false, so no sending yet)
	data->audio_source = audio_src;
	obs_source_add_audio_capture_callback(audio_src, audio_capture_callback, data);

	// 3. Activate captioning and start WebSocket
	data->captioning = true;
	data->websocket->start();
	obs_log(LOG_INFO, "Caption started, waiting for connection...");
}

// --- Test connection (without audio) ---
static void test_connection(elevenlabs_caption_data *data)
{
	if (data->websocket) {
		data->websocket->stop();
		data->websocket.reset();
		data->connected = false;
	}

	if (data->api_key.empty()) {
		update_text_display(data, "[Enter API Key first!]");
		return;
	}

	update_text_display(data, "Testing connection...");

	data->websocket = std::make_unique<ix::WebSocket>();
	data->websocket->setUrl(build_ws_url(data));

	ix::WebSocketHttpHeaders headers;
	headers["xi-api-key"] = data->api_key;
	data->websocket->setExtraHeaders(headers);

	data->websocket->disableAutomaticReconnection();

	data->websocket->setOnMessageCallback([data](const ix::WebSocketMessagePtr &msg) {
		switch (msg->type) {
		case ix::WebSocketMessageType::Open:
			data->connected = true;
			update_text_display(data, "Connected OK!");
			obs_log(LOG_INFO, "Test connection: OK");
			break;
		case ix::WebSocketMessageType::Message: {
			try {
				json resp = json::parse(msg->str);
				std::string msg_type = resp.value("message_type", "");
				if (msg_type == "session_started") {
					update_text_display(data, "Connected! Session ready.");
				} else if (resp.contains("error")) {
					update_text_display(
						data,
						("Error: " + resp["error"].get<std::string>()).c_str());
				}
			} catch (...) {
			}
			break;
		}
		case ix::WebSocketMessageType::Error:
			update_text_display(data, ("Error: " + msg->errorInfo.reason).c_str());
			break;
		case ix::WebSocketMessageType::Close:
			if (!data->stopping)
				update_text_display(data, "Test: Disconnected");
			data->connected = false;
			break;
		default:
			break;
		}
	});

	data->websocket->start();
}

// --- Hotkey: Toggle Start/Stop ---
static void hotkey_toggle_caption(void *private_data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *data = static_cast<elevenlabs_caption_data *>(private_data);

	obs_data_t *settings = obs_source_get_settings(data->source);
	data->api_key = obs_data_get_string(settings, "api_key");
	data->language = obs_data_get_string(settings, "language");
	data->audio_source_name = obs_data_get_string(settings, "audio_source");
	data->vad_threshold = (float)obs_data_get_double(settings, "vad_threshold");
	data->vad_silence_secs = (float)obs_data_get_double(settings, "vad_silence_secs");
	obs_data_release(settings);

	if (data->captioning)
		stop_captioning(data);
	else
		start_captioning(data);
}

// --- OBS Callbacks ---

static const char *elevenlabs_caption_get_name(void *)
{
	return "ElevenLabs Captions";
}

static void *elevenlabs_caption_create(obs_data_t *settings, obs_source_t *source)
{
	auto *data = new elevenlabs_caption_data();
	data->source = source;
	data->font_size = (int)obs_data_get_int(settings, "font_size");

	obs_data_t *ts = obs_data_create();
	obs_data_set_string(ts, "text", "ElevenLabs Captions Ready!");
	obs_data_set_int(ts, "font_size", data->font_size);
#ifdef _WIN32
	data->text_source = obs_source_create_private("text_gdiplus", "elevenlabs_text", ts);
#else
	data->text_source = obs_source_create_private("text_ft2_source_v2", "elevenlabs_text", ts);
#endif
	obs_data_release(ts);

	data->hotkey_id = obs_hotkey_register_source(source, "elevenlabs_toggle_caption",
						     "Toggle ElevenLabs Captions",
						     hotkey_toggle_caption, data);

	obs_log(LOG_INFO, "caption source created");
	return data;
}

static void elevenlabs_caption_destroy(void *private_data)
{
	auto *data = static_cast<elevenlabs_caption_data *>(private_data);
	stop_captioning(data);
	if (data->hotkey_id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(data->hotkey_id);
	if (data->text_source)
		obs_source_release(data->text_source);
	delete data;
}

static void elevenlabs_caption_update(void *private_data, obs_data_t *settings)
{
	auto *data = static_cast<elevenlabs_caption_data *>(private_data);
	data->font_size = (int)obs_data_get_int(settings, "font_size");
	data->font_face = obs_data_get_string(settings, "font_face");
	data->api_key = obs_data_get_string(settings, "api_key");
	data->language = obs_data_get_string(settings, "language");
	data->audio_source_name = obs_data_get_string(settings, "audio_source");
	data->vad_threshold = (float)obs_data_get_double(settings, "vad_threshold");
	data->vad_silence_secs = (float)obs_data_get_double(settings, "vad_silence_secs");

	if (!data->captioning && !data->connected) {
		if (!data->api_key.empty())
			update_text_display(data, "ElevenLabs Captions Ready!");
		else
			update_text_display(data, "[Set API Key in Properties]");
	}
}

// --- Button callbacks ---
static bool on_test_clicked(obs_properties_t *, obs_property_t *, void *private_data)
{
	auto *data = static_cast<elevenlabs_caption_data *>(private_data);
	obs_data_t *settings = obs_source_get_settings(data->source);
	data->api_key = obs_data_get_string(settings, "api_key");
	data->language = obs_data_get_string(settings, "language");
	data->vad_threshold = (float)obs_data_get_double(settings, "vad_threshold");
	data->vad_silence_secs = (float)obs_data_get_double(settings, "vad_silence_secs");
	obs_data_release(settings);

	if (data->connected) {
		data->stopping = true;
		data->websocket->stop();
		data->websocket.reset();
		data->connected = false;
		data->stopping = false;
		update_text_display(data, "ElevenLabs Captions Ready!");
	} else {
		test_connection(data);
	}
	return false;
}

static bool on_start_stop_clicked(obs_properties_t *, obs_property_t *, void *private_data)
{
	auto *data = static_cast<elevenlabs_caption_data *>(private_data);

	obs_data_t *settings = obs_source_get_settings(data->source);
	data->api_key = obs_data_get_string(settings, "api_key");
	data->language = obs_data_get_string(settings, "language");
	data->audio_source_name = obs_data_get_string(settings, "audio_source");
	data->vad_threshold = (float)obs_data_get_double(settings, "vad_threshold");
	data->vad_silence_secs = (float)obs_data_get_double(settings, "vad_silence_secs");
	obs_data_release(settings);

	if (data->captioning)
		stop_captioning(data);
	else
		start_captioning(data);
	return true;
}

// --- Enumerate audio sources ---
static bool enum_audio_sources(void *param, obs_source_t *source)
{
	auto *list = static_cast<obs_property_t *>(param);
	uint32_t flags = obs_source_get_output_flags(source);
	if (flags & OBS_SOURCE_AUDIO) {
		const char *name = obs_source_get_name(source);
		if (name && strlen(name) > 0)
			obs_property_list_add_string(list, name, name);
	}
	return true;
}

// --- Properties UI ---
static obs_properties_t *elevenlabs_caption_get_properties(void *private_data)
{
	auto *data = static_cast<elevenlabs_caption_data *>(private_data);
	obs_properties_t *props = obs_properties_create();

	// API Key
	obs_properties_add_text(props, "api_key", "ElevenLabs API Key", OBS_TEXT_PASSWORD);

	// Audio source selection
	obs_property_t *audio_list =
		obs_properties_add_list(props, "audio_source", "Audio Source", OBS_COMBO_TYPE_LIST,
					OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(audio_list, "(Select audio source)", "");
	obs_enum_sources(enum_audio_sources, audio_list);

	// Language selection (57 languages supported, showing common ones)
	obs_property_t *lang =
		obs_properties_add_list(props, "language", "Language", OBS_COMBO_TYPE_LIST,
					OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(lang, "Korean", "ko");
	obs_property_list_add_string(lang, "English", "en");
	obs_property_list_add_string(lang, "Japanese", "ja");
	obs_property_list_add_string(lang, "Chinese", "zh");
	obs_property_list_add_string(lang, "Spanish", "es");
	obs_property_list_add_string(lang, "French", "fr");
	obs_property_list_add_string(lang, "German", "de");
	obs_property_list_add_string(lang, "Portuguese", "pt");
	obs_property_list_add_string(lang, "Russian", "ru");

	// VAD settings
	obs_properties_add_float_slider(props, "vad_threshold", "VAD Threshold", 0.1, 0.9, 0.1);
	obs_properties_add_float_slider(props, "vad_silence_secs", "Silence Duration (sec)", 0.3,
					3.0, 0.1);

	// Font selection
	obs_property_t *font_list =
		obs_properties_add_list(props, "font_face", "Font", OBS_COMBO_TYPE_LIST,
					OBS_COMBO_FORMAT_STRING);
#ifdef _WIN32
	obs_property_list_add_string(font_list, "Malgun Gothic", "Malgun Gothic");
	obs_property_list_add_string(font_list, "Yu Gothic", "Yu Gothic");
#else
	obs_property_list_add_string(font_list, "Apple SD Gothic Neo", "Apple SD Gothic Neo");
	obs_property_list_add_string(font_list, "Hiragino Sans", "Hiragino Sans");
#endif
	obs_property_list_add_string(font_list, "Noto Sans CJK KR", "Noto Sans CJK KR");
	obs_property_list_add_string(font_list, "Noto Sans CJK JP", "Noto Sans CJK JP");
	obs_property_list_add_string(font_list, "Arial", "Arial");
	obs_property_list_add_string(font_list, "Helvetica", "Helvetica");

	// Font size
	obs_properties_add_int_slider(props, "font_size", "Font Size", 12, 120, 2);

	// Buttons
	obs_properties_add_button(props, "test_connection", "Test Connection", on_test_clicked);

	const char *btn_text = (data && data->captioning) ? "Stop Caption" : "Start Caption";
	obs_properties_add_button(props, "start_stop", btn_text, on_start_stop_clicked);

	return props;
}

static void elevenlabs_caption_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "api_key", "");
	obs_data_set_default_string(settings, "language", "ko");
	obs_data_set_default_string(settings, "audio_source", "");
	obs_data_set_default_double(settings, "vad_threshold", 0.4);
	obs_data_set_default_double(settings, "vad_silence_secs", 0.4);
#ifdef _WIN32
	obs_data_set_default_string(settings, "font_face", "Malgun Gothic");
#else
	obs_data_set_default_string(settings, "font_face", "Apple SD Gothic Neo");
#endif
	obs_data_set_default_int(settings, "font_size", 48);
}

static uint32_t elevenlabs_caption_get_width(void *private_data)
{
	auto *data = static_cast<elevenlabs_caption_data *>(private_data);
	return data->text_source ? obs_source_get_width(data->text_source) : 0;
}

static uint32_t elevenlabs_caption_get_height(void *private_data)
{
	auto *data = static_cast<elevenlabs_caption_data *>(private_data);
	return data->text_source ? obs_source_get_height(data->text_source) : 0;
}

static void elevenlabs_caption_video_render(void *private_data, gs_effect_t *)
{
	auto *data = static_cast<elevenlabs_caption_data *>(private_data);
	if (data->text_source)
		obs_source_video_render(data->text_source);
}

// --- Source registration ---
static obs_source_info elevenlabs_caption_source_info = {};

bool obs_module_load(void)
{
	elevenlabs_caption_source_info.id = "elevenlabs_caption";
	elevenlabs_caption_source_info.type = OBS_SOURCE_TYPE_INPUT;
	elevenlabs_caption_source_info.output_flags = OBS_SOURCE_VIDEO;
	elevenlabs_caption_source_info.get_name = elevenlabs_caption_get_name;
	elevenlabs_caption_source_info.create = elevenlabs_caption_create;
	elevenlabs_caption_source_info.destroy = elevenlabs_caption_destroy;
	elevenlabs_caption_source_info.update = elevenlabs_caption_update;
	elevenlabs_caption_source_info.get_properties = elevenlabs_caption_get_properties;
	elevenlabs_caption_source_info.get_defaults = elevenlabs_caption_get_defaults;
	elevenlabs_caption_source_info.get_width = elevenlabs_caption_get_width;
	elevenlabs_caption_source_info.get_height = elevenlabs_caption_get_height;
	elevenlabs_caption_source_info.video_render = elevenlabs_caption_video_render;

	obs_register_source(&elevenlabs_caption_source_info);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
