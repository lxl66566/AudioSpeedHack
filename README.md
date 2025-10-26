# AudioSpeedHack

一个实用工具，用于**加速所有基于 dsound.dll 的游戏音频**。

## 用法

1. 安装 [VB-CABLE](https://vb-audio.com/Cable/)：下载并解压，执行 VBCABLE_Setup_x64.exe，并点击 `Install Driver`。
2. 从 [Github Releases](https://github.com/lxl66566/AudioSpeedHack/releases) 下载最新的压缩包，解压后放到游戏所在目录。
3. 选择音频输出设备为 _CABLE Input (VB-Audio Virtual Cable)_。
4. 双击执行进入 TUI 模式，选择 _解压并启动_：
   1. 平台一般选择 win32 即可。
   2. 输入设备选择 _CABLE Output (VB-Audio Virtual Cable)_。
   3. 输出设备选择你的实际音频输出设备。
   4. 速度设为你想要的加速倍率。
   5. 执行程序选择你的游戏 exe 文件。
   6. 点击 _确认！_ 启动。
   - 或者在命令行中执行 `AudioSpeedHack -h` 查看命令行用法。

## 原理

本项目的音频加速本质上是让程序加载修改后的 DLL，对音频**先加速升调，再降调**得到的。

1.  **音频拦截与加速**：本工具内置了 [dsoal](https://github.com/lxl66566/dsoal) 修改而来的 `dsound.dll` 文件。启动时，工具根据选择的加速倍率，将一些 DLL 释放到游戏根目录。当游戏运行时，它会加载自定义的 `dsound.dll` 而非系统默认 DLL。此 DLL 会强制加速音频缓冲区处理，从而提高音频播放速度，但副作用是音调也会随之升高。
2.  **音高实时校正**：为了解决音调升高的问题，游戏的高音调音频会通过 [VB-CABLE Virtual Audio Device](https://vb-audio.com/Cable/) 输出。AudioSpeedHack 主程序会捕获来自虚拟声卡的音频流，对其进行实时的音高修正（降调），最后将正常音高、加速后的音频输出到播放设备上。

## 问题排查

### 如何判断当前游戏是否使用 dsound.dll？

可以使用微软官方的 [Process Monitor](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon) 工具来检查。

1.  运行 `Procmon64.exe`。
2.  启动您的游戏，并确保游戏已经播放了一段音频。
3.  切换到 Process Monitor 窗口，点击工具栏上的“漏斗”图标 (Filter) 打开筛选器。
4.  添加筛选规则：
    - `Path` contains `dsound.dll`
5.  查看结果列表。如果能找到匹配的条目，并且 Process Name 是游戏相关进程，其 Path 指向的是游戏目录下的 `dsound.dll`，则证明此工具适用。

## TODO

本工具还处于极为原始的阶段，欢迎任何形式的贡献（Issue/PR）。

- [ ] **支持 xaudio2 与其他音频 API**
- [ ] 音质改善
- [ ] 傻瓜式判断游戏是否使用 dsound.dll
- [ ] 注入而非预编译，或者减小 dll 体积
- [ ] 更好的 TUI 界面，或 GUI

## Tested On

Windows 11

- 春音 Alice＊Gram
- 白恋 Sakura＊Gram
- Deep One -ディープワン
