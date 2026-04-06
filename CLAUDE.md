# ElevenLabs Captions for OBS

OBS Studio 플러그인 - ElevenLabs Scribe v2를 이용한 실시간 음성 자막

## Project

- **언어**: C++ (OBS Plugin API)
- **빌드**: CMake 3.28+ / Xcode (macOS) / MSVC (Windows)
- **의존성**: IXWebSocket v11.4.5, nlohmann/json v3.11.3, OpenSSL
- **OBS SDK**: 31.1.1
- **소스**: `src/plugin-main.cpp` (단일 파일)

## Build

```bash
cmake --preset macos
cmake --build build_macos --config RelWithDebInfo
```

## Architecture

SonioxCaptionPlugin 구조 기반, Soniox → ElevenLabs 교체:
- WebSocket: `wss://api.elevenlabs.io/v1/speech-to-text/realtime`
- 인증: `xi-api-key` HTTP 헤더
- 오디오: PCM16 48kHz → Base64 → JSON
- 응답: `message_type` 기반 (`partial_transcript`, `committed_transcript`)
- VAD: `commit_strategy=vad` (자동 음성 구간 감지)

## Key Files

- `src/plugin-main.cpp` - 전체 플러그인 코드
- `CMakeLists.txt` - 빌드 설정
- `buildspec.json` - 플러그인 메타데이터
- `docs/technical-guide.md` - 상세 기술 문서

## Reference Projects

- `~/Documents/SonioxCaptionPlugIn/` - 원본 OBS 플러그인 (구조 템플릿)
- `~/Documents/ELSTTv2/` - ElevenLabs Scribe v2 웹앱 (API 패턴 참조)
