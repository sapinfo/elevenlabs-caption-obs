# ElevenLabs Captions for OBS

**English** | [한국어](README.md)

**Real-time speech-to-text captions** for OBS Studio using the [ElevenLabs](https://elevenlabs.io) Scribe v2 API.
Speak into your microphone and see captions appear instantly on screen.

---

## What is ElevenLabs Scribe v2?

[ElevenLabs](https://elevenlabs.io) is a leading voice AI platform, and **Scribe v2** is their real-time speech-to-text (STT) model.

- **57 languages supported**: Korean, English, Japanese, Chinese, and many more
- **Real-time transcription**: Low-latency streaming via WebSocket
- **VAD (Voice Activity Detection)**: Automatically detects speech segments for natural sentence-level commits
- **High accuracy**: Industry-leading recognition with the latest Scribe v2 model
- **48kHz native support**: Processes high-quality audio without downsampling

## Features

- **Real-time speech-to-text** (ElevenLabs Scribe v2 Realtime model)
- **VAD auto-commit** — automatically finalizes sentences on silence detection (configurable sensitivity and duration)
- **48kHz native** — sends OBS audio directly without downsampling
- Hotkey support (start/stop without opening Properties)
- Auto-reconnect on disconnect
- CJK font support
- Configurable font size

## Quick Start

### 1. Get ElevenLabs API Key

Sign up at [elevenlabs.io](https://elevenlabs.io) and get your API key.

### 2. Download

[**Download Latest Release**](../../releases/latest)

| Platform | File |
|----------|------|
| macOS (Apple Silicon) | `elevenlabs-caption-obs-v0.1.1-macos-arm64.zip` |
| Windows | `elevenlabs-caption-obs-0.1.1-windows-x64.zip` |
| Linux (Ubuntu) | `elevenlabs-caption-obs-0.1.1-x86_64-linux-gnu.deb` |

### 3. Install

<details>
<summary><b>macOS</b></summary>

1. Download and unzip `elevenlabs-caption-obs-v0.1.1-macos-arm64.zip`
2. In OBS, go to **File** → **Show Settings Folder**
3. Open the **plugins** folder
4. Copy `elevenlabs-caption-obs.plugin` into the **plugins** folder
5. Restart OBS Studio

**Uninstall:** In OBS, go to **File** → **Show Settings Folder** → **plugins** folder → delete `elevenlabs-caption-obs.plugin`
</details>

<details>
<summary><b>Windows</b></summary>

1. Download and unzip `elevenlabs-caption-obs-0.1.1-windows-x64.zip`
2. Copy contents to:
   ```
   %APPDATA%\obs-studio\plugins\elevenlabs-caption-obs\
   ```
3. Restart OBS Studio
</details>

<details>
<summary><b>Linux (Ubuntu)</b></summary>

```bash
sudo dpkg -i elevenlabs-caption-obs-0.1.1-x86_64-linux-gnu.deb
```

Or manually copy to `~/.config/obs-studio/plugins/elevenlabs-caption-obs/`
</details>

### 4. Usage

1. In OBS, click **+** in Sources → select **ElevenLabs Captions**
2. Right-click the source → **Properties**:
   - Enter your **ElevenLabs API Key**
   - Select **Audio Source** (e.g., Mic/Aux)
   - Choose **Language**
   - (Optional) Adjust **VAD Threshold** / **Silence Duration**
3. Click **Start Caption**
4. Speak into your microphone — captions appear in real-time!

### Hotkey

Assign a hotkey in **OBS Settings → Hotkeys → Toggle ElevenLabs Captions** to start/stop without opening Properties.

---

## Build from Source

<details>
<summary>Expand</summary>

### Prerequisites

- CMake 3.28+
- Xcode 16+ (macOS) / Visual Studio 2022 (Windows) / GCC 12+ (Linux)
- OpenSSL (macOS: `brew install openssl`)

### macOS

```bash
cmake --preset macos
cmake --build build_macos --config RelWithDebInfo
# Output: build_macos/RelWithDebInfo/elevenlabs-caption-obs.plugin
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

All dependencies (IXWebSocket, nlohmann/json, OBS SDK) are automatically downloaded via CMake FetchContent.

</details>

## Support

If you find this project helpful, buy me a coffee!

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-FFDD00?style=for-the-badge&logo=buy-me-a-coffee&logoColor=black)](https://buymeacoffee.com/inseokko)

## License

GPL-2.0 - see [LICENSE](LICENSE)
