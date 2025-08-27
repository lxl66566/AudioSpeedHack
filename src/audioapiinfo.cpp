// This file is a debug utility for hooking audio APIs and logging their
// invocations.
#include <windows.h>

#include <audioclient.h>
#include <audiopolicy.h>
#include <detours.h>
#include <dsound.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>
#include <xaudio2.h>

#include <map>
#include <mutex>
#include <sstream>
#include <string>

// Helper function to format output strings
template <typename... Args>
std::string format_string(const std::string &format, Args... args) {
  size_t size =
      snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
  if (size <= 0) {
    return "Error formatting string";
  }
  std::unique_ptr<char[]> buf(new char[size]);
  snprintf(buf.get(), size, format.c_str(), args...);
  return std::string(buf.get(),
                     buf.get() + size - 1); // We don't want the '\0' inside
}

// Helper to print debug messages
void Log(const char *message) { OutputDebugStringA(message); }

// ===================================================================================
// 1. Core Audio (WASAPI) Hooks
// ===================================================================================

// --- Function Pointer Definitions ---
typedef HRESULT(STDMETHODCALLTYPE *PFN_IAudioClient_Initialize)(
    IAudioClient *pThis, AUDCLNT_SHAREMODE ShareMode, DWORD StreamFlags,
    REFERENCE_TIME hnsBufferDuration, REFERENCE_TIME hnsPeriodicity,
    const WAVEFORMATEX *pFormat, LPCGUID AudioSessionGuid);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IAudioClient_GetService)(
    IAudioClient *pThis, REFIID riid, void **ppv);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IAudioRenderClient_GetBuffer)(
    IAudioRenderClient *pThis, UINT32 NumFramesRequested, BYTE **ppData);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IAudioRenderClient_ReleaseBuffer)(
    IAudioRenderClient *pThis, UINT32 NumFramesWritten, DWORD dwFlags);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IMMDevice_Activate)(
    IMMDevice *pThis, REFIID iid, DWORD dwClsCtx,
    PROPVARIANT *pActivationParams, void **ppInterface);

// --- Original Function Pointers ---
static PFN_IAudioClient_Initialize Real_IAudioClient_Initialize = nullptr;
static PFN_IAudioClient_GetService Real_IAudioClient_GetService = nullptr;
static PFN_IAudioRenderClient_GetBuffer Real_IAudioRenderClient_GetBuffer =
    nullptr;
static PFN_IAudioRenderClient_ReleaseBuffer
    Real_IAudioRenderClient_ReleaseBuffer = nullptr;
static PFN_IMMDevice_Activate Real_IMMDevice_Activate = nullptr;

// --- Global State for WASAPI ---
static std::mutex g_wasapiMutex;
// Maps an IAudioRenderClient to its original GetBuffer/ReleaseBuffer functions
static std::map<IAudioRenderClient *, PFN_IAudioRenderClient_GetBuffer>
    g_renderClientGetBufferMap;
static std::map<IAudioRenderClient *, PFN_IAudioRenderClient_ReleaseBuffer>
    g_renderClientReleaseBufferMap;

// --- Hooked Functions ---

// HRESULT STDMETHODCALLTYPE Hook_IAudioRenderClient_ReleaseBuffer(
//     IAudioRenderClient *pThis, UINT32 NumFramesWritten, DWORD dwFlags) {
//   Log(format_string("[WASAPI] IAudioRenderClient::ReleaseBuffer -> "
//                     "NumFramesWritten: %u, Flags: 0x%X",
//                     NumFramesWritten, dwFlags)
//           .c_str());

//   std::lock_guard<std::mutex> lock(g_wasapiMutex);
//   auto it = g_renderClientReleaseBufferMap.find(pThis);
//   if (it != g_renderClientReleaseBufferMap.end()) {
//     return it->second(pThis, NumFramesWritten, dwFlags);
//   }
//   // Fallback in case something went wrong
//   return Real_IAudioRenderClient_ReleaseBuffer(pThis, NumFramesWritten,
//                                                dwFlags);
// }

