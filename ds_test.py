import atexit
import ctypes
import os

import soundfile as sf

# --- 请将这里的文件路径替换为您自己的 OGG 文件路径 ---
audio_file_path = "beep.ogg"

# --- 检查文件是否存在 ---
if not os.path.exists(audio_file_path):
    print(f"错误：找不到音频文件 '{audio_file_path}'")
    exit()

# --- 第 1 步: 使用 soundfile 解码 OGG 文件为 PCM 数据 ---
try:
    data, samplerate = sf.read(audio_file_path, dtype="int16")
    print(f"音频文件解码成功: 采样率={samplerate}, 时长={len(data) / samplerate:.2f}秒")
except Exception as e:
    print(f"解码 OGG 文件时出错: {e}")
    exit()

# 获取音频参数
channels = data.shape[1] if len(data.shape) > 1 else 1
bits_per_sample = data.dtype.itemsize * 8
block_align = channels * (bits_per_sample // 8)
bytes_per_second = samplerate * block_align
buffer_size = len(data.tobytes())

# --- 第 2 步: 使用 ctypes 定义 DirectSound 需要的 C 结构体和常量 ---

# 定义 Windows 数据类型
DWORD = ctypes.c_ulong
LONG = ctypes.c_long
USHORT = ctypes.c_ushort
HWND = ctypes.c_void_p
LPVOID = ctypes.c_void_p
HRESULT = LONG
WINFUNCTYPE = ctypes.WINFUNCTYPE


class GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", DWORD),
        ("Data2", USHORT),
        ("Data3", USHORT),
        ("Data4", ctypes.c_ubyte * 8),
    ]


class WAVEFORMATEX(ctypes.Structure):
    _fields_ = [
        ("wFormatTag", USHORT),
        ("nChannels", USHORT),
        ("nSamplesPerSec", DWORD),
        ("nAvgBytesPerSec", DWORD),
        ("nBlockAlign", USHORT),
        ("wBitsPerSample", USHORT),
        ("cbSize", USHORT),
    ]


class DSBUFFERDESC(ctypes.Structure):
    _fields_ = [
        ("dwSize", DWORD),
        ("dwFlags", DWORD),
        ("dwBufferBytes", DWORD),
        ("dwReserved", DWORD),
        ("lpwfxFormat", ctypes.POINTER(WAVEFORMATEX)),
        ("guid3DAlgorithm", GUID),
    ]


# 加载 dsound.dll
dsound_dll = ctypes.windll.dsound

# 定义函数原型
DirectSoundCreate8 = dsound_dll.DirectSoundCreate8
DirectSoundCreate8.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_void_p),
    ctypes.c_void_p,
]
DirectSoundCreate8.restype = HRESULT

# DirectSound 常量
DS_OK = 0
DSSCL_PRIORITY = 0x00000002
DSBCAPS_GLOBALFOCUS = 0x00008000
DSBPLAY_DEFAULT = 0

# --- 第 3 步: 初始化 DirectSound 并创建声音缓冲区 ---

# 1. 创建 DirectSound 主对象
p_dsound = ctypes.c_void_p()
hr = DirectSoundCreate8(None, ctypes.byref(p_dsound), None)
if hr != DS_OK:
    raise RuntimeError(f"DirectSoundCreate8 失败，错误码: {hr}")

# 2. 【修正】获取 IDirectSound8 vtable
# p_dsound 是指向对象的指针。对象的第一项是指向vtable的指针。
# 第一次 cast/contents 获取 vtable 的地址 (一个 c_void_p)
vtable_addr = ctypes.cast(p_dsound, ctypes.POINTER(ctypes.c_void_p)).contents
# 第二次 cast/contents 将该地址转换为一个可下标访问的指针数组
dsound_vtable = ctypes.cast(vtable_addr, ctypes.POINTER(ctypes.c_void_p * 20)).contents

# 3. 【修正】设置协作级别 (IDirectSound::SetCooperativeLevel, vtable 索引 6)
SetCooperativeLevel = WINFUNCTYPE(HRESULT, ctypes.c_void_p, HWND, DWORD)(
    dsound_vtable[6]
)
hwnd = ctypes.windll.kernel32.GetConsoleWindow()
hr = SetCooperativeLevel(p_dsound, hwnd, DSSCL_PRIORITY)
if hr != DS_OK:
    raise RuntimeError(f"SetCooperativeLevel 失败，错误码: {hr}")

# 4. 填充 WAVEFORMATEX
wfx = WAVEFORMATEX(
    1, channels, samplerate, bytes_per_second, block_align, bits_per_sample, 0
)

