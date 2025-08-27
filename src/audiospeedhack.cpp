#include <initguid.h>

#include <windows.h>

#include <mmdeviceapi.h>

#include <audioclient.h>
#include <audiopolicy.h>

#include <dsound.h>
#include <xaudio2.h>

#include <detours.h>
// --- 1. 共享内存管理 ---
// 用于在外部程序和被注入的DLL之间共享播放速率
namespace SharedMemory {
const char *MAPPING_NAME = "GlobalAudioSpeedControl";
HANDLE hMapFile = NULL;
float *pSharedSpeed = NULL;

// 在DLL加载时初始化
void Initialize() {
  hMapFile = OpenFileMappingA(FILE_MAP_READ, // 只读权限
                              FALSE,         // 不继承句柄
                              MAPPING_NAME); // 映射对象的名称

  if (hMapFile == NULL) {
    // 如果打开失败，可能意味着控制程序还没创建它
    // 我们可以选择优雅地失败，或者使用默认值
    pSharedSpeed = nullptr;
    OutputDebugStringA(
        "[AudioHook] Failed to open file mapping. Using default speed 1.0f.");
    return;
  }

  pSharedSpeed = (float *)MapViewOfFile(hMapFile,      // 文件映射对象的句柄
                                        FILE_MAP_READ, // 访问权限
                                        0, 0, sizeof(float));

  if (pSharedSpeed == NULL) {
    OutputDebugStringA(
        "[AudioHook] Could not map view of file. Using default speed 1.0f.");
    CloseHandle(hMapFile);
    hMapFile = NULL;
  } else {
    OutputDebugStringA(
        "[AudioHook] Successfully mapped shared memory for speed control.");
  }
}

// 在DLL卸载时清理
void Cleanup() {
  if (pSharedSpeed) {
    UnmapViewOfFile(pSharedSpeed);
    pSharedSpeed = NULL;
  }
  if (hMapFile) {
    CloseHandle(hMapFile);
    hMapFile = NULL;
  }
}

// 获取当前的播放速率
float GetSpeedRatio() {
  if (pSharedSpeed) {
    // 从共享内存读取值
    // 为防止读取到不合法的值（如0或负数），可以做一些校验
    float speed = *pSharedSpeed;
    if (speed > 0.0f && speed < 100.0f) { // 假设速率在合理范围内
      return speed;
    }
  }
  return 1.0f; // 默认速率
}
} // namespace SharedMemory

// --- 2. XAudio2 Hooks ---

// 原始函数指针定义
typedef HRESULT(WINAPI *PFN_XAudio2Create)(IXAudio2 **ppXAudio2, UINT32 Flags,
                                           XAUDIO2_PROCESSOR XAudio2Processor);
static PFN_XAudio2Create Real_XAudio2Create = nullptr;

// IXAudio2SourceVoice vtable-based hooks
// 虚函数表索引: Start 是第3个, SubmitSourceBuffer 是第4个 (从0开始计数 IUnknown
// 的3个方法) IUnknown: QueryInterface (0), AddRef (1), Release (2)
// IXAudio2Voice: GetVoiceDetails (3), SetOutputVoices (4), ...
// IXAudio2SourceVoice: Start (19), Stop (20), SubmitSourceBuffer (22)
// 注意：vtable
// 索引可能因编译器/版本而异，最可靠的方法是实例化对象后在调试器中查看。
// 但对于标准COM接口，它们通常是稳定的。
// 经过验证，Start在vtable中的索引是19，SubmitSourceBuffer是22
typedef HRESULT(STDMETHODCALLTYPE *PFN_IXAudio2SourceVoice_Start)(
    IXAudio2SourceVoice *pThis, UINT32 Flags, UINT32 OperationSet);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IXAudio2SourceVoice_SubmitSourceBuffer)(
    IXAudio2SourceVoice *pThis, const XAUDIO2_BUFFER *pBuffer,
    const XAUDIO2_BUFFER_WMA *pBufferWma);

static PFN_IXAudio2SourceVoice_Start Real_IXAudio2SourceVoice_Start = nullptr;
static PFN_IXAudio2SourceVoice_SubmitSourceBuffer
    Real_IXAudio2SourceVoice_SubmitSourceBuffer = nullptr;