// HRESULT STDMETHODCALLTYPE Hook_IAudioRenderClient_GetBuffer(
//     IAudioRenderClient *pThis, UINT32 NumFramesRequested, BYTE **ppData) {
//   Log(format_string(
//           "[WASAPI] IAudioRenderClient::GetBuffer -> NumFramesRequested: %u",
//           NumFramesRequested)
//           .c_str());

//   std::lock_guard<std::mutex> lock(g_wasapiMutex);
//   auto it = g_renderClientGetBufferMap.find(pThis);
//   if (it != g_renderClientGetBufferMap.end()) {
//     return it->second(pThis, NumFramesRequested, ppData);
//   }
//   // Fallback
//   return Real_IAudioRenderClient_GetBuffer(pThis, NumFramesRequested,
//   ppData);
// }

// HRESULT STDMETHODCALLTYPE Hook_IAudioClient_GetService(IAudioClient *pThis,
//                                                        REFIID riid,
//                                                        void **ppv) {
//   Log(format_string(
//           "[WASAPI] IAudioClient::GetService -> Requesting service (IID)...")
//           .c_str());

//   HRESULT hr = Real_IAudioClient_GetService(pThis, riid, ppv);

//   if (SUCCEEDED(hr) && riid == IID_IAudioRenderClient) {
//     Log("[WASAPI] IAudioClient::GetService -> IID_IAudioRenderClient
//     obtained. "
//         "Hooking its methods.");
//     IAudioRenderClient *pRenderClient = static_cast<IAudioRenderClient
//     *>(*ppv);

//     void **vtable = *(void ***)pRenderClient;
//     PFN_IAudioRenderClient_GetBuffer pfnGetBuffer =
//         (PFN_IAudioRenderClient_GetBuffer)vtable[3];
//     PFN_IAudioRenderClient_ReleaseBuffer pfnReleaseBuffer =
//         (PFN_IAudioRenderClient_ReleaseBuffer)vtable[4];

//     {
//       std::lock_guard<std::mutex> lock(g_wasapiMutex);
//       g_renderClientGetBufferMap[pRenderClient] = pfnGetBuffer;
//       g_renderClientReleaseBufferMap[pRenderClient] = pfnReleaseBuffer;
//     }

//     DetourTransactionBegin();
//     DetourUpdateThread(GetCurrentThread());
//     DetourAttach(&(PVOID &)pfnGetBuffer, Hook_IAudioRenderClient_GetBuffer);
//     DetourAttach(&(PVOID &)pfnReleaseBuffer,
//                  Hook_IAudioRenderClient_ReleaseBuffer);
//     DetourTransactionCommit();
//   }
//   return hr;
// }

// HRESULT STDMETHODCALLTYPE Hook_IAudioClient_Initialize(
//     IAudioClient *pThis, AUDCLNT_SHAREMODE ShareMode, DWORD StreamFlags,
//     REFERENCE_TIME hnsBufferDuration, REFERENCE_TIME hnsPeriodicity,
//     const WAVEFORMATEX *pFormat, LPCGUID AudioSessionGuid) {
//   Log(format_string(
//           "[WASAPI] IAudioClient::Initialize -> ShareMode: %d, Flags: 0x%X, "
//           "SampleRate: %lu, Channels: %u, BitsPerSample: %u",
//           ShareMode, StreamFlags, pFormat ? pFormat->nSamplesPerSec : 0,
//           pFormat ? pFormat->nChannels : 0,
//           pFormat ? pFormat->wBitsPerSample : 0)
//           .c_str());

//   return Real_IAudioClient_Initialize(pThis, ShareMode, StreamFlags,
//                                       hnsBufferDuration, hnsPeriodicity,
//                                       pFormat, AudioSessionGuid);
// }

// HRESULT STDMETHODCALLTYPE
// Hook_IMMDevice_Activate(IMMDevice *pThis, REFIID iid, DWORD dwClsCtx,
//                         PROPVARIANT *pActivationParams, void **ppInterface) {
//   Log(format_string(
//           "[WASAPI] IMMDevice::Activate -> Activating interface (IID)...")
//           .c_str());