# 5. 填充 DSBUFFERDESC
dsbd = DSBUFFERDESC(
    ctypes.sizeof(DSBUFFERDESC),
    DSBCAPS_GLOBALFOCUS,
    buffer_size,
    0,
    ctypes.pointer(wfx),
    GUID(),
)

# 6. 【修正】创建声音缓冲区 (IDirectSound::CreateSoundBuffer, vtable 索引 3)
CreateSoundBuffer = WINFUNCTYPE(
    HRESULT,
    ctypes.c_void_p,
    ctypes.POINTER(DSBUFFERDESC),
    ctypes.POINTER(ctypes.c_void_p),
    ctypes.c_void_p,
)(dsound_vtable[3])
p_buffer = ctypes.c_void_p()
hr = CreateSoundBuffer(p_dsound, ctypes.byref(dsbd), ctypes.byref(p_buffer), None)
if hr != DS_OK:
    raise RuntimeError(f"CreateSoundBuffer 失败，错误码: {hr}")

# 7. 【修正】获取 IDirectSoundBuffer8 vtable (使用与上面相同的双重转换方法)
buffer_vtable_addr = ctypes.cast(p_buffer, ctypes.POINTER(ctypes.c_void_p)).contents
buffer_vtable = ctypes.cast(
    buffer_vtable_addr, ctypes.POINTER(ctypes.c_void_p * 25)
).contents

# --- 第 4 步: 将 PCM 数据写入缓冲区 ---

# 8. 【修正】锁定缓冲区 (IDirectSoundBuffer::Lock, vtable 索引 11)
Lock = WINFUNCTYPE(
    HRESULT,
    ctypes.c_void_p,
    DWORD,
    DWORD,
    ctypes.POINTER(LPVOID),
    ctypes.POINTER(DWORD),
    ctypes.POINTER(LPVOID),
    ctypes.POINTER(DWORD),
    DWORD,
)(buffer_vtable[11])
audio_ptr1 = LPVOID()
audio_bytes1 = DWORD()
hr = Lock(
    p_buffer,
    0,
    buffer_size,
    ctypes.byref(audio_ptr1),
    ctypes.byref(audio_bytes1),
    None,
    None,
    0,
)
if hr != DS_OK:
    raise RuntimeError(f"锁定缓冲区失败，错误码: {hr}")

# 9. 将 PCM 数据复制到缓冲区
pcm_data_bytes = data.tobytes()
ctypes.memmove(audio_ptr1, pcm_data_bytes, buffer_size)

# 10. 【修正】解锁缓冲区 (IDirectSoundBuffer::Unlock, vtable 索引 19)
Unlock = WINFUNCTYPE(HRESULT, ctypes.c_void_p, LPVOID, DWORD, LPVOID, DWORD)(
    buffer_vtable[19]
)
hr = Unlock(p_buffer, audio_ptr1, audio_bytes1, None, 0)
if hr != DS_OK:
    raise RuntimeError(f"解锁缓冲区失败，错误码: {hr}")

print("\nDirectSound 初始化完成，缓冲区已加载数据。")
print("按回车键播放音频，输入 'q' 或 'quit' 并按回车键退出。")

# --- 第 5 步: 定义播放和清理函数 ---

# 【修正】获取正确的函数指针
# HRESULT SetCurrentPosition(DWORD dwNewPosition); (vtable 索引 13)
SetCurrentPosition = WINFUNCTYPE(HRESULT, ctypes.c_void_p, DWORD)(buffer_vtable[13])
# HRESULT Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags); (vtable 索引 12)
Play = WINFUNCTYPE(HRESULT, ctypes.c_void_p, DWORD, DWORD, DWORD)(buffer_vtable[12])


def play_sound():
    SetCurrentPosition(p_buffer, 0)
    Play(p_buffer, 0, 0, DSBPLAY_DEFAULT)


# 获取 Release 函数 (IUnknown::Release, vtable 索引 2)
ReleaseBuffer = WINFUNCTYPE(HRESULT, ctypes.c_void_p)(buffer_vtable[2])
ReleaseDSound = WINFUNCTYPE(HRESULT, ctypes.c_void_p)(dsound_vtable[2])


def cleanup():
    print("\n正在清理资源...")
    # 必须检查指针是否有效再释放
    if p_buffer and p_buffer.value:
        ReleaseBuffer(p_buffer)
    if p_dsound and p_dsound.value:
        ReleaseDSound(p_dsound)
    print("清理完毕。")


atexit.register(cleanup)

# --- 主循环 ---
while True:
    try:
        user_input = input()
        if user_input.lower() in ["q", "quit"]:
            break
        play_sound()
    except (EOFError, KeyboardInterrupt):
        break
