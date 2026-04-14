# AudioSpeedHack

[简体中文](README.md) | English

Background: [SPEED UP!](https://absx.pages.dev/articles/speedup.html)

A galgame toolset that includes several audio tools based on DLL injection.

1. **Galgame Audio Speedup** (SPEEDUP)
2. **Uninterrupted Voice (Experimental)** (ZeroInterrupt)

## Quick Start

> [!TIP]
> AudioSpeedHack has been integrated into [GalgameManager](https://github.com/lxl66566/GalgameManager), includes full SPEEDUP and ZeroInterrupt functions. It is recommended to use [GalgameManager](https://github.com/lxl66566/GalgameManager) for the best GUI experience.

1. Download the latest package from [Github Releases](https://github.com/lxl66566/AudioSpeedHack/releases), extract it, and place it in the game directory.
2. Double-click to enter TUI mode and select _Voice Speedup (SPEEDUP)_:
   1. For the extracted DLL, generally choose `MMDevAPI`, as it has the broadest compatibility on modern Windows.
   2. Select `Auto/x64` for the game architecture; the program supports automatic detection of the exe file architecture (fallback to x64).
   3. Set the speed to your desired acceleration multiplier.
   4. Select your game’s exe file to automatically detect the architecture.
   5. Finally, select _Confirm!_ and press Enter. The program will finish, then launch the game.
   6. After exiting the game, press Enter again to clear the registry and DLL and exit.
3. Uninterrupted Voice (ZeroInterrupt) operates similarly.

For more usage instructions, run `AudioSpeedHack -h` in the command line or read the source code.

## Tested Games

> Tested on Windows 11. Multiple DLLs listed indicate that any of them can be used.

### SPEEDUP

<!-- Please sort the table in ascending order. -->
<!-- prettier-ignore -->
|DLL|Architecture|Engine|Game Name|Test Version|
|---|---|---|---|---|
|dsound<br/>MMDevAPI|x64|Artemis|[FLIP＊FLOP](https://vndb.org/v39197)（全系列）|v1.1.0|
|dsound<br/>MMDevAPI|x86|Artemis|[天选庶民的真命之选](https://vndb.org/v47175)|v1.1.0|
|dsound<br/>MMDevAPI|x86|Artemis|[樱之刻](https://vndb.org/v20431)|v1.1.0|
|dsound<br/>MMDevAPI|x86|BGI|[大图书馆的牧羊人](https://vndb.org/v8158)（全系列）|v1.1.0|
|dsound<br/>MMDevAPI|x86|BGI|[ジュエリー・ハーツ・アカデミア -We will wing wonder world-](https://vndb.org/v33175)|v1.1.0|
|dsound<br/>MMDevAPI|x86|FVP|[五彩斑斓的世界](https://vndb.org/v5834)|v1.1.0|
|dsound<br/>MMDevAPI|x86|FVP|[樱花，萌放](https://vndb.org/v22313)|v1.1.0|
|dsound<br/>MMDevAPI|x86|Kirikiri|[瑠璃櫻](https://vndb.org/v30970)|v1.1.0|
|dsound<br/>MMDevAPI|x86|Kirikiri|[死に逝く騎士、異世界に響く断末魔](https://vndb.org/v49274)|v1.1.0|
|dsound<br/>MMDevAPI|x86|Kirikiri|[Alice＊Gram](https://vndb.org/v19133)（全系列）|v1.1.0|
|dsound<br/>MMDevAPI|x86|Kirikiri|[Deep One -ディープワン](https://vndb.org/v22499)|v1.1.0|
|dsound<br/>MMDevAPI|x86|Kirikiri|[サルテ](https://vndb.org/v26999)|v1.1.0|
|dsound<br/>MMDevAPI|x86|MAGES. Engine|[Ever17](https://vndb.org/v19373)|v1.1.0|
|dsound<br/>MMDevAPI|x86|Ren'Py|[Chaos;Head Noah](https://vndb.org/v22505)|v1.1.0|
|dsound<br/>MMDevAPI|x86|SiglusEngine|[虹彩都市](https://vndb.org/v48532)|v1.1.0|
|dsound<br/>MMDevAPI|x86|Yaneurao|[まほ×ろば -Witches spiritual home-](https://vndb.org/v21455)|v1.1.0|
|dsound<br/>MMDevAPI|x86|YU-RIS|[猫忍之心](https://vndb.org/v45381)（全系列）|v1.1.0|
|dsound<br/>MMDevAPI|x86|YU-RIS|[アンレス テルミナリア](https://vndb.org/v32757)|v1.1.0|
|MMDevAPI|x64|FuriKuru|[諦観のイヴ・ベセル](https://vndb.org/v43113)|v1.2.0|
|MMDevAPI|x64|Light.vn|[カタネガイ](https://vndb.org/v55892)|v1.1.0|
|MMDevAPI|x64|LucaSystem|[恋狱～月狂病～ FHD](https://vndb.org/v515)|v1.1.0|
|MMDevAPI|x64|TyranoScript (electron)|[传述之魔女](https://vndb.org/v32758)|v1.1.0|
|MMDevAPI|x64|Unity|[魔法少女的魔女审判](https://vndb.org/v50283)|v1.1.0|
|MMDevAPI|x86|-|[箱庭ロジック](https://vndb.org/v14924)|v1.1.0|
|MMDevAPI|x86|AliceSoft|[ランス03 リーザス陥落](https://vndb.org/v17642)|v1.1.0|
|MMDevAPI|x86|AVG32|[AIR](https://vndb.org/v36)|v1.1.0|
|MMDevAPI|x86|CatSystem2|[灰色的果实](https://vndb.org/v5154)|v1.1.0|
|MMDevAPI|x86|Escu:de|[廃村少女［弐］ ～陰り誘う秘姫の匣～](https://vndb.org/v53486)|v1.2.0|
|MMDevAPI|x86|QLIE|[美少女万華鏡異聞 雪おんな](https://vndb.org/v44184)|v1.1.0|
|MMDevAPI|x86|Silky Engine|[ふゆから、くるる。](https://vndb.org/v30012)|v1.1.0|

### ZeroInterrupt

<!-- Please sort the table in ascending order. -->
<!-- prettier-ignore -->
|Engine|Game Name|Test Version|Availability|
|---|---|---|---|
|Kirikiri (x86)|*|v1.2.0|❌|
|BGI (x86)|[The Shepherd of the Grand Library](https://vndb.org/v8158) (full series)<br/>[Jewellery Hearts Academia -We will wing wonder world-](https://vndb.org/v33175)|v1.2.0|❔|
|MAGES. Engine (x86)|[Ever17](https://vndb.org/v19373)|v1.2.0|✅|

## Principle

This program extracts two DLLs into the current directory. Among them, `dsound.dll` can be loaded by simply placing it in the game directory, while `MMDevAPI.dll` requires modifying the registry to be loaded. When loaded, the DLL reads the `SPEEDUP` environment variable for voice acceleration or loads an ONNX model to achieve uninterrupted voice.

### SPEEDUP V1

Version V1 processes the **audio data directly**: it accelerates the output of application audio data by faking the current padding, retrieves the audio data, uses SoundTouch to speed it up without changing the pitch, and then sends it to the sound card for playback.

Currently, V1 offers the best sound quality and good compatibility. It is recommended for general users.

### ZeroInterrupt (Experimental)

This feature is still experimental. Uninterrupted voice is based on dsound hooking, distinguishing voice from other audio using length features and the [silero-vad](https://github.com/snakers4/silero-vad) model. It buffers and plays voice data in a queue while intercepting the game’s stop signals.

Implementing uninterrupted voice is much more complex than acceleration (see [Background](#audiospeedhack)). It currently has some limitations but can shine on older games.

### SPEEDUP V0

<details>
<summary>Click to expand</summary>

Version V0 works by **first accelerating and raising the pitch of the audio, then lowering the pitch**. It accelerates audio data consumption by faking the sample rate, forcing the program to output faster, achieving the effect of acceleration and pitch raising. Then, it sends the audio to a virtual device (VB-CABLE) and restores the pitch before output.

V0 has lower sound quality and is more cumbersome to use. Unless you encounter bugs with V1, it is not recommended. If you need to use V0, please refer to the [V0 Release](https://github.com/lxl66566/AudioSpeedHack/releases/tag/v0.2.1) and read the corresponding README.

</details>

## Troubleshooting

- How to check if the current game uses a supported DLL?
  - For V1, directly enable "Unpack ALL" and run the game. If a `SPEEDUP_announcement.txt` file appears in the current directory, the DLL has been injected.
  - You can also use Microsoft’s official [Process Monitor](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon) tool:
    1. Run `Procmon64.exe`.
    2. Launch your game and ensure that audio has played.
    3. Switch to the Process Monitor window, click the "funnel" icon (Filter) on the toolbar to open the filter.
    4. Add filter rules:
       - `Process Name` is `your game exe`
       - `Path` contains `dsound`
       - `Path` contains `mmdevapi`
    5. Check the result list. If matching entries are found, this tool is likely applicable.

- No sound when launching the game  
  0. First, check your device and system volume to ensure audio plays normally without using this tool.
  1. Try using a specific DLL, such as only MMDevAPI instead of ALL, with 2.0x speed.
  2. Submit an issue.

- Actual speed exceeds the set value
  - Check if "Unpack ALL" was used. For programs that support both dsound and MMDevAPI, audio may be accelerated twice.

- Other software has no sound after using this tool
  - This is likely due to not clearing the registry entries. Run the program, select _Clean (Remove AudioSpeedHack Residuals)_, and then restart your other software.

- Other troubleshooting methods that may be useful when submitting an issue:
  - Debug env: `SPEEDUP_DEBUG=1`, then use [DebugView++](https://github.com/CobaltFusion/DebugViewPP) to view and copy the logs.

- Uninterrupted voice ineffective, crashes, audio popping, etc.
  - If the game is not in the test list, you can submit an issue, but logs must be provided.

## TODO

Contributions of any kind (Issues/PRs) are welcome.

- [x] Support other audio APIs
  - [x] MMDevAPI
- [x] **Sound quality improvement**
- [x] Uninterrupted voice
- [x] Better TUI interface, or GUI

## License

- The Rust core of AudioSpeedHack is licensed under the MIT License.
- The dsound.dll in the V0 source code originates from [dsoal fork](https://github.com/lxl66566/dsoal) and inherits GPLv2. The MMDevAPI.dll in V0 allows distribution and commercial use.
- For the DLL files in the V1 source code, except for third-party DLLs such as SoundTouch and onnxruntime, unauthorized commercial use is prohibited. The V1 DLL is not yet open-source; it may be considered open-source after more users.