//   HRESULT hr = Real_IMMDevice_Activate(pThis, iid, dwClsCtx,
//   pActivationParams,
//                                        ppInterface);

//   if (SUCCEEDED(hr) && iid == IID_IAudioClient) {
//     Log("[WASAPI] IMMDevice::Activate -> IID_IAudioClient obtained. Hooking "
//         "its methods.");
//     IAudioClient *pAudioClient = static_cast<IAudioClient *>(*ppInterface);

//     void **vtable = *(void ***)pAudioClient;
//     Real_IAudioClient_Initialize = (PFN_IAudioClient_Initialize)vtable[3];
//     Real_IAudioClient_GetService = (PFN_IAudioClient_GetService)vtable[8];

//     DetourTransactionBegin();
//     DetourUpdateThread(GetCurrentThread());
//     DetourAttach(&(PVOID &)Real_IAudioClient_Initialize,
//                  Hook_IAudioClient_Initialize);
//     DetourAttach(&(PVOID &)Real_IAudioClient_GetService,
//                  Hook_IAudioClient_GetService);
//     DetourTransactionCommit();
//   }
//   return hr;
// }

// ===================================================================================
// 2. XAudio2 Hooks (Logging Only)
// ===================================================================================

// --- Function Pointer Definitions ---
typedef HRESULT(STDMETHODCALLTYPE *PFN_IXAudio2SourceVoice_SubmitSourceBuffer)(
    IXAudio2SourceVoice *pThis, const XAUDIO2_BUFFER *pBuffer,
    const XAUDIO2_BUFFER_WMA *pBufferWma);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IXAudio2_CreateSourceVoice)(
    IXAudio2 *pThis, IXAudio2SourceVoice **ppSourceVoice,
    const WAVEFORMATEX *pSourceFormat, UINT32 Flags, float MaxFrequencyRatio,
    IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList,
    const XAUDIO2_EFFECT_CHAIN *pEffectChain);
typedef HRESULT(WINAPI *PFN_XAudio2Create)(IXAudio2 **ppXAudio2, UINT32 Flags,
                                           XAUDIO2_PROCESSOR XAudio2Processor);

// --- Original Function Pointers ---
static PFN_XAudio2Create Real_XAudio2Create = nullptr;
static PFN_IXAudio2_CreateSourceVoice Real_IXAudio2_CreateSourceVoice = nullptr;
static PFN_IXAudio2SourceVoice_SubmitSourceBuffer
    Real_IXAudio2SourceVoice_SubmitSourceBuffer = nullptr;

// --- Hooked Functions ---
HRESULT STDMETHODCALLTYPE Hook_IXAudio2SourceVoice_SubmitSourceBuffer(
    IXAudio2SourceVoice *pThis, const XAUDIO2_BUFFER *pBuffer,
    const XAUDIO2_BUFFER_WMA *pBufferWma) {
  Log(format_string("[XAudio2] IXAudio2SourceVoice::SubmitSourceBuffer -> "
                    "AudioBytes: %u, Flags: 0x%X, LoopCount: %u",
                    pBuffer ? pBuffer->AudioBytes : 0,
                    pBuffer ? pBuffer->Flags : 0,
                    pBuffer ? pBuffer->LoopCount : 0)
          .c_str());

  return Real_IXAudio2SourceVoice_SubmitSourceBuffer(pThis, pBuffer,
                                                     pBufferWma);
}

