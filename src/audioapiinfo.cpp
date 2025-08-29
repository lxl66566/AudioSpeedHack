// This file is a debug utility for hooking audio APIs and logging their
// invocations using manual VTable patching.
#include <windows.h>

#include <audioclient.h>
#include <audiopolicy.h>
#include <dsound.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <mmsystem.h>
#include <xaudio2.h>

#include <map>
#include <memory> // For std::unique_ptr
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

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
// Manual VTable Hooking Utilities
// ===================================================================================

// Patches a VTable entry.
void VTableHook(void **vtable, int index, void *hookFunc, void **originalFunc) {
  if (!vtable || !hookFunc || !originalFunc)
    return;

  // Store the original function pointer if it hasn't been stored yet.
  if (*originalFunc == nullptr) {
    *originalFunc = vtable[index];
  }

  // Change memory protection to allow writing.
  DWORD oldProtect;
  if (VirtualProtect(&vtable[index], sizeof(void *), PAGE_READWRITE,
                     &oldProtect)) {
    vtable[index] = hookFunc;
    // Restore original memory protection.
    VirtualProtect(&vtable[index], sizeof(void *), oldProtect, &oldProtect);
  } else {
    Log("VTableHook: VirtualProtect failed!");
  }
}

// Restores a VTable entry.
void VTableUnhook(void **vtable, int index, void *originalFunc) {
  if (!vtable || !originalFunc)
    return;

  DWORD oldProtect;
  if (VirtualProtect(&vtable[index], sizeof(void *), PAGE_READWRITE,
                     &oldProtect)) {
    vtable[index] = originalFunc;
    VirtualProtect(&vtable[index], sizeof(void *), oldProtect, &oldProtect);
  }
}

// ===================================================================================
// 1. Core Audio (WASAPI) Hooks
// ===================================================================================

// // --- Function Pointer Definitions ---
// typedef HRESULT(STDMETHODCALLTYPE *PFN_IAudioClient_Initialize)(
//     IAudioClient *pThis, AUDCLNT_SHAREMODE ShareMode, DWORD StreamFlags,
//     REFERENCE_TIME hnsBufferDuration, REFERENCE_TIME hnsPeriodicity,
//     const WAVEFORMATEX *pFormat, LPCGUID AudioSessionGuid);
// typedef HRESULT(STDMETHODCALLTYPE *PFN_IAudioClient_GetService)(
//     IAudioClient *pThis, REFIID riid, void **ppv);
// typedef HRESULT(STDMETHODCALLTYPE *PFN_IAudioRenderClient_GetBuffer)(
//     IAudioRenderClient *pThis, UINT32 NumFramesRequested, BYTE **ppData);
// typedef HRESULT(STDMETHODCALLTYPE *PFN_IAudioRenderClient_ReleaseBuffer)(
//     IAudioRenderClient *pThis, UINT32 NumFramesWritten, DWORD dwFlags);
// typedef HRESULT(STDMETHODCALLTYPE *PFN_IMMDevice_Activate)(
//     IMMDevice *pThis, REFIID iid, DWORD dwClsCtx,
//     PROPVARIANT *pActivationParams, void **ppInterface);

// // --- Original Function Pointers ---
// static PFN_IAudioClient_Initialize Real_IAudioClient_Initialize = nullptr;
// static PFN_IAudioClient_GetService Real_IAudioClient_GetService = nullptr;
// static PFN_IAudioRenderClient_GetBuffer Real_IAudioRenderClient_GetBuffer =
//     nullptr;
// static PFN_IAudioRenderClient_ReleaseBuffer
//     Real_IAudioRenderClient_ReleaseBuffer = nullptr;
// static PFN_IMMDevice_Activate Real_IMMDevice_Activate = nullptr;

// // --- Hooked Functions ---

// HRESULT STDMETHODCALLTYPE Hook_IAudioRenderClient_ReleaseBuffer(
//     IAudioRenderClient *pThis, UINT32 NumFramesWritten, DWORD dwFlags) {
//   Log(format_string("[WASAPI] IAudioRenderClient::ReleaseBuffer -> "
//                     "NumFramesWritten: %u, Flags: 0x%X",
//                     NumFramesWritten, dwFlags)
//           .c_str());
//   return Real_IAudioRenderClient_ReleaseBuffer(pThis, NumFramesWritten,
//                                                dwFlags);
// }

