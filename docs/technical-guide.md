# ElevenLabs Captions for OBS - Technical Guide

## Architecture Overview

```
OBS Audio Source (float32, 48kHz)
    │
    ▼
Audio Capture Callback
    ├─ float32 → int16 PCM 변환 (48kHz 유지, 다운샘플링 없음)
    ├─ Base64 인코딩
    └─ JSON 메시지로 래핑
    │
    ▼
ElevenLabs WebSocket (wss://api.elevenlabs.io/v1/speech-to-text/realtime)
    ├─ 인증: xi-api-key HTTP 헤더
    ├─ 설정: URL 쿼리파라미터 (model_id, audio_format, commit_strategy 등)
    └─ 응답: message_type 기반 JSON
    │
    ▼
Message Handler
    ├─ session_started → "Listening..." 표시
    ├─ partial_transcript → 실시간 부분 텍스트 표시
    └─ committed_transcript → 확정 텍스트 표시
    │
    ▼
OBS Text Source (text_ft2_source_v2 / text_gdiplus)
    └─ 화면에 자막 렌더링
```

## ElevenLabs Scribe v2 WebSocket Protocol

### Connection

```
URL: wss://api.elevenlabs.io/v1/speech-to-text/realtime
     ?model_id=scribe_v2_realtime
     &audio_format=pcm_48000
     &commit_strategy=vad
     &language_code=ko
     &vad_silence_threshold_secs=0.4
     &vad_threshold=0.4

Header: xi-api-key: <YOUR_API_KEY>
```

### Client → Server (Audio Chunk)

```json
{
  "message_type": "input_audio_chunk",
  "audio_base_64": "<base64-encoded PCM16 audio>",
  "commit": false,
  "sample_rate": 48000
}
```

### Server → Client (Responses)

| message_type | 설명 |
|---|---|
| `session_started` | 세션 시작, `session_id` 포함 |
| `partial_transcript` | 실시간 부분 텍스트 (`text` 필드) |
| `committed_transcript` | VAD에 의해 확정된 텍스트 |
| `committed_transcript_with_timestamps` | 단어별 타임스탬프 포함 확정 텍스트 |
| `auth_error`, `quota_exceeded`, 등 | 에러 (`error` 필드) |

### Audio Format

- **인코딩**: PCM 16-bit signed little-endian
- **샘플레이트**: 48000 Hz (OBS 네이티브, 다운샘플링 불필요)
- **채널**: Mono (1ch)
- **전송방식**: Base64 인코딩 후 JSON 래핑

## Soniox vs ElevenLabs 비교

| 항목 | Soniox | ElevenLabs |
|------|--------|------------|
| WebSocket URL | `wss://stt-rt.soniox.com/transcribe-websocket` | `wss://api.elevenlabs.io/v1/speech-to-text/realtime` |
| 인증 | config JSON에 api_key | xi-api-key HTTP 헤더 |
| 설정 전달 | Open 후 JSON 메시지 | URL 쿼리파라미터 |
| 오디오 전송 | Raw binary PCM | JSON + base64 |
| 샘플레이트 | 16kHz (48→16 다운샘플링) | 48kHz 네이티브 지원 |
| 응답 형식 | `tokens[]` 배열 + `is_final` | `message_type` 기반 분기 |
| 발화 종료 | `<end>` 토큰 | `committed_transcript` 이벤트 |
| 내장 번역 | 지원 (one_way) | 미지원 |
| VAD 설정 | 서버 기본값 | 클라이언트 설정 가능 |

## Dependencies

| 라이브러리 | 버전 | 용도 |
|---|---|---|
| OBS Studio SDK | 31.1.1 | 플러그인 API |
| IXWebSocket | v11.4.5 | WebSocket 클라이언트 (WSS/TLS) |
| nlohmann/json | v3.11.3 | JSON 파싱 |
| OpenSSL | 3.x | TLS (via Homebrew on macOS) |

## Build

```bash
# macOS (arm64)
cmake --preset macos
cmake --build build_macos --config RelWithDebInfo

# 결과: build_macos/RelWithDebInfo/elevenlabs-caption-obs.plugin
```

## CI/CD (GitHub Actions)

### 트리거
- **태그 push만** 트리거 (main 브랜치 push 시 Actions 실행 안 함)
- 태그 형식: `0.1.1`, `0.1.1-beta1`, `0.1.1-rc1`