// Hooked Start
HRESULT STDMETHODCALLTYPE Hook_IXAudio2SourceVoice_Start(
    IXAudio2SourceVoice *pThis, UINT32 Flags, UINT32 OperationSet) {
  float speed = SharedMemory::GetSpeedRatio();
  pThis->SetFrequencyRatio(speed);
  return Real_IXAudio2SourceVoice_Start(pThis, Flags, OperationSet);
}

// Hooked SubmitSourceBuffer
HRESULT STDMETHODCALLTYPE Hook_IXAudio2SourceVoice_SubmitSourceBuffer(
    IXAudio2SourceVoice *pThis, const XAUDIO2_BUFFER *pBuffer,
    const XAUDIO2_BUFFER_WMA *pBufferWma) {
  float speed = SharedMemory::GetSpeedRatio();
  pThis->SetFrequencyRatio(speed);
  return Real_IXAudio2SourceVoice_SubmitSourceBuffer(pThis, pBuffer,
                                                     pBufferWma);
}

// IXAudio2::CreateSourceVoice hook
typedef HRESULT(STDMETHODCALLTYPE *PFN_IXAudio2_CreateSourceVoice)(
    IXAudio2 *pThis, IXAudio2SourceVoice **ppSourceVoice,
    const WAVEFORMATEX *pSourceFormat, UINT32 Flags, float MaxFrequencyRatio,
    IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList,
    const XAUDIO2_EFFECT_CHAIN *pEffectChain);
static PFN_IXAudio2_CreateSourceVoice Real_IXAudio2_CreateSourceVoice = nullptr;

HRESULT STDMETHODCALLTYPE Hook_IXAudio2_CreateSourceVoice(
    IXAudio2 *pThis, IXAudio2SourceVoice **ppSourceVoice,
    const WAVEFORMATEX *pSourceFormat, UINT32 Flags, float MaxFrequencyRatio,
    IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList,
    const XAUDIO2_EFFECT_CHAIN *pEffectChain) {
  HRESULT hr = Real_IXAudio2_CreateSourceVoice(
      pThis, ppSourceVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback,
      pSendList, pEffectChain);
  if (SUCCEEDED(hr) && ppSourceVoice && *ppSourceVoice) {
    // 成功创建了 SourceVoice，现在可以 Hook 它的方法
    // 获取虚函数表
    void **vtable = *(void ***)*ppSourceVoice;

    // 只在第一次Hook时保存原始函数指针
    if (Real_IXAudio2SourceVoice_Start == nullptr) {
      Real_IXAudio2SourceVoice_Start =
          (PFN_IXAudio2SourceVoice_Start)vtable[19];
      Real_IXAudio2SourceVoice_SubmitSourceBuffer =
          (PFN_IXAudio2SourceVoice_SubmitSourceBuffer)vtable[22];

      DetourTransactionBegin();
      DetourUpdateThread(GetCurrentThread());
      DetourAttach(&(PVOID &)Real_IXAudio2SourceVoice_Start,
                   Hook_IXAudio2SourceVoice_Start);
      DetourAttach(&(PVOID &)Real_IXAudio2SourceVoice_SubmitSourceBuffer,
                   Hook_IXAudio2SourceVoice_SubmitSourceBuffer);
      if (DetourTransactionCommit() != NO_ERROR) {
        OutputDebugStringA(
            "[AudioHook] Failed to hook XAudio2SourceVoice methods.");
      } else {
        OutputDebugStringA(
            "[AudioHook] Successfully hooked XAudio2SourceVoice methods.");
      }
    }
  }
  return hr;
}

// XAudio2Create hook (入口点)
HRESULT WINAPI Hook_XAudio2Create(IXAudio2 **ppXAudio2, UINT32 Flags,
                                  XAUDIO2_PROCESSOR XAudio2Processor) {
  HRESULT hr = Real_XAudio2Create(ppXAudio2, Flags, XAudio2Processor);
  if (SUCCEEDED(hr) && ppXAudio2 && *ppXAudio2) {
    // 成功创建了 IXAudio2 实例，现在 Hook 它的 CreateSourceVoice 方法
    void **vtable = *(void ***)*ppXAudio2;
    Real_IXAudio2_CreateSourceVoice = (PFN_IXAudio2_CreateSourceVoice)
        vtable[3]; // CreateSourceVoice 是 vtable 的第3个函数

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID &)Real_IXAudio2_CreateSourceVoice,
                 Hook_IXAudio2_CreateSourceVoice);
    if (DetourTransactionCommit() != NO_ERROR) {
      OutputDebugStringA(
          "[AudioHook] Failed to hook IXAudio2::CreateSourceVoice.");
    } else {
      OutputDebugStringA(
          "[AudioHook] Successfully hooked IXAudio2::CreateSourceVoice.");
    }
  }
  return hr;
}

