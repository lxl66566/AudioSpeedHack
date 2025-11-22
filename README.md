# AudioSpeedHack

背景：[SPEED UP!](https://absx.pages.dev/articles/speedup.html)

一个 galgame 小工具，**基于 dll 注入 加速 galgame 音频**。

## 基本用法（V1）

1. 从 [Github Releases](https://github.com/lxl66566/AudioSpeedHack/releases) 下载最新的压缩包，解压后放到游戏所在目录。
2. 双击执行进入 TUI 模式，选择 _UnpackDll (解压 DLL)_：
   1. 解压的 DLL 一般选择 `ALL` 即可；有时候为了避免 DLL 互相影响，可能需要解压特定 DLL。
   2. 游戏架构选择 `Auto/x64` 即可，程序支持自动检测 exe 文件架构。(fallback x64)
   3. 速度设为你想要的加速倍率。
   4. 执行程序选择你的游戏 exe 文件，以自动检测。
   5. 最后点击 _确认！_，然后打开游戏即可。

或者在命令行中执行 `AudioSpeedHack -h` 查看命令行用法。

## 成功实现加速的游戏

> 于 Windows 11 系统上测试

<!-- 请按升序排列表格。 -->
<!-- prettier-ignore -->
|DLL|架构|引擎|游戏名|可用版本|
|---|---|---|---|---|
|dsound.dll|x86|BGI|ジュエリー・ハーツ・アカデミア -We will wing wonder world-|ALL|
|dsound.dll|x86|Kirikiri|春音 Alice＊Gram，白恋 Sakura＊Gram|ALL|
|dsound.dll|x86|Kirikiri|Deep One -ディープワン|ALL|
|dsound.dll|x86|YU-RIS|猫忍之心 全系列|ALL|
|MMDevAPI.dll|x64|TyranoScript (electron)|传述之魔女|V0|
|MMDevAPI.dll|x64|Unity|魔法少女的魔女审判|ALL|
|MMDevAPI.dll|x64|LucaSystem|恋狱～月狂病～ FHD|V0|
|MMDevAPI.dll|x86|QLIE|美少女万華鏡異聞 雪おんな|V0|
|MMDevAPI.dll|x86|Silky Engine|ふゆから、くるる。|V0|

## 原理

所有版本程序都能够注入 dsound.dll 和 MMDevAPI.dll。其中 dsound.dll 丢进游戏目录即可加载，而 MMDevAPI.dll 需要修改注册表才能加载。

### V1

V1 版本是对**音频数据本身直接处理**：通过伪造 Buffer Position，加速音频数据输出；并拦截获取到的音频数据，使用 SoundTouch WSOLA 进行不变调加速后，再交给声卡播放。

目前 V1 版本仅在部分游戏可用，泛用性稍差；但是音频质量更好。

### V0

V0 版本是对音频**先加速升调，再降调**得到的，通过伪造采样率加速音频数据消耗，迫使程序更快输出，达到加速升调的效果；然后再通过 VB-CABLE 虚拟设备送到程序中，还原音高后输出。

由于 V0 版本对 DLL 的修改较少，因此运行更加稳定，bug 更少；代价是音质较差，并且使用起来更加麻烦。

使用 V0 版本，请[前往 Release](https://github.com/lxl66566/AudioSpeedHack/releases/tag/v0.2.1) 阅读该版本 README。

## 问题排查

### 如何判断当前游戏是否使用支持的 dll？

可以使用微软官方的 [Process Monitor](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon) 工具来检查。

1.  运行 `Procmon64.exe`。
2.  启动您的游戏，并确保游戏已经播放了一段音频。
3.  切换到 Process Monitor 窗口，点击工具栏上的“漏斗”图标 (Filter) 打开筛选器。
4.  添加筛选规则：
    - `Process Name` is `你的游戏 exe`
    - `Path` contains `dsound`
    - `Path` contains `mmdevapi`
5.  查看结果列表。如果能找到匹配的条目，则说明此工具很可能适用。

### 打开游戏没有听到任何声音

1. 尝试使用 2.0 倍速的特定 DLL，例如只使用 MMDevAPI，而不是 ALL。
2. 目前 V1 版本的 MMDevAPI 确实存在 bug；请尝试使用 V0。
3. 提出 issue。

### 我使用特定 speed 时遇到一些问题

并非所有速度都能够正常工作，有可能是 dll 内部限制（dsound 无法在 2.0 倍速以上工作 #2 ），或者是 dll wrapper 的 bug（V0 MMDevAPI 2.3 倍速无声 #5 ）。请优先尝试 2.0 倍速以确认加速是否有效。若 2.0 倍速有效而某些其他倍速有问题，请提出 issue。

## TODO

本工具还处于极为原始的阶段，欢迎任何形式的贡献（Issue/PR）。

- [ ] [issue 区](https://github.com/lxl66566/AudioSpeedHack/issues)
- [ ] 支持其他音频 API
  - [x] MMDevAPI
  - [ ] xaudio2
  - [ ] winmm
- [x] **音质改善**
- [ ] 更好的 TUI 界面，或 GUI

## License

AudioSpeedHack 的 Rust 本体遵循 MIT 协议。

V0 源码中的 dsound.dll 来自 [dsoal fork](https://github.com/lxl66566/dsoal)，继承 GPLv2。V0 的 MMDevAPI.dll 允许分发与商用。

V1 源码中的 DLL 文件，除 SoundTouch 外均禁止未授权的商用。