### 빌드 매트릭스
| 플랫폼 | 타겟 | Runner |
|--------|------|--------|
| macOS (Apple Silicon) | `macos-arm64` | macos-15 |
| macOS (Intel) | `macos-x86_64` | macos-15 (cross-compile) |
| Windows | `x64` | windows-2022 |
| Ubuntu | `x86_64` | ubuntu-24.04 |

### Intel Mac (x86_64) 빌드 참고사항
- macos-15 runner(ARM64)에서 x86_64 크로스 컴파일
- Intel Homebrew (`/usr/local/bin/brew`)로 x86_64 OpenSSL 별도 설치
- CMake preset `macos-ci-x86_64`에서 `OPENSSL_ROOT_DIR=/usr/local/opt/openssl@3` 설정
- `CMakeLists.txt`에서 preset이 `OPENSSL_ROOT_DIR`을 이미 설정한 경우 `brew --prefix openssl` 건너뜀

### 릴리스 절차
```bash
# 1. buildspec.json 버전 업데이트
# 2. 태그 생성 및 push
git tag 0.x.x && git push origin 0.x.x
# 3. Actions가 빌드 + draft 릴리스 자동 생성
# 4. gh release edit --draft=false 로 publish
```

### OBS에 설치

```bash
# macOS
cp -r build_macos/RelWithDebInfo/elevenlabs-caption-obs.plugin \
  ~/Library/Application\ Support/obs-studio/plugins/
```

## Source Structure

```
ElevenlabsCaptionPlugin/
├── CMakeLists.txt              # 빌드 설정 (IXWebSocket, nlohmann/json)
├── CMakePresets.json            # 플랫폼별 프리셋 (macos-ci, macos-ci-x86_64 등)
├── buildspec.json               # 플러그인 메타데이터, OBS SDK 버전
├── src/
│   ├── plugin-main.cpp          # 전체 플러그인 로직 (~700 lines)
│   ├── plugin-support.h         # 로깅 헤더
│   └── plugin-support.c.in      # 로깅 구현 (CMake 생성)
├── data/locale/en-US.ini        # i18n
├── cmake/                       # OBS 플러그인 CMake 인프라
├── docs/
│   └── technical-guide.md       # 이 문서
└── todo.txt                     # 프로젝트 할일
```

## Key Design Decisions

1. **48kHz 직접 전송**: ElevenLabs가 pcm_48000을 지원하므로 다운샘플링 제거 → 코드 단순화 + 음질 보존
2. **xi-api-key 헤더 인증**: Single-use token 발급 대신 직접 API 키 사용 → OBS 플러그인(로컬 실행)에 적합
3. **VAD commit_strategy**: 자동 음성 구간 감지로 수동 commit 불필요
4. **번역 제거**: ElevenLabs Scribe v2에 내장 번역 없음 → 필요 시 DeepL API 별도 연동
5. **Base64 인코딩**: ElevenLabs가 JSON+base64 요구 → `base64_encode()` 인라인 구현
6. **버튼 텍스트 토글**: `obs_property_set_description()`을 콜백 내에서 직접 호출. `return true`는 `RefreshProperties()`만 트리거하므로 `get_properties()` 재호출 없이 description 변경 반영

## Changelog

### 0.1.3 (2026-04-10)
- Feature: 시스템 폰트 피커 (`obs_properties_add_font`) - 기존 드롭다운+슬라이더 대체
- Feature: 텍스트 색상 (`color1`, `color2`) - Gradient 지원 (macOS/Linux)
- Feature: 텍스트 효과 (`outline`, `drop_shadow`)
- Feature: 텍스트 레이아웃 (`custom_width`, `word_wrap`)
- Improve: 플랫폼별 텍스트 렌더링 (Windows: `text_gdiplus`, macOS/Linux: `text_ft2_source_v2`)

### 0.1.2 (2026-04-07)
- Fix: Start/Stop 버튼 텍스트가 클릭 후 토글되지 않던 문제 수정
  - `on_start_stop_clicked` 콜백에서 `obs_property_set_description()` 호출 추가

### 0.1.1
- Fix: Intel Mac (x86_64) 빌드 지원
- CI/CD: 태그 트리거 + 멀티플랫폼 빌드 + draft 릴리스 자동화

### 0.1.0
- Initial release: ElevenLabs Scribe v2 기반 실시간 자막 플러그인