HRESULT STDMETHODCALLTYPE Hook_IXAudio2_CreateSourceVoice(
    IXAudio2 *pThis, IXAudio2SourceVoice **ppSourceVoice,
    const WAVEFORMATEX *pSourceFormat, UINT32 Flags, float MaxFrequencyRatio,
    IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList,
    const XAUDIO2_EFFECT_CHAIN *pEffectChain) {
  Log(format_string("[XAudio2] IXAudio2::CreateSourceVoice -> Flags: 0x%X, "
                    "SampleRate: %lu, Channels: %u, BitsPerSample: %u",
                    Flags, pSourceFormat ? pSourceFormat->nSamplesPerSec : 0,
                    pSourceFormat ? pSourceFormat->nChannels : 0,
                    pSourceFormat ? pSourceFormat->wBitsPerSample : 0)
          .c_str());

  HRESULT hr = Real_IXAudio2_CreateSourceVoice(
      pThis, ppSourceVoice, pSourceFormat, Flags, MaxFrequencyRatio, pCallback,
      pSendList, pEffectChain);

  if (SUCCEEDED(hr) && ppSourceVoice && *ppSourceVoice) {
    void **vtable = *(void ***)*ppSourceVoice;
    // Hook SubmitSourceBuffer for this specific voice instance
    if (Real_IXAudio2SourceVoice_SubmitSourceBuffer == nullptr) {
      Real_IXAudio2SourceVoice_SubmitSourceBuffer =
          (PFN_IXAudio2SourceVoice_SubmitSourceBuffer)vtable[9];
      DetourTransactionBegin();
      DetourUpdateThread(GetCurrentThread());
      DetourAttach(&(PVOID &)Real_IXAudio2SourceVoice_SubmitSourceBuffer,
                   Hook_IXAudio2SourceVoice_SubmitSourceBuffer);
      DetourTransactionCommit();
    }
  }
  return hr;
}

HRESULT WINAPI Hook_XAudio2Create(IXAudio2 **ppXAudio2, UINT32 Flags,
                                  XAUDIO2_PROCESSOR XAudio2Processor) {
  Log("[XAudio2] XAudio2Create called.");
  HRESULT hr = Real_XAudio2Create(ppXAudio2, Flags, XAudio2Processor);
  if (SUCCEEDED(hr) && ppXAudio2 && *ppXAudio2) {
    void **vtable = *(void ***)*ppXAudio2;
    if (Real_IXAudio2_CreateSourceVoice == nullptr) {
      Real_IXAudio2_CreateSourceVoice =
          (PFN_IXAudio2_CreateSourceVoice)vtable[3];
      DetourTransactionBegin();
      DetourUpdateThread(GetCurrentThread());
      DetourAttach(&(PVOID &)Real_IXAudio2_CreateSourceVoice,
                   Hook_IXAudio2_CreateSourceVoice);
      DetourTransactionCommit();
    }
  }
  return hr;
}

// ===================================================================================
// 3. DirectSound Hooks (Logging Only)
// ===================================================================================

// --- Function Pointer Definitions ---
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSoundBuffer_Lock)(
    IDirectSoundBuffer *pThis, DWORD dwOffset, DWORD dwBytes,
    LPVOID *ppvAudioPtr1, LPDWORD pdwAudioBytes1, LPVOID *ppvAudioPtr2,
    LPDWORD pdwAudioBytes2, DWORD dwFlags);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSoundBuffer_Play)(
    IDirectSoundBuffer *pThis, DWORD dwReserved1, DWORD dwPriority,
    DWORD dwFlags);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSound_CreateSoundBuffer)(
    IDirectSound *pThis, LPCDSBUFFERDESC pcDSBufferDesc,
    LPDIRECTSOUNDBUFFER *ppDSBuffer, LPUNKNOWN pUnkOuter);
typedef HRESULT(WINAPI *PFN_DirectSoundCreate)(LPCGUID pcGuidDevice,
                                               LPDIRECTSOUND *ppDS,
                                               LPUNKNOWN pUnkOuter);
typedef HRESULT(WINAPI *PFN_DirectSoundCreate8)(LPCGUID pcGuidDevice,
                                                LPDIRECTSOUND8 *ppDS8,
                                                LPUNKNOWN pUnkOuter);

// --- Original Function Pointers ---
static PFN_DirectSoundCreate Real_DirectSoundCreate = nullptr;
static PFN_DirectSoundCreate8 Real_DirectSoundCreate8 = nullptr;
static PFN_IDirectSound_CreateSoundBuffer Real_IDirectSound_CreateSoundBuffer =
    nullptr;
static PFN_IDirectSoundBuffer_Lock Real_IDirectSoundBuffer_Lock = nullptr;
static PFN_IDirectSoundBuffer_Play Real_IDirectSoundBuffer_Play = nullptr;

