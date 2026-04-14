# AudioSpeedHack

简体中文 | [English](README-en.md)

背景：[SPEED UP!](https://absx.pages.dev/articles/speedup.html)

一个 galgame 工具集，包含了一些基于 dll 注入的音频工具。

1. **galgame 音频加速** (SPEEDUP)
2. **语音不中断（实验性）** (ZeroInterrupt)

## 快速入门

> [!TIP]
> AudioSpeedHack 现已集成到 [GalgameManager](https://github.com/lxl66566/GalgameManager) 中，包含完整的音频加速与语音不中断功能。推荐直接使用 [GalgameManager](https://github.com/lxl66566/GalgameManager) 以获取最丝滑的 GUI 使用体验。

1. 从 [Github Releases](https://github.com/lxl66566/AudioSpeedHack/releases) 下载最新的压缩包，解压后放到游戏所在目录。
2. 双击执行进入 TUI 模式，选择 _语音加速 (SPEEDUP)_：
   1. 解压的 DLL 一般选择 `MMDevAPI` 即可，在现代 Windows 下它的泛用性最高。
   2. 游戏架构选择 `Auto/x64` 即可，程序支持自动检测 exe 文件架构。(fallback x64)
   3. 速度设为你想要的加速倍率。
   4. 执行程序选择你的游戏 exe 文件，以自动检测架构。
   5. 最后选中 _确认！_ 按下 enter 键，程序运行完毕，然后打开游戏即可。
   6. 游戏结束后，再次按下 enter 键，清除注册表和 DLL 并退出。
3. 语音不中断 (ZeroInterrupt) 也类似。

更多用法请在命令行中执行 `AudioSpeedHack -h`，或阅读源码。

## 通过测试的游戏

> 于 Windows 11 系统上测试。多个 DLL 表示选任一均可用。

### SPEEDUP

<!-- 请按升序排列表格。 -->
<!-- prettier-ignore -->
|DLL|架构|引擎|游戏名|测试版本|
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

<!-- 请按升序排列表格。 -->
<!-- prettier-ignore -->
|引擎|游戏名|测试版本|可用性|
|---|---|---|---|
|Kirikiri (x86)|*|v1.2.0|❌|
|BGI (x86)|[大图书馆的牧羊人](https://vndb.org/v8158)（全系列）<br/>[ジュエリー・ハーツ・アカデミア -We will wing wonder world-](https://vndb.org/v33175)|v1.2.0|❔|
|MAGES. Engine (x86)|[Ever17](https://vndb.org/v19373)|v1.2.0|✅|

## 原理

本程序会解压两个 DLL 到当前目录下。其中 dsound.dll 丢进游戏目录即可加载，而 MMDevAPI.dll 需要修改注册表才能加载。dll 加载时读取 `SPEEDUP` 环境变量进行语音加速，或加载 onnx 模型实现语音不中断。

### SPEEDUP V1

V1 版本是对**音频数据本身直接处理**：通过伪造 current padding，加速应用音频数据输出；并获取获取到的音频数据，使用 SoundTouch 进行不变调加速后，再交给声卡播放。

目前 V1 版本拥有最好的音质与不错的兼容性，推荐一般玩家使用该版本。

### ZeroInterrupt (Experimental)

当前仍然为实验性质。语音不中断基于 dsound hook，通过长度特征和 [silero-vad](https://github.com/snakers4/silero-vad) 模型判别语音与其他音频，对语音进行队列缓冲播放，同时拦截游戏的停止信号。

语音不中断的实现比加速要复杂得多（见[背景](#audiospeedhack)），目前仍有一定局限性，不过在部分较老的游戏上可以大放异彩。

### SPEEDUP V0

<details>
<summary>点击展开</summary>

V0 版本是对音频**先加速升调，再降调**得到的，通过伪造采样率加速音频数据消耗，迫使程序更快输出，达到加速升调的效果；然后再通过 VB-CABLE 虚拟设备送到程序中，还原音高后输出。

V0 版本音质较差，并且使用起来更加麻烦。除非用 V1 版本遇到 bug，否则不建议使用 V0。若要使用，请[前往 V0 Release](https://github.com/lxl66566/AudioSpeedHack/releases/tag/v0.2.1) 阅读对应版本 README。

</details>

## 问题排查

- 如何判断当前游戏是否使用支持的 dll？
  - 对 V1 版本，直接加速 Unpack ALL，运行游戏，如果当前目录下出现 `SPEEDUP_announcement.txt` 文件，则说明 DLL 已注入。
  - 也可以使用微软官方的 [Process Monitor](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon) 工具来检查：
    1.  运行 `Procmon64.exe`。
    2.  启动您的游戏，并确保游戏已经播放了一段音频。
    3.  切换到 Process Monitor 窗口，点击工具栏上的“漏斗”图标 (Filter) 打开筛选器。
    4.  添加筛选规则：
        - `Process Name` is `你的游戏 exe`
        - `Path` contains `dsound`
        - `Path` contains `mmdevapi`
    5.  查看结果列表。如果能找到匹配的条目，则说明此工具很可能适用。

- 打开游戏没有听到任何声音 0. 先检查设备和系统音量，确认在不使用该工具的情况下，音频正常播放。
  1. 尝试使用 2.0 倍速的特定 DLL，例如只使用 MMDevAPI，而不是 ALL。
  2. 提出 issue。

- 实际倍速超过了自己的设定
  - 检查是否 Unpack ALL。对于同时支持 dsound 和 MMDevAPI 的程序，音频可能会被加速两次。

- 用了这个工具，再打开其他软件发现没声音了
  - 大概率是没有清除注册表项导致的，请运行程序，选择 _Clean (清除 AudioSpeedHack 残留)_ 后，重启你的其他软件。

- 其他排查方法，提 issue 时可能会用到。
  - debug env: `SPEEDUP_DEBUG=1`，然后使用 [DebugView++](https://github.com/CobaltFusion/DebugViewPP) 查看并复制日志。

- 语音不中断无效，闪退，爆音等。
  - 如果游戏不在测试列表中，可以提出 issue，需要同时提供日志。

## TODO

欢迎任何形式的贡献（Issue/PR）。

- [x] 支持其他音频 API
  - [x] MMDevAPI
  <!-- - [ ] xaudio2
  - [ ] winmm -->
- [x] **音质改善**
- [x] 语音不中断
- [x] [更好的 TUI 界面，或 GUI](https://github.com/lxl66566/GalgameManager)

## License

- AudioSpeedHack 的 Rust 本体遵循 MIT 协议。
- V0 源码中的 dsound.dll 来自 [dsoal fork](https://github.com/lxl66566/dsoal)，继承 GPLv2。V0 的 MMDevAPI.dll 允许分发与商用。
- V1 源码中的 DLL 文件，除 SoundTouch、onnxruntime 等三方 DLL 外，均禁止未授权的商用。V1 DLL 暂未开源，user 数多了以后会考虑开源。