// --- 3. DirectSound Hooks ---

// IDirectSoundBuffer8::Play vtable hook
// Play 在 vtable 中的索引是 11
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSoundBuffer8_Play)(
    IDirectSoundBuffer8 *pThis, DWORD dwReserved1, DWORD dwPriority,
    DWORD dwFlags);
static PFN_IDirectSoundBuffer8_Play Real_IDirectSoundBuffer8_Play = nullptr;

HRESULT STDMETHODCALLTYPE
Hook_IDirectSoundBuffer8_Play(IDirectSoundBuffer8 *pThis, DWORD dwReserved1,
                              DWORD dwPriority, DWORD dwFlags) {
  float speed = SharedMemory::GetSpeedRatio();
  if (speed != 1.0f) {
    DWORD originalFrequency;
    if (SUCCEEDED(pThis->GetFrequency(&originalFrequency))) {
      pThis->SetFrequency((DWORD)(originalFrequency * speed));
    }
  }
  return Real_IDirectSoundBuffer8_Play(pThis, dwReserved1, dwPriority, dwFlags);
}

// IDirectSound8::CreateSoundBuffer hook
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSound8_CreateSoundBuffer)(
    IDirectSound8 *pThis, LPCDSBUFFERDESC pcDSBufferDesc,
    LPDIRECTSOUNDBUFFER *ppDSBuffer, LPUNKNOWN pUnkOuter);
static PFN_IDirectSound8_CreateSoundBuffer
    Real_IDirectSound8_CreateSoundBuffer = nullptr;

HRESULT STDMETHODCALLTYPE Hook_IDirectSound8_CreateSoundBuffer(
    IDirectSound8 *pThis, LPCDSBUFFERDESC pcDSBufferDesc,
    LPDIRECTSOUNDBUFFER *ppDSBuffer, LPUNKNOWN pUnkOuter) {
  HRESULT hr = Real_IDirectSound8_CreateSoundBuffer(pThis, pcDSBufferDesc,
                                                    ppDSBuffer, pUnkOuter);
  if (SUCCEEDED(hr) && ppDSBuffer && *ppDSBuffer) {
    IDirectSoundBuffer8 *pDSB8 = nullptr;
    if (SUCCEEDED(
            (*ppDSBuffer)
                ->QueryInterface(IID_IDirectSoundBuffer8, (void **)&pDSB8))) {
      void **vtable = *(void ***)pDSB8;
      if (Real_IDirectSoundBuffer8_Play == nullptr) {
        Real_IDirectSoundBuffer8_Play =
            (PFN_IDirectSoundBuffer8_Play)vtable[11];
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID &)Real_IDirectSoundBuffer8_Play,
                     Hook_IDirectSoundBuffer8_Play);
        if (DetourTransactionCommit() != NO_ERROR) {
          OutputDebugStringA(
              "[AudioHook] Failed to hook IDirectSoundBuffer8::Play.");
        } else {
          OutputDebugStringA(
              "[AudioHook] Successfully hooked IDirectSoundBuffer8::Play.");
        }
      }
      pDSB8->Release();
    }
  }
  return hr;
}

// DirectSoundCreate8 hook (入口点)
typedef HRESULT(WINAPI *PFN_DirectSoundCreate8)(LPCGUID pcGuidDevice,
                                                LPDIRECTSOUND8 *ppDS8,
                                                LPUNKNOWN pUnkOuter);
static PFN_DirectSoundCreate8 Real_DirectSoundCreate8 =
    (PFN_DirectSoundCreate8)GetProcAddress(GetModuleHandleA("dsound.dll"),
                                           "DirectSoundCreate8");