// --- Hooked Functions ---
HRESULT STDMETHODCALLTYPE
Hook_IDirectSoundBuffer_Play(IDirectSoundBuffer *pThis, DWORD dwReserved1,
                             DWORD dwPriority, DWORD dwFlags) {
  Log(format_string(
          "[DirectSound] IDirectSoundBuffer::Play -> Priority: %u, Flags: 0x%X",
          dwPriority, dwFlags)
          .c_str());
  return Real_IDirectSoundBuffer_Play(pThis, dwReserved1, dwPriority, dwFlags);
}

HRESULT STDMETHODCALLTYPE Hook_IDirectSoundBuffer_Lock(
    IDirectSoundBuffer *pThis, DWORD dwOffset, DWORD dwBytes,
    LPVOID *ppvAudioPtr1, LPDWORD pdwAudioBytes1, LPVOID *ppvAudioPtr2,
    LPDWORD pdwAudioBytes2, DWORD dwFlags) {
  Log(format_string("[DirectSound] IDirectSoundBuffer::Lock -> Offset: %u, "
                    "Bytes: %u, Flags: 0x%X",
                    dwOffset, dwBytes, dwFlags)
          .c_str());
  return Real_IDirectSoundBuffer_Lock(pThis, dwOffset, dwBytes, ppvAudioPtr1,
                                      pdwAudioBytes1, ppvAudioPtr2,
                                      pdwAudioBytes2, dwFlags);
}

HRESULT STDMETHODCALLTYPE Hook_IDirectSound_CreateSoundBuffer(
    IDirectSound *pThis, LPCDSBUFFERDESC pcDSBufferDesc,
    LPDIRECTSOUNDBUFFER *ppDSBuffer, LPUNKNOWN pUnkOuter) {
  const WAVEFORMATEX *pWfx =
      pcDSBufferDesc ? pcDSBufferDesc->lpwfxFormat : nullptr;
  Log(format_string("[DirectSound] IDirectSound::CreateSoundBuffer -> Flags: "
                    "0x%lX, BufferBytes: %lu, SampleRate: %lu, Channels: %u",
                    pcDSBufferDesc ? pcDSBufferDesc->dwFlags : 0,
                    pcDSBufferDesc ? pcDSBufferDesc->dwBufferBytes : 0,
                    pWfx ? pWfx->nSamplesPerSec : 0, pWfx ? pWfx->nChannels : 0)
          .c_str());

  HRESULT hr = Real_IDirectSound_CreateSoundBuffer(pThis, pcDSBufferDesc,
                                                   ppDSBuffer, pUnkOuter);
  if (SUCCEEDED(hr) && ppDSBuffer && *ppDSBuffer) {
    void **vtable = *(void ***)*ppDSBuffer;
    if (Real_IDirectSoundBuffer_Lock == nullptr) {
      Real_IDirectSoundBuffer_Lock = (PFN_IDirectSoundBuffer_Lock)vtable[10];
      Real_IDirectSoundBuffer_Play = (PFN_IDirectSoundBuffer_Play)vtable[11];
      DetourTransactionBegin();
      DetourUpdateThread(GetCurrentThread());
      DetourAttach(&(PVOID &)Real_IDirectSoundBuffer_Lock,
                   Hook_IDirectSoundBuffer_Lock);
      DetourAttach(&(PVOID &)Real_IDirectSoundBuffer_Play,
                   Hook_IDirectSoundBuffer_Play);
      DetourTransactionCommit();
    }
  }
  return hr;
}

void AttachToDirectSound(IUnknown *pDS) {
  void **vtable = *(void ***)pDS;
  if (Real_IDirectSound_CreateSoundBuffer == nullptr) {
    Real_IDirectSound_CreateSoundBuffer =
        (PFN_IDirectSound_CreateSoundBuffer)vtable[3];
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID &)Real_IDirectSound_CreateSoundBuffer,
                 Hook_IDirectSound_CreateSoundBuffer);
    DetourTransactionCommit();
  }
}

