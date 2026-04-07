# ElevenLabs Captions for OBS

OBS Studio 플러그인 - ElevenLabs Scribe v2를 이용한 실시간 음성 자막

## Project

- **언어**: C++ (OBS Plugin API)
- **빌드**: CMake 3.28+ / Xcode (macOS) / MSVC (Windows)
- **의존성**: IXWebSocket v11.4.5, nlohmann/json v3.11.3, OpenSSL
- **OBS SDK**: 31.1.1
- **버전**: 0.1.2
- **소스**: `src/plugin-main.cpp` (단일 파일)

## Build

```bash
# macOS (arm64)
cmake --preset macos
cmake --build build_macos --config RelWithDebInfo

# macOS (x86_64) - Intel Homebrew OpenSSL 필요
cmake --preset macos-ci-x86_64
cmake --build build_macos --config RelWithDebInfo
```

## CI/CD

- **트리거**: 태그 push만 (main push 시 Actions 안 함)
- **빌드**: macOS arm64 + x86_64, Windows x64, Ubuntu x86_64
- **릴리스**: 태그 push → 자동 빌드 → draft 릴리스 생성
- **x86_64 주의**: CMake preset이 OPENSSL_ROOT_DIR 설정 시 brew --prefix 건너뜀

## Architecture

SonioxCaptionPlugin 구조 기반, Soniox → ElevenLabs 교체:
- WebSocket: `wss://api.elevenlabs.io/v1/speech-to-text/realtime`
- 인증: `xi-api-key` HTTP 헤더
- 오디오: PCM16 48kHz → Base64 → JSON
- 응답: `message_type` 기반 (`partial_transcript`, `committed_transcript`)
- VAD: `commit_strategy=vad` (자동 음성 구간 감지)

## Key Files

- `src/plugin-main.cpp` - 전체 플러그인 코드
- `CMakeLists.txt` - 빌드 설정 (OpenSSL preset 분기 포함)
- `CMakePresets.json` - 플랫폼별 프리셋 (macos-ci, macos-ci-x86_64 등)
- `buildspec.json` - 플러그인 메타데이터 + 버전
- `.github/workflows/push.yaml` - CI/CD (태그 트리거 + 릴리스)
- `.github/workflows/build-project.yaml` - 멀티플랫폼 빌드
- `.github/scripts/build-macos` - macOS 빌드 스크립트 (--arch 지원)
- `docs/technical-guide.md` - 상세 기술 문서 (changelog 포함)

## Reference Projects

- `~/Documents/SonioxCaptionPlugIn/` - 원본 OBS 플러그인 (구조 템플릿)
- `~/Documents/ELSTTv2/` - ElevenLabs Scribe v2 웹앱 (API 패턴 참조)