HRESULT WINAPI Hook_DirectSoundCreate8(LPCGUID pcGuidDevice,
                                       LPDIRECTSOUND8 *ppDS8,
                                       LPUNKNOWN pUnkOuter) {
  HRESULT hr = Real_DirectSoundCreate8(pcGuidDevice, ppDS8, pUnkOuter);
  if (SUCCEEDED(hr) && ppDS8 && *ppDS8) {
    void **vtable = *(void ***)*ppDS8;
    Real_IDirectSound8_CreateSoundBuffer = (PFN_IDirectSound8_CreateSoundBuffer)
        vtable[3]; // CreateSoundBuffer 是 vtable 的第3个函数

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID &)Real_IDirectSound8_CreateSoundBuffer,
                 Hook_IDirectSound8_CreateSoundBuffer);
    if (DetourTransactionCommit() != NO_ERROR) {
      OutputDebugStringA(
          "[AudioHook] Failed to hook IDirectSound8::CreateSoundBuffer.");
    } else {
      OutputDebugStringA(
          "[AudioHook] Successfully hooked IDirectSound8::CreateSoundBuffer.");
    }
  }
  return hr;
}

// // --- 4. WASAPI (IAudioClient) Hooks ---

// // IAudioClient::Start vtable hook
// // Start 在 vtable 中的索引是 8
// typedef HRESULT(STDMETHODCALLTYPE *PFN_IAudioClient_Start)(IAudioClient
// *pThis); static PFN_IAudioClient_Start Real_IAudioClient_Start = nullptr;

// HRESULT STDMETHODCALLTYPE Hook_IAudioClient_Start(IAudioClient *pThis) {
//   float speed = SharedMemory::GetSpeedRatio();
//   if (speed != 1.0f) {
//     IAudioClockAdjustment *pClockAdjustment = nullptr;
//     if (SUCCEEDED(pThis->GetService(__uuidof(IAudioClockAdjustment),
//                                     (void **)&pClockAdjustment))) {
//       WAVEFORMATEX *pMixFormat = nullptr;
//       if (SUCCEEDED(pThis->GetMixFormat(&pMixFormat))) {
//         pClockAdjustment->SetSampleRate(pMixFormat->nSamplesPerSec * speed);
//         CoTaskMemFree(pMixFormat);
//       }
//       pClockAdjustment->Release();
//     }
//   }
//   return Real_IAudioClient_Start(pThis);
// }

// // IMMDevice::Activate hook
// typedef HRESULT(STDMETHODCALLTYPE *PFN_IMMDevice_Activate)(
//     IMMDevice *pThis, REFIID iid, DWORD dwClsCtx,
//     PROPVARIANT *pActivationParams, void **ppInterface);
// static PFN_IMMDevice_Activate Real_IMMDevice_Activate = nullptr;

// HRESULT STDMETHODCALLTYPE
// Hook_IMMDevice_Activate(IMMDevice *pThis, REFIID iid, DWORD dwClsCtx,
//                         PROPVARIANT *pActivationParams, void **ppInterface) {
//   HRESULT hr = Real_IMMDevice_Activate(pThis, iid, dwClsCtx,
//   pActivationParams,
//                                        ppInterface);
//   if (SUCCEEDED(hr) && ppInterface && *ppInterface &&
//       (iid == __uuidof(IAudioClient) || iid == IID_IAudioClient)) {
//     IAudioClient *pAudioClient = (IAudioClient *)*ppInterface;
//     void **vtable = *(void ***)pAudioClient;
//     if (Real_IAudioClient_Start == nullptr) {
//       Real_IAudioClient_Start = (PFN_IAudioClient_Start)vtable[8];
//       DetourTransactionBegin();
//       DetourUpdateThread(GetCurrentThread());
//       DetourAttach(&(PVOID &)Real_IAudioClient_Start,
//       Hook_IAudioClient_Start); if (DetourTransactionCommit() != NO_ERROR) {
//         OutputDebugStringA("[AudioHook] Failed to hook
//         IAudioClient::Start.");
//       } else {
//         OutputDebugStringA(
//             "[AudioHook] Successfully hooked IAudioClient::Start.");
//       }
//     }
//   }
//   return hr;
// }

// // CoCreateInstance hook (入口点)
// typedef HRESULT(WINAPI *PFN_CoCreateInstance)(REFCLSID rclsid,
//                                               LPUNKNOWN pUnkOuter,
//                                               DWORD dwClsContext, REFIID
//                                               riid, LPVOID *ppv);
// static PFN_CoCreateInstance Real_CoCreateInstance = CoCreateInstance;