HRESULT WINAPI Hook_DirectSoundCreate(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS,
                                      LPUNKNOWN pUnkOuter) {
  Log("[DirectSound] DirectSoundCreate called.");
  HRESULT hr = Real_DirectSoundCreate(pcGuidDevice, ppDS, pUnkOuter);
  if (SUCCEEDED(hr) && ppDS && *ppDS) {
    AttachToDirectSound(*ppDS);
  }
  return hr;
}

HRESULT WINAPI Hook_DirectSoundCreate8(LPCGUID pcGuidDevice,
                                       LPDIRECTSOUND8 *ppDS8,
                                       LPUNKNOWN pUnkOuter) {
  Log("[DirectSound] DirectSoundCreate8 called.");
  HRESULT hr = Real_DirectSoundCreate8(pcGuidDevice, ppDS8, pUnkOuter);
  if (SUCCEEDED(hr) && ppDS8 && *ppDS8) {
    AttachToDirectSound(*ppDS8);
  }
  return hr;
}

// ===================================================================================
// 4. Legacy waveOut Hooks
// ===================================================================================

// --- Function Pointer Definitions ---
typedef MMRESULT(WINAPI *PFN_waveOutOpen)(LPHWAVEOUT phwo, UINT uDeviceID,
                                          LPCWAVEFORMATEX pwfx,
                                          DWORD_PTR dwCallback,
                                          DWORD_PTR dwInstance, DWORD fdwOpen);
typedef MMRESULT(WINAPI *PFN_waveOutClose)(HWAVEOUT hwo);
typedef MMRESULT(WINAPI *PFN_waveOutWrite)(HWAVEOUT hwo, LPWAVEHDR pwh,
                                           UINT cbwh);
typedef MMRESULT(WINAPI *PFN_waveOutReset)(HWAVEOUT hwo);

// --- Original Function Pointers ---
static PFN_waveOutOpen Real_waveOutOpen = nullptr;
static PFN_waveOutClose Real_waveOutClose = nullptr;
static PFN_waveOutWrite Real_waveOutWrite = nullptr;
static PFN_waveOutReset Real_waveOutReset = nullptr;

// --- Hooked Functions ---
MMRESULT WINAPI Hook_waveOutOpen(LPHWAVEOUT phwo, UINT uDeviceID,
                                 LPCWAVEFORMATEX pwfx, DWORD_PTR dwCallback,
                                 DWORD_PTR dwInstance, DWORD fdwOpen) {
  Log(format_string("[waveOut] waveOutOpen -> DeviceID: %u, SampleRate: %lu, "
                    "Channels: %u, BitsPerSample: %u, Flags: 0x%lX",
                    uDeviceID, pwfx ? pwfx->nSamplesPerSec : 0,
                    pwfx ? pwfx->nChannels : 0, pwfx ? pwfx->wBitsPerSample : 0,
                    fdwOpen)
          .c_str());
  return Real_waveOutOpen(phwo, uDeviceID, pwfx, dwCallback, dwInstance,
                          fdwOpen);
}

MMRESULT WINAPI Hook_waveOutClose(HWAVEOUT hwo) {
  Log(format_string("[waveOut] waveOutClose -> Handle: 0x%p", hwo).c_str());
  return Real_waveOutClose(hwo);
}

MMRESULT WINAPI Hook_waveOutWrite(HWAVEOUT hwo, LPWAVEHDR pwh, UINT cbwh) {
  Log(format_string("[waveOut] waveOutWrite -> Handle: 0x%p, BufferLength: "
                    "%lu, Flags: 0x%lX",
                    hwo, pwh ? pwh->dwBufferLength : 0, pwh ? pwh->dwFlags : 0)
          .c_str());
  return Real_waveOutWrite(hwo, pwh, cbwh);
}

MMRESULT WINAPI Hook_waveOutReset(HWAVEOUT hwo) {
  Log(format_string("[waveOut] waveOutReset -> Handle: 0x%p", hwo).c_str());
  return Real_waveOutReset(hwo);
}

// ===================================================================================
// 5. High-Level PlaySound Hooks
// ===================================================================================

// --- Function Pointer Definitions ---
typedef BOOL(WINAPI *PFN_PlaySoundA)(LPCSTR pszSound, HMODULE hmod,
                                     DWORD fdwSound);
typedef BOOL(WINAPI *PFN_PlaySoundW)(LPCWSTR pszSound, HMODULE hmod,
                                     DWORD fdwSound);

// --- Original Function Pointers ---
static PFN_PlaySoundA Real_PlaySoundA = nullptr;
static PFN_PlaySoundW Real_PlaySoundW = nullptr;

// --- Hooked Functions ---
BOOL WINAPI Hook_PlaySoundA(LPCSTR pszSound, HMODULE hmod, DWORD fdwSound) {
  Log(format_string("[PlaySound] PlaySoundA -> SoundName: %s, Flags: 0x%lX",
                    (fdwSound & SND_MEMORY) ? "(memory)"
                                            : (pszSound ? pszSound : "NULL"),
                    fdwSound)
          .c_str());
  return Real_PlaySoundA(pszSound, hmod, fdwSound);
}

BOOL WINAPI Hook_PlaySoundW(LPCWSTR pszSound, HMODULE hmod, DWORD fdwSound) {
  // For logging, we convert wide string to multibyte string
  char soundNameMb[256] = {0};
  if (!(fdwSound & SND_MEMORY) && pszSound) {
    WideCharToMultiByte(CP_ACP, 0, pszSound, -1, soundNameMb,
                        sizeof(soundNameMb) - 1, NULL, NULL);
  } else if (fdwSound & SND_MEMORY) {
    strcpy_s(soundNameMb, "(memory)");
  } else {
    strcpy_s(soundNameMb, "NULL");
  }
  Log(format_string("[PlaySound] PlaySoundW -> SoundName: %s, Flags: 0x%lX",
                    soundNameMb, fdwSound)
          .c_str());
  return Real_PlaySoundW(pszSound, hmod, fdwSound);
}

// ===================================================================================
// DLL Entry Point
// ===================================================================================

void AttachHooks() {
  // --- WASAPI ---
  // We can't hook CoCreateInstance easily for all cases, so we hook a
  // lower-level API. A good target is MMDevAPI's Activate method. We need to
  // get a pointer to it first. We do this by creating a temporary device
  // enumerator.

  // IMMDeviceEnumerator *pEnumerator = NULL;
  // IMMDevice *pDevice = NULL;
  // if (SUCCEEDED(CoInitialize(NULL))) {
  //   if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
  //                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
  //                                  (void **)&pEnumerator))) {
  //     if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,
  //                                                        &pDevice))) {
  //       void **vtable = *(void ***)pDevice;
  //       Real_IMMDevice_Activate = (PFN_IMMDevice_Activate)vtable[5];
  //       pDevice->Release();
  //     }
  //     pEnumerator->Release();
  //   }
  //   CoUninitialize();
  // }

  // --- XAudio2 ---
  HMODULE hXAudio = GetModuleHandleA("XAudio2_9.dll");
  if (!hXAudio)
    hXAudio = GetModuleHandleA("XAudio2_8.dll");
  if (!hXAudio)
    hXAudio = GetModuleHandleA("XAudio2_7.dll");
  if (hXAudio) {
    Real_XAudio2Create =
        (PFN_XAudio2Create)GetProcAddress(hXAudio, "XAudio2Create");
  }

  // --- DirectSound ---
  HMODULE hDsound = GetModuleHandleA("dsound.dll");
  if (hDsound) {
    Real_DirectSoundCreate =
        (PFN_DirectSoundCreate)GetProcAddress(hDsound, "DirectSoundCreate");
    Real_DirectSoundCreate8 =
        (PFN_DirectSoundCreate8)GetProcAddress(hDsound, "DirectSoundCreate8");
  }

  // --- waveOut & PlaySound ---
  HMODULE hWinmm = GetModuleHandleA("winmm.dll");
  if (hWinmm) {
    Real_waveOutOpen = (PFN_waveOutOpen)GetProcAddress(hWinmm, "waveOutOpen");
    Real_waveOutClose =
        (PFN_waveOutClose)GetProcAddress(hWinmm, "waveOutClose");
    Real_waveOutWrite =
        (PFN_waveOutWrite)GetProcAddress(hWinmm, "waveOutWrite");
    Real_waveOutReset =
        (PFN_waveOutReset)GetProcAddress(hWinmm, "waveOutReset");
    Real_PlaySoundA = (PFN_PlaySoundA)GetProcAddress(hWinmm, "PlaySoundA");
    Real_PlaySoundW = (PFN_PlaySoundW)GetProcAddress(hWinmm, "PlaySoundW");
  }

  // --- Attach all hooks in a single transaction ---
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());

  // if (Real_IMMDevice_Activate)
  //   DetourAttach(&(PVOID &)Real_IMMDevice_Activate, Hook_IMMDevice_Activate);
  if (Real_XAudio2Create)
    DetourAttach(&(PVOID &)Real_XAudio2Create, Hook_XAudio2Create);
  if (Real_DirectSoundCreate)
    DetourAttach(&(PVOID &)Real_DirectSoundCreate, Hook_DirectSoundCreate);
  if (Real_DirectSoundCreate8)
    DetourAttach(&(PVOID &)Real_DirectSoundCreate8, Hook_DirectSoundCreate8);
  if (Real_waveOutOpen)
    DetourAttach(&(PVOID &)Real_waveOutOpen, Hook_waveOutOpen);
  if (Real_waveOutClose)
    DetourAttach(&(PVOID &)Real_waveOutClose, Hook_waveOutClose);
  if (Real_waveOutWrite)
    DetourAttach(&(PVOID &)Real_waveOutWrite, Hook_waveOutWrite);
  if (Real_waveOutReset)
    DetourAttach(&(PVOID &)Real_waveOutReset, Hook_waveOutReset);
  if (Real_PlaySoundA)
    DetourAttach(&(PVOID &)Real_PlaySoundA, Hook_PlaySoundA);
  if (Real_PlaySoundW)
    DetourAttach(&(PVOID &)Real_PlaySoundW, Hook_PlaySoundW);

  if (DetourTransactionCommit() == NO_ERROR) {
    Log("====== All Audio Hooks Attached Successfully ======");
  } else {
    Log("!!!!!! FAILED to attach audio hooks !!!!!!");
  }
}

void DetachHooks() {
  DetourTransactionBegin();
  DetourUpdateThread(GetCurrentThread());

  // if (Real_IMMDevice_Activate)
  //   DetourDetach(&(PVOID &)Real_IMMDevice_Activate, Hook_IMMDevice_Activate);
  if (Real_XAudio2Create)
    DetourDetach(&(PVOID &)Real_XAudio2Create, Hook_XAudio2Create);
  if (Real_DirectSoundCreate)
    DetourDetach(&(PVOID &)Real_DirectSoundCreate, Hook_DirectSoundCreate);
  if (Real_DirectSoundCreate8)
    DetourDetach(&(PVOID &)Real_DirectSoundCreate8, Hook_DirectSoundCreate8);
  if (Real_waveOutOpen)
    DetourDetach(&(PVOID &)Real_waveOutOpen, Hook_waveOutOpen);
  if (Real_waveOutClose)
    DetourDetach(&(PVOID &)Real_waveOutClose, Hook_waveOutClose);
  if (Real_waveOutWrite)
    DetourDetach(&(PVOID &)Real_waveOutWrite, Hook_waveOutWrite);
  if (Real_waveOutReset)
    DetourDetach(&(PVOID &)Real_waveOutReset, Hook_waveOutReset);
  if (Real_PlaySoundA)
    DetourDetach(&(PVOID &)Real_PlaySoundA, Hook_PlaySoundA);
  if (Real_PlaySoundW)
    DetourDetach(&(PVOID &)Real_PlaySoundW, Hook_PlaySoundW);

  DetourTransactionCommit();
  Log("====== All Audio Hooks Detached ======");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  if (DetourIsHelperProcess()) {
    return TRUE;
  }

  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(hModule);
    AttachHooks();
    break;
  case DLL_PROCESS_DETACH:
    DetachHooks();
    break;
  }
  return TRUE;
}