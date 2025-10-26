# AudioSpeedHack

一个实用小工具，**用于加速所有基于 dsound.dll 的游戏音频**。

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

1. 修改 [dsoal](https://github.com/lxl66566/dsoal) 项目代码，强制将 frequency 调整为 1.0 到 2.5 倍的自定义值，并为每个频率编译一个 dll，打包进此工具内。在执行“解压 DLL”选项后，工具将必要的 dll 解压到当前目录下。启动游戏，加载这些 dll 后，所有音频都会被加速 + 升调。
2. 然后，游戏音频会通过 VB-CABLE 输出到本工具的音频处理程序，该音频处理将升调后的音频还原到原始音高，并输出处理后的音频到播放设备上。

## 排查问题

### 如何判断当前游戏是否使用 dsound.dll？

可以下载一个 [Process Monitor](https://learn.microsoft.com/en-us/sysinternals/downloads/procmon)。

1. 运行 Procmon64.exe。
2. 启动你的游戏，并且让其播放音频。
3. 返回 Process Monitor，点击漏斗图标打开 Filter，筛选 Process Name 为你的游戏名称、Path contains `dsound.dll`。
4. 查看列表中是否有匹配的结果，Path 是否是游戏文件下的 `dsound.dll`。

## TODO

本工具还处于极为原始的阶段，欢迎 PR。

- [ ] **支持 xaudio2 与其他音频 API**
- [ ] 傻瓜式判断游戏是否使用 dsound.dll
- [ ] 注入而非预编译，或者减小 dll 体积
- [ ] 更好的 TUI 界面或 GUI