// HRESULT STDMETHODCALLTYPE Hook_IAudioRenderClient_GetBuffer(
//     IAudioRenderClient *pThis, UINT32 NumFramesRequested, BYTE **ppData) {
//   Log(format_string(
//           "[WASAPI] IAudioRenderClient::GetBuffer -> NumFramesRequested: %u",
//           NumFramesRequested)
//           .c_str());
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

//   if (SUCCEEDED(hr) && riid == IID_IAudioRenderClient && *ppv) {
//     Log("[WASAPI] IAudioClient::GetService -> IID_IAudioRenderClient
//     obtained. "
//         "Hooking its methods.");
//     IAudioRenderClient *pRenderClient = static_cast<IAudioRenderClient
//     *>(*ppv); void **vtable = *(void ***)pRenderClient;

//     // Hook the VTable only once
//     if (Real_IAudioRenderClient_GetBuffer == nullptr) {
//       VTableHook(vtable, 3, (void *)Hook_IAudioRenderClient_GetBuffer,
//                  (void **)&Real_IAudioRenderClient_GetBuffer);
//       VTableHook(vtable, 4, (void *)Hook_IAudioRenderClient_ReleaseBuffer,
//                  (void **)&Real_IAudioRenderClient_ReleaseBuffer);
//     }
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

//   if (SUCCEEDED(hr) && iid == IID_IAudioClient && *ppInterface) {
//     Log("[WASAPI] IMMDevice::Activate -> IID_IAudioClient obtained. Hooking "
//         "its methods.");
//     IAudioClient *pAudioClient = static_cast<IAudioClient *>(*ppInterface);
//     void **vtable = *(void ***)pAudioClient;

//     // Hook the VTable only once
//     if (Real_IAudioClient_Initialize == nullptr) {
//       VTableHook(vtable, 3, (void *)Hook_IAudioClient_Initialize,
//                  (void **)&Real_IAudioClient_Initialize);
//       VTableHook(vtable, 8, (void *)Hook_IAudioClient_GetService,
//                  (void **)&Real_IAudioClient_GetService);
//     }
//   }
//   return hr;
// }

// ===================================================================================
// 2. XAudio2 Hooks (Logging Only)
// ===================================================================================

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

static PFN_XAudio2Create Real_XAudio2Create = nullptr;
static PFN_IXAudio2_CreateSourceVoice Real_IXAudio2_CreateSourceVoice = nullptr;
static PFN_IXAudio2SourceVoice_SubmitSourceBuffer
    Real_IXAudio2SourceVoice_SubmitSourceBuffer = nullptr;

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
    if (Real_IXAudio2SourceVoice_SubmitSourceBuffer == nullptr) {
      VTableHook(vtable, 9, (void *)Hook_IXAudio2SourceVoice_SubmitSourceBuffer,
                 (void **)&Real_IXAudio2SourceVoice_SubmitSourceBuffer);
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
      VTableHook(vtable, 3, (void *)Hook_IXAudio2_CreateSourceVoice,
                 (void **)&Real_IXAudio2_CreateSourceVoice);
    }
  }
  return hr;
}

// ===================================================================================
// 3. DirectSound Hooks (Logging Only)
// ===================================================================================

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

static PFN_DirectSoundCreate Real_DirectSoundCreate = nullptr;
static PFN_DirectSoundCreate8 Real_DirectSoundCreate8 = nullptr;
static PFN_IDirectSound_CreateSoundBuffer Real_IDirectSound_CreateSoundBuffer =
    nullptr;
static PFN_IDirectSoundBuffer_Lock Real_IDirectSoundBuffer_Lock = nullptr;
static PFN_IDirectSoundBuffer_Play Real_IDirectSoundBuffer_Play = nullptr;

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
      VTableHook(vtable, 11, (void *)Hook_IDirectSoundBuffer_Lock,
                 (void **)&Real_IDirectSoundBuffer_Lock);
      VTableHook(vtable, 12, (void *)Hook_IDirectSoundBuffer_Play,
                 (void **)&Real_IDirectSoundBuffer_Play);
    }
  }
  return hr;
}

void AttachToDirectSound(IUnknown *pDS) {
  void **vtable = *(void ***)pDS;
  if (Real_IDirectSound_CreateSoundBuffer == nullptr) {
    VTableHook(vtable, 3, (void *)Hook_IDirectSound_CreateSoundBuffer,
               (void **)&Real_IDirectSound_CreateSoundBuffer);
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
// 4. Legacy waveOut & PlaySound Hooks (via simple function pointer replacement)
// ===================================================================================

// For exported functions, we will use a simple inline hook (memory patch).
// This map stores original bytes for unhooking.
static std::map<void *, std::vector<BYTE>> g_inlineHooks;

void InlineHook(void *target, void *hook, void **original) {
  if (!target || !hook || !original)
    return;

  *original = target; // The "real" function is just the original address.

  DWORD oldProtect;
  if (VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    // Save original bytes
    std::vector<BYTE> originalBytes(5);
    memcpy(originalBytes.data(), target, 5);
    g_inlineHooks[target] = originalBytes;

    // Write jmp instruction (E9) + relative offset
    BYTE jmp[5];
    jmp[0] = 0xE9;
    DWORD relativeOffset = (DWORD)hook - (DWORD)target - 5;
    memcpy(&jmp[1], &relativeOffset, 4);

    memcpy(target, jmp, 5);

    VirtualProtect(target, 5, oldProtect, &oldProtect);
  } else {
    Log("InlineHook: VirtualProtect failed!");
  }
}

void InlineUnhook(void *target) {
  if (g_inlineHooks.find(target) == g_inlineHooks.end())
    return;

  DWORD oldProtect;
  if (VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) {
    memcpy(target, g_inlineHooks[target].data(), 5);
    VirtualProtect(target, 5, oldProtect, &oldProtect);
    g_inlineHooks.erase(target);
  }
}

// --- Function Pointer Definitions ---
typedef MMRESULT(WINAPI *PFN_waveOutOpen)(LPHWAVEOUT phwo, UINT uDeviceID,
                                          LPCWAVEFORMATEX pwfx,
                                          DWORD_PTR dwCallback,
                                          DWORD_PTR dwInstance, DWORD fdwOpen);
typedef MMRESULT(WINAPI *PFN_waveOutClose)(HWAVEOUT hwo);
typedef MMRESULT(WINAPI *PFN_waveOutWrite)(HWAVEOUT hwo, LPWAVEHDR pwh,
                                           UINT cbwh);
typedef MMRESULT(WINAPI *PFN_waveOutReset)(HWAVEOUT hwo);
typedef BOOL(WINAPI *PFN_PlaySoundA)(LPCSTR pszSound, HMODULE hmod,
                                     DWORD fdwSound);
typedef BOOL(WINAPI *PFN_PlaySoundW)(LPCWSTR pszSound, HMODULE hmod,
                                     DWORD fdwSound);

// --- Original Function Pointers ---
static PFN_waveOutOpen Real_waveOutOpen = nullptr;
static PFN_waveOutClose Real_waveOutClose = nullptr;
static PFN_waveOutWrite Real_waveOutWrite = nullptr;
static PFN_waveOutReset Real_waveOutReset = nullptr;
static PFN_PlaySoundA Real_PlaySoundA = nullptr;
static PFN_PlaySoundW Real_PlaySoundW = nullptr;

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
BOOL WINAPI Hook_PlaySoundA(LPCSTR pszSound, HMODULE hmod, DWORD fdwSound) {
  Log(format_string("[PlaySound] PlaySoundA -> SoundName: %s, Flags: 0x%lX",
                    (fdwSound & SND_MEMORY) ? "(memory)"
                                            : (pszSound ? pszSound : "NULL"),
                    fdwSound)
          .c_str());
  return Real_PlaySoundA(pszSound, hmod, fdwSound);
}
BOOL WINAPI Hook_PlaySoundW(LPCWSTR pszSound, HMODULE hmod, DWORD fdwSound) {
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
  Log("====== Attaching Audio Hooks... ======");

  // --- WASAPI ---
  // NOTE: Calling CoCreateInstance in DllMain is DANGEROUS and can cause
  // deadlocks. This is done here to get a VTable pointer, but a safer method
  // would be to hook CoCreateInstance itself.
  // IMMDeviceEnumerator *pEnumerator = NULL;
  // IMMDevice *pDevice = NULL;
  // if (SUCCEEDED(CoInitialize(NULL))) {
  //   if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
  //                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
  //                                  (void **)&pEnumerator))) {
  //     if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,
  //                                                        &pDevice))) {
  //       void **vtable = *(void ***)pDevice;
  //       // Hook IMMDevice::Activate (index 5)
  //       VTableHook(vtable, 5, (void *)Hook_IMMDevice_Activate,
  //                  (void **)&Real_IMMDevice_Activate);
  //       pDevice->Release();
  //     }
  //     pEnumerator->Release();
  //   }
  //   CoUninitialize();
  // }

  // --- XAudio2, DirectSound, waveOut, PlaySound (Exported Functions) ---
  // We hook these using a simple inline hook on the function's entry point.
  HMODULE hXAudio = GetModuleHandleA("XAudio2_9.dll");
  if (!hXAudio)
    hXAudio = GetModuleHandleA("XAudio2_8.dll");
  if (!hXAudio)
    hXAudio = GetModuleHandleA("XAudio2_7.dll");
  if (hXAudio) {
    void *pfn = GetProcAddress(hXAudio, "XAudio2Create");
    if (pfn)
      InlineHook(pfn, Hook_XAudio2Create, (void **)&Real_XAudio2Create);
  }

  HMODULE hDsound = GetModuleHandleA("dsound.dll");
  if (hDsound) {
    void *pfnCreate = GetProcAddress(hDsound, "DirectSoundCreate");
    if (pfnCreate)
      InlineHook(pfnCreate, Hook_DirectSoundCreate,
                 (void **)&Real_DirectSoundCreate);
    void *pfnCreate8 = GetProcAddress(hDsound, "DirectSoundCreate8");
    if (pfnCreate8)
      InlineHook(pfnCreate8, Hook_DirectSoundCreate8,
                 (void **)&Real_DirectSoundCreate8);
  }

  HMODULE hWinmm = GetModuleHandleA("winmm.dll");
  if (hWinmm) {
    void *pfnOpen = GetProcAddress(hWinmm, "waveOutOpen");
    if (pfnOpen)
      InlineHook(pfnOpen, Hook_waveOutOpen, (void **)&Real_waveOutOpen);
    void *pfnClose = GetProcAddress(hWinmm, "waveOutClose");
    if (pfnClose)
      InlineHook(pfnClose, Hook_waveOutClose, (void **)&Real_waveOutClose);
    void *pfnWrite = GetProcAddress(hWinmm, "waveOutWrite");
    if (pfnWrite)
      InlineHook(pfnWrite, Hook_waveOutWrite, (void **)&Real_waveOutWrite);
    void *pfnReset = GetProcAddress(hWinmm, "waveOutReset");
    if (pfnReset)
      InlineHook(pfnReset, Hook_waveOutReset, (void **)&Real_waveOutReset);
    void *pfnPlayA = GetProcAddress(hWinmm, "PlaySoundA");
    if (pfnPlayA)
      InlineHook(pfnPlayA, Hook_PlaySoundA, (void **)&Real_PlaySoundA);
    void *pfnPlayW = GetProcAddress(hWinmm, "PlaySoundW");
    if (pfnPlayW)
      InlineHook(pfnPlayW, Hook_PlaySoundW, (void **)&Real_PlaySoundW);
  }

  Log("====== Audio Hooks Attached ======");
}

void DetachHooks() {
  Log("====== Detaching All Audio Hooks ======");
  // Unhook all inline hooks
  if (Real_XAudio2Create)
    InlineUnhook((void *)Real_XAudio2Create);
  if (Real_DirectSoundCreate)
    InlineUnhook((void *)Real_DirectSoundCreate);
  if (Real_DirectSoundCreate8)
    InlineUnhook((void *)Real_DirectSoundCreate8);
  if (Real_waveOutOpen)
    InlineUnhook((void *)Real_waveOutOpen);
  if (Real_waveOutClose)
    InlineUnhook((void *)Real_waveOutClose);
  if (Real_waveOutWrite)
    InlineUnhook((void *)Real_waveOutWrite);
  if (Real_waveOutReset)
    InlineUnhook((void *)Real_waveOutReset);
  if (Real_PlaySoundA)
    InlineUnhook((void *)Real_PlaySoundA);
  if (Real_PlaySoundW)
    InlineUnhook((void *)Real_PlaySoundW);

  // Note: VTable hooks are harder to unhook globally and safely,
  // as the original VTable might be unloaded. Since we are detaching on
  // process exit, it's generally okay to leave them patched.
  // A more robust solution would track all patched VTables.
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
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