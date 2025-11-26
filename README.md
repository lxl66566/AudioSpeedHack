# AudioSpeedHack

背景：[SPEED UP!](https://absx.pages.dev/articles/speedup.html)

一个 galgame 小工具，**基于 dll 注入，加速 galgame 音频**。

## 基本用法（V1）

1. 从 [Github Releases](https://github.com/lxl66566/AudioSpeedHack/releases) 下载最新的压缩包，解压后放到游戏所在目录。
2. 双击执行进入 TUI 模式，选择 _UnpackDll (解压 DLL)_：
   1. 解压的 DLL 一般选择 `ALL` 即可；有时为了避免 DLL 互相影响，可能需要解压特定 DLL。
   2. 游戏架构选择 `Auto/x64` 即可，程序支持自动检测 exe 文件架构。(fallback x64)
   3. 速度设为你想要的加速倍率。
   4. 执行程序选择你的游戏 exe 文件，以自动检测架构。
   5. 最后选中 _确认！_ 按下 enter 键，程序运行完毕，然后打开游戏即可。
   6. 游戏结束后，再次按下 enter 键，清除注册表和 DLL 并退出。

也可以在命令行中执行 `AudioSpeedHack -h` 查看命令行用法。

## 成功实现加速的游戏

> 于 Windows 11 系统上测试。多个 DLL 表示选任一均可用。

<!-- 请按升序排列表格。 -->
<!-- prettier-ignore -->
|DLL|架构|引擎|游戏名|测试版本|
|---|---|---|---|---|
|dsound.dll<br/>MMDevAPI.dll|x86|BGI|大图书馆的牧羊人（全系列）|v1.1.0|
|dsound.dll<br/>MMDevAPI.dll|x86|BGI|ジュエリー・ハーツ・アカデミア -We will wing wonder world-|v1.1.0|
|dsound.dll<br/>MMDevAPI.dll|x86|Kirikiri|春音 Alice＊Gram，白恋 Sakura＊Gram|v1.1.0|
|dsound.dll<br/>MMDevAPI.dll|x86|Kirikiri|Deep One -ディープワン|v1.1.0|
|dsound.dll<br/>MMDevAPI.dll|x86|YU-RIS|猫忍之心（全系列）|v1.1.0|
|MMDevAPI.dll|x64|LucaSystem|恋狱～月狂病～ FHD|v1.1.0|
|MMDevAPI.dll|x64|TyranoScript (electron)|传述之魔女|v1.1.0|
|MMDevAPI.dll|x64|Unity|魔法少女的魔女审判|v1.1.0|
|MMDevAPI.dll|x86|QLIE|美少女万華鏡異聞 雪おんな|v1.1.0|
|MMDevAPI.dll|x86|Silky Engine|ふゆから、くるる。|v1.1.0|

## 原理

本程序会解压两个 DLL 到当前目录下。其中 dsound.dll 丢进游戏目录即可加载，而 MMDevAPI.dll 需要修改注册表才能加载。

### V1

V1 版本是对**音频数据本身直接处理**：通过伪造 current padding，加速应用音频数据输出；并获取获取到的音频数据，使用 SoundTouch 进行不变调加速后，再交给声卡播放。

目前 V1 版本拥有最好的音质与不错的兼容性，推荐一般玩家使用该版本。

### V0

V0 版本是对音频**先加速升调，再降调**得到的，通过伪造采样率加速音频数据消耗，迫使程序更快输出，达到加速升调的效果；然后再通过 VB-CABLE 虚拟设备送到程序中，还原音高后输出。

V0 版本音质较差，并且使用起来更加麻烦。除非用 V1 版本遇到 bug，否则不建议使用 V0。若要使用，请[前往 V0 Release](https://github.com/lxl66566/AudioSpeedHack/releases/tag/v0.2.1) 阅读对应版本 README。

## 问题排查

### 如何判断当前游戏是否使用支持的 dll？

- 对 V1 版本，直接 Unpack ALL，运行游戏，如果当前目录下出现 `SPEEDUP_announcement.txt` 文件，则说明 DLL 已注入。
- 也可以使用微软官方的 [Process Monitor](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon) 工具来检查：
  1.  运行 `Procmon64.exe`。
  2.  启动您的游戏，并确保游戏已经播放了一段音频。
  3.  切换到 Process Monitor 窗口，点击工具栏上的“漏斗”图标 (Filter) 打开筛选器。
  4.  添加筛选规则：
      - `Process Name` is `你的游戏 exe`
      - `Path` contains `dsound`
      - `Path` contains `mmdevapi`
  5.  查看结果列表。如果能找到匹配的条目，则说明此工具很可能适用。

### 打开游戏没有听到任何声音

0. 先检查设备和系统音量，确认在不使用该工具的情况下，音频正常播放。
1. 尝试使用 2.0 倍速的特定 DLL，例如只使用 MMDevAPI，而不是 ALL。
2. 提出 issue。

### 实际倍速超过了自己的设定

检查是否 Unpack ALL。对于同时支持 dsound 和 MMDevAPI 的程序，音频会被加速两次。

### 用了这个工具，再打开其他软件发现没声音了

大概率是没有清除注册表项导致的，请运行程序，选择 _Clean (清除 AudioSpeedHack 残留)_ 后，重启你的其他软件。

## TODO

欢迎任何形式的贡献（Issue/PR）。

- [ ] [issue 区](https://github.com/lxl66566/AudioSpeedHack/issues)
- [x] 支持其他音频 API
  - [x] MMDevAPI
  <!-- - [ ] xaudio2
  - [ ] winmm -->
- [x] **音质改善**
- [ ] 更好的 TUI 界面，或 GUI

## License

- AudioSpeedHack 的 Rust 本体遵循 MIT 协议。
- V0 源码中的 dsound.dll 来自 [dsoal fork](https://github.com/lxl66566/dsoal)，继承 GPLv2。V0 的 MMDevAPI.dll 允许分发与商用。
- V1 源码中的 DLL 文件，除 SoundTouch 外，均禁止未授权的商用。
