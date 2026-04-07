# ElevenLabs Captions for OBS

[English](README.en.md) | **한국어**

ElevenLabs Scribe v2 API를 사용한 OBS Studio **실시간 음성 자막** 플러그인입니다.
마이크에 말하면 즉시 자막이 화면에 표시됩니다.

---

## ElevenLabs Scribe v2란?

[ElevenLabs](https://elevenlabs.io)는 업계 최고 수준의 음성 AI 플랫폼이며, **Scribe v2**는 그들의 실시간 음성 인식(STT) 모델입니다.

- **57개 언어 지원**: 한국어, 영어, 일본어, 중국어 등 폭넓은 언어 지원
- **실시간 전사**: WebSocket 기반 스트리밍으로 낮은 지연시간
- **VAD (Voice Activity Detection)**: 음성 구간을 자동 감지하여 자연스러운 문장 단위로 확정
- **높은 정확도**: 최신 Scribe v2 모델로 업계 최상위 수준의 인식률
- **48kHz 네이티브 지원**: 다운샘플링 없이 고음질 오디오 직접 처리

## 주요 기능

- **실시간 음성→텍스트** (ElevenLabs Scribe v2 Realtime 모델)
- **VAD 자동 커밋** — 침묵 감지 시 자동으로 문장 확정 (민감도/침묵 시간 조절 가능)
- **48kHz 네이티브** — OBS 오디오를 다운샘플링 없이 직접 전송
- 단축키 지원 (Properties 열지 않고 시작/중지)
- 네트워크 끊김 시 자동 재연결
- 한중일(CJK) 폰트 지원
- 폰트 크기 조절 가능

## 빠른 시작

### 1. ElevenLabs API 키 발급

[elevenlabs.io](https://elevenlabs.io)에서 가입 후 API 키를 발급받으세요.

### 2. 다운로드

[**최신 Release 다운로드**](../../releases/latest)

| 플랫폼 | 파일 |
|--------|------|
| macOS (Apple Silicon) | `elevenlabs-caption-obs-v0.1.1-macos-arm64.zip` |
| Windows | `elevenlabs-caption-obs-0.1.1-windows-x64.zip` |
| Linux (Ubuntu) | `elevenlabs-caption-obs-0.1.1-x86_64-linux-gnu.deb` |

### 3. 설치

<details>
<summary><b>macOS</b></summary>

1. `elevenlabs-caption-obs-v0.1.1-macos-arm64.zip` 다운로드 후 압축 해제
2. OBS 메뉴 → **File** → **Show Settings Folder** 클릭
3. 열린 폴더에서 **plugins** 폴더로 이동
4. `elevenlabs-caption-obs.plugin` 을 **plugins** 폴더에 복사
5. OBS Studio 재시작

**제거:** OBS 메뉴 → **File** → **Show Settings Folder** → **plugins** 폴더에서 `elevenlabs-caption-obs.plugin` 삭제
</details>

<details>
<summary><b>Windows</b></summary>

1. `elevenlabs-caption-obs-0.1.1-windows-x64.zip` 다운로드 후 압축 해제
2. 내용물을 아래 경로로 복사:
   ```
   %APPDATA%\obs-studio\plugins\elevenlabs-caption-obs\
   ```
3. OBS Studio 재시작
</details>

<details>
<summary><b>Linux (Ubuntu)</b></summary>

```bash
sudo dpkg -i elevenlabs-caption-obs-0.1.1-x86_64-linux-gnu.deb
```

또는 수동으로 `~/.config/obs-studio/plugins/elevenlabs-caption-obs/` 에 복사
</details>

### 4. 사용법

1. OBS에서 소스 **+** 클릭 → **ElevenLabs Captions** 선택
2. 소스 우클릭 → **속성**:
   - **ElevenLabs API Key** 입력
   - **Audio Source** 에서 마이크 선택 (예: Mic/Aux)
   - **Language** 선택
   - (선택) **VAD Threshold** / **Silence Duration** 조절
3. **Start Caption** 클릭
4. 마이크에 말하면 실시간 자막이 화면에 표시됩니다!

### 단축키

**OBS 설정 → 단축키 → Toggle ElevenLabs Captions** 에서 단축키를 지정하면 Properties를 열지 않고도 시작/중지할 수 있습니다.

---

## 소스 빌드

<details>
<summary>펼치기</summary>

### 사전 요구사항

- CMake 3.28+
- Xcode 16+ (macOS) / Visual Studio 2022 (Windows) / GCC 12+ (Linux)
- OpenSSL (macOS: `brew install openssl`)

### macOS

```bash
cmake --preset macos
cmake --build build_macos --config RelWithDebInfo
# 결과물: build_macos/RelWithDebInfo/elevenlabs-caption-obs.plugin
```

### Windows

```bash
cmake --preset windows-x64
cmake --build --preset windows-x64
```

### Linux

```bash
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
```

모든 의존성(IXWebSocket, nlohmann/json, OBS SDK)은 CMake FetchContent를 통해 자동 다운로드됩니다.

</details>

## 후원

이 프로젝트가 도움이 되셨다면 커피 한 잔 사주세요!

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-FFDD00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/inseokko)

## 라이선스

GPL-2.0 - [LICENSE](LICENSE) 참조