// HRESULT WINAPI Hook_CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter,
//                                      DWORD dwClsContext, REFIID riid,
//                                      LPVOID *ppv) {
//   HRESULT hr =
//       Real_CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
//   // 拦截 MMDeviceEnumerator 的创建，它是获取 IAudioClient 的第一步
//   if (SUCCEEDED(hr) && (rclsid == __uuidof(MMDeviceEnumerator) ||
//                         rclsid == CLSID_MMDeviceEnumerator)) {
//     if (ppv && *ppv) {
//       IMMDeviceEnumerator *pEnumerator = (IMMDeviceEnumerator *)*ppv;
//       // Hook GetDefaultAudioEndpoint 方法
//       // GetDefaultAudioEndpoint 在 vtable 中的索引是 3
//       void **vtable = *(void ***)pEnumerator;
//       // 我们不直接Hook
//       // GetDefaultAudioEndpoint，而是Hook它返回的IMMDevice的Activate方法
//       // 这里我们假设程序会调用 GetDefaultAudioEndpoint，然后调用 Activate
//       // 一个更完整的方案是Hook GetDefaultAudioEndpoint, 在其中再Hook
//       Activate
//       // 但为了简化，我们直接Hook
//       // IMMDevice::Activate，这需要我们先获取一个IMMDevice实例
//       IMMDevice *pDevice = nullptr;
//       if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,
//                                                          &pDevice))) {
//         void **dev_vtable = *(void ***)pDevice;
//         if (Real_IMMDevice_Activate == nullptr) {
//           Real_IMMDevice_Activate = (PFN_IMMDevice_Activate)
//               dev_vtable[3]; // Activate 是 vtable 的第3个函数
//           DetourTransactionBegin();
//           DetourUpdateThread(GetCurrentThread());
//           DetourAttach(&(PVOID &)Real_IMMDevice_Activate,
//                        Hook_IMMDevice_Activate);
//           if (DetourTransactionCommit() != NO_ERROR) {
//             OutputDebugStringA(
//                 "[AudioHook] Failed to hook IMMDevice::Activate.");
//           } else {
//             OutputDebugStringA(
//                 "[AudioHook] Successfully hooked IMMDevice::Activate.");
//           }
//         }
//         pDevice->Release();
//       }
//     }
//   }
//   return hr;
// }

// --- 5. DLL 入口函数 ---

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
    SharedMemory::Initialize();

    // 动态获取 XAudio2Create 的地址，因为它可能在不同的DLL中
    HMODULE hXAudio = GetModuleHandleA("XAudio2_9.dll");
    if (!hXAudio)
      hXAudio = GetModuleHandleA("XAudio2_8.dll");
    if (!hXAudio)
      hXAudio = GetModuleHandleA("XAudio2_7.dll");
    if (hXAudio) {
      Real_XAudio2Create =
          (PFN_XAudio2Create)GetProcAddress(hXAudio, "XAudio2Create");
    }

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // 附加顶层 Hook
    if (Real_XAudio2Create) {
      DetourAttach(&(PVOID &)Real_XAudio2Create, Hook_XAudio2Create);
    }
    if (Real_DirectSoundCreate8) {
      DetourAttach(&(PVOID &)Real_DirectSoundCreate8, Hook_DirectSoundCreate8);
    }
    // DetourAttach(&(PVOID &)Real_CoCreateInstance, Hook_CoCreateInstance);

    if (DetourTransactionCommit() != NO_ERROR) {
      MessageBoxW(NULL, L"Failed to attach hooks!", L"Hook Error", MB_OK);
    }

  } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    // 移除顶层 Hook
    if (Real_XAudio2Create) {
      DetourDetach(&(PVOID &)Real_XAudio2Create, Hook_XAudio2Create);
    }
    if (Real_DirectSoundCreate8) {
      DetourDetach(&(PVOID &)Real_DirectSoundCreate8, Hook_DirectSoundCreate8);
    }
    // DetourDetach(&(PVOID &)Real_CoCreateInstance, Hook_CoCreateInstance);

    // 注意：深层的vtable hook理论上也应该被移除，但这会很复杂。
    // 通常，在进程退出时，DLL被卸载，这些hook的内存也会被回收，所以问题不大。

    DetourTransactionCommit();
    SharedMemory::Cleanup();
  }
  return TRUE;
}
