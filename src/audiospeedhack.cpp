#include <windows.h>

#include <audioclient.h>
#include <audiopolicy.h>
#include <chrono>
#include <detours.h>
#include <dsound.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <xaudio2.h>


// C++ Standard Library
#include <algorithm>
#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits> // For std::is_same_v
#include <utility>     // For std::move
#include <vector>

// --- SoundTouch ---
#include <soundtouch/SoundTouch.h>

// --- 1. 共享内存管理 ---
namespace SharedMemory {
const char *MAPPING_NAME = "GlobalAudioSpeedControl";
HANDLE hMapFile = NULL;
float *pSharedSpeed = NULL;

void Initialize() {
  hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, MAPPING_NAME);
  if (hMapFile == NULL) {
    pSharedSpeed = nullptr;
    OutputDebugStringA(
        "[AudioHook] Failed to open file mapping. Using default speed 1.0f.");
    return;
  }
  pSharedSpeed =
      (float *)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, sizeof(float));
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

void Cleanup() {
  if (pSharedSpeed)
    UnmapViewOfFile(pSharedSpeed);
  if (hMapFile)
    CloseHandle(hMapFile);
  pSharedSpeed = NULL;
  hMapFile = NULL;
}

float GetSpeedRatio() {
  if (pSharedSpeed) {
    float speed = *pSharedSpeed;
    if (speed > 0.1f && speed < 10.0f) { // 使用一个安全的范围
      return speed;
    }
  }
  return 1.0f;
}
} // namespace SharedMemory

// --- 2. 辅助工具：线程安全队列 ---
template <typename T> class ThreadSafeQueue {
public:
  void push(const T *data, size_t count) {
    std::lock_guard lock(m_mutex);
    m_queue.insert(m_queue.end(), data, data + count);
  }

  size_t pop(T *data, size_t max_count) {
    std::lock_guard lock(m_mutex);
    size_t count = std::min(max_count, m_queue.size());
    if (count > 0) {
      std::copy(m_queue.begin(), m_queue.begin() + count, data);
      m_queue.erase(m_queue.begin(), m_queue.begin() + count);
    }
    return count;
  }

  size_t size_bytes() {
    std::lock_guard lock(m_mutex);
    return m_queue.size() * sizeof(T);
  }

private:
  std::deque<T> m_queue;
  std::mutex m_mutex;
};

// --- 3. XAudio2 实现 ---

// --- XAudio2 全局变量与定义 ---
enum class SampleFormat { UNSUPPORTED, INT16, FLOAT32 };

struct XAudio2StreamState {
  std::unique_ptr<soundtouch::SoundTouch> st;
  std::vector<BYTE> outputBuffer;
  SampleFormat format = SampleFormat::UNSUPPORTED;
};

static std::map<IXAudio2SourceVoice *, XAudio2StreamState> g_xaudio2Streams;
static std::mutex g_xaudio2Mutex;

// XAudio2 函数指针类型定义
typedef HRESULT(STDMETHODCALLTYPE *PFN_IXAudio2SourceVoice_SubmitSourceBuffer)(
    IXAudio2SourceVoice *pThis, const XAUDIO2_BUFFER *pBuffer,
    const XAUDIO2_BUFFER_WMA *pBufferWma);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IXAudio2_CreateSourceVoice)(
    IXAudio2 *pThis, IXAudio2SourceVoice **ppSourceVoice,
    const WAVEFORMATEX *pSourceFormat, UINT32 Flags, float MaxFrequencyRatio,
    IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList,
    const XAUDIO2_EFFECT_CHAIN *pEffectChain);
typedef void(STDMETHODCALLTYPE *PFN_IXAudio2Voice_DestroyVoice)(
    IXAudio2Voice *pThis);

// 原始函数指针
typedef HRESULT(WINAPI *PFN_XAudio2Create)(IXAudio2 **ppXAudio2, UINT32 Flags,
                                           XAUDIO2_PROCESSOR XAudio2Processor);
static PFN_XAudio2Create Real_XAudio2Create = nullptr;
static PFN_IXAudio2SourceVoice_SubmitSourceBuffer
    Real_IXAudio2SourceVoice_SubmitSourceBuffer = nullptr;
static PFN_IXAudio2_CreateSourceVoice Real_IXAudio2_CreateSourceVoice = nullptr;
static PFN_IXAudio2Voice_DestroyVoice Real_IXAudio2Voice_DestroyVoice = nullptr;

// --- XAudio2 Hook 函数 ---

// --- FIX: 修改模板函数以处理 short -> float 的转换 ---
template <typename T>
HRESULT ProcessXAudio2Buffer(IXAudio2SourceVoice *pThis,
                             const XAUDIO2_BUFFER *pBuffer,
                             XAudio2StreamState &state) {
  float speed = SharedMemory::GetSpeedRatio();

  XAUDIO2_VOICE_DETAILS voiceDetails;
  pThis->GetVoiceDetails(&voiceDetails);

  state.st->setTempo(speed);

  const T *inputSamples = reinterpret_cast<const T *>(pBuffer->pAudioData);
  const uint numInputFrames =
      pBuffer->AudioBytes / (voiceDetails.InputChannels * sizeof(T));

  // 根据模板类型决定是否需要转换
  if constexpr (std::is_same_v<T, float>) {
    // 类型是 float，无需转换
    state.st->putSamples(inputSamples, numInputFrames);
  } else if constexpr (std::is_same_v<T, short>) {
    // 类型是 short，需要转换为 float
    std::vector<float> floatBuffer(numInputFrames * voiceDetails.InputChannels);
    for (size_t i = 0; i < floatBuffer.size(); ++i) {
      floatBuffer[i] = inputSamples[i] / 32768.0f;
    }
    state.st->putSamples(floatBuffer.data(), numInputFrames);
  }

  const uint outputBufferFrames = static_cast<uint>(numInputFrames / speed) +
                                  8192; // 增加缓冲区大小以防万一
  uint numOutputFrames = 0;

  if constexpr (std::is_same_v<T, float>) {
    // 接收 float 数据
    state.outputBuffer.resize(outputBufferFrames * voiceDetails.InputChannels *
                              sizeof(float));
    float *outputSamples = reinterpret_cast<float *>(state.outputBuffer.data());
    numOutputFrames =
        state.st->receiveSamples(outputSamples, outputBufferFrames);
  } else if constexpr (std::is_same_v<T, short>) {
    // 接收 float 数据，然后转换回 short
    std::vector<float> processedFloatBuffer(outputBufferFrames *
                                            voiceDetails.InputChannels);
    numOutputFrames = state.st->receiveSamples(processedFloatBuffer.data(),
                                               outputBufferFrames);
    if (numOutputFrames > 0) {
      state.outputBuffer.resize(numOutputFrames * voiceDetails.InputChannels *
                                sizeof(short));
      short *outputSamples =
          reinterpret_cast<short *>(state.outputBuffer.data());
      for (size_t i = 0; i < numOutputFrames * voiceDetails.InputChannels;
           ++i) {
        outputSamples[i] = static_cast<short>(std::clamp(
            processedFloatBuffer[i] * 32767.0f, -32768.0f, 32767.0f));
      }
    }
  }

  if (numOutputFrames == 0)
    return S_OK;

  XAUDIO2_BUFFER newBuffer = *pBuffer;
  newBuffer.pAudioData = state.outputBuffer.data();
  newBuffer.AudioBytes =
      numOutputFrames * voiceDetails.InputChannels * sizeof(T);

  pThis->SetFrequencyRatio(1.0f);
  return Real_IXAudio2SourceVoice_SubmitSourceBuffer(pThis, &newBuffer,
                                                     nullptr);
}

HRESULT STDMETHODCALLTYPE Hook_IXAudio2SourceVoice_SubmitSourceBuffer(
    IXAudio2SourceVoice *pThis, const XAUDIO2_BUFFER *pBuffer,
    const XAUDIO2_BUFFER_WMA *pBufferWma) {

  float speed = SharedMemory::GetSpeedRatio();
  if (speed == 1.0f || pBufferWma != nullptr || pBuffer->pAudioData == NULL ||
      pBuffer->AudioBytes == 0) {
    pThis->SetFrequencyRatio(1.0f);
    return Real_IXAudio2SourceVoice_SubmitSourceBuffer(pThis, pBuffer,
                                                       pBufferWma);
  }

  std::lock_guard lock(g_xaudio2Mutex);
  auto it = g_xaudio2Streams.find(pThis);
  if (it == g_xaudio2Streams.end() ||
      it->second.format == SampleFormat::UNSUPPORTED) {
    pThis->SetFrequencyRatio(speed);
    return Real_IXAudio2SourceVoice_SubmitSourceBuffer(pThis, pBuffer,
                                                       pBufferWma);
  }

  XAudio2StreamState &state = it->second;
  if (state.format == SampleFormat::INT16) {
    return ProcessXAudio2Buffer<short>(pThis, pBuffer, state);
  } else if (state.format == SampleFormat::FLOAT32) {
    return ProcessXAudio2Buffer<float>(pThis, pBuffer, state);
  }

  return Real_IXAudio2SourceVoice_SubmitSourceBuffer(pThis, pBuffer,
                                                     pBufferWma);
}

void STDMETHODCALLTYPE Hook_IXAudio2Voice_DestroyVoice(IXAudio2Voice *pThis) {
  {
    std::lock_guard<std::mutex> lock(g_xaudio2Mutex);
    if (g_xaudio2Streams.erase(static_cast<IXAudio2SourceVoice *>(pThis))) {
      OutputDebugStringA("[AudioHook] Cleaned up XAudio2 stream state.");
    }
  }
  Real_IXAudio2Voice_DestroyVoice(pThis);
}

HRESULT STDMETHODCALLTYPE Hook_IXAudio2_CreateSourceVoice(
    IXAudio2 *pThis, IXAudio2SourceVoice **ppSourceVoice,
    const WAVEFORMATEX *pSourceFormat, UINT32 Flags, float MaxFrequencyRatio,
    IXAudio2VoiceCallback *pCallback, const XAUDIO2_VOICE_SENDS *pSendList,
    const XAUDIO2_EFFECT_CHAIN *pEffectChain) {

  HRESULT hr = Real_IXAudio2_CreateSourceVoice(
      pThis, ppSourceVoice, pSourceFormat, Flags, XAUDIO2_MAX_FREQ_RATIO,
      pCallback, pSendList, pEffectChain);

  if (SUCCEEDED(hr) && ppSourceVoice && *ppSourceVoice) {
    void **vtable = *(void ***)*ppSourceVoice;

    if (Real_IXAudio2SourceVoice_SubmitSourceBuffer == nullptr) {
      Real_IXAudio2SourceVoice_SubmitSourceBuffer =
          (PFN_IXAudio2SourceVoice_SubmitSourceBuffer)vtable[9];
      Real_IXAudio2Voice_DestroyVoice =
          (PFN_IXAudio2Voice_DestroyVoice)vtable[0];

      DetourTransactionBegin();
      DetourUpdateThread(GetCurrentThread());
      DetourAttach(&(PVOID &)Real_IXAudio2SourceVoice_SubmitSourceBuffer,
                   Hook_IXAudio2SourceVoice_SubmitSourceBuffer);
      DetourAttach(&(PVOID &)Real_IXAudio2Voice_DestroyVoice,
                   Hook_IXAudio2Voice_DestroyVoice);
      DetourTransactionCommit();
    }

    SampleFormat format = SampleFormat::UNSUPPORTED;
    if (pSourceFormat->wFormatTag == WAVE_FORMAT_PCM &&
        pSourceFormat->wBitsPerSample == 16) {
      format = SampleFormat::INT16;
    } else if (pSourceFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT &&
               pSourceFormat->wBitsPerSample == 32) {
      format = SampleFormat::FLOAT32;
    }

    if (format != SampleFormat::UNSUPPORTED) {
      std::lock_guard<std::mutex> lock(g_xaudio2Mutex);
      auto &state = g_xaudio2Streams[*ppSourceVoice];
      state.format = format;
      state.st = std::make_unique<soundtouch::SoundTouch>();
      state.st->setSampleRate(pSourceFormat->nSamplesPerSec);
      state.st->setChannels(pSourceFormat->nChannels);
      OutputDebugStringA(
          "[AudioHook] Created SoundTouch instance for XAudio2 stream.");
    }
  }
  return hr;
}

HRESULT WINAPI Hook_XAudio2Create(IXAudio2 **ppXAudio2, UINT32 Flags,
                                  XAUDIO2_PROCESSOR XAudio2Processor) {
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

// --- 4. DirectSound 实现 ---

// DirectSound 函数指针类型定义
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSoundBuffer8_Lock)(
    IDirectSoundBuffer8 *pThis, DWORD dwOffset, DWORD dwBytes,
    LPVOID *ppvAudioPtr1, LPDWORD pdwAudioBytes1, LPVOID *ppvAudioPtr2,
    LPDWORD pdwAudioBytes2, DWORD dwFlags);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSoundBuffer8_Unlock)(
    IDirectSoundBuffer8 *pThis, LPVOID pvAudioPtr1, DWORD dwAudioBytes1,
    LPVOID pvAudioPtr2, DWORD dwAudioBytes2);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSoundBuffer8_Play)(
    IDirectSoundBuffer8 *pThis, DWORD dwReserved1, DWORD dwPriority,
    DWORD dwFlags);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSoundBuffer8_Stop)(
    IDirectSoundBuffer8 *pThis);
typedef ULONG(STDMETHODCALLTYPE *PFN_IDirectSoundBuffer8_Release)(
    IDirectSoundBuffer8 *pThis);
typedef HRESULT(STDMETHODCALLTYPE *PFN_IDirectSound8_CreateSoundBuffer)(
    IDirectSound8 *pThis, LPCDSBUFFERDESC pcDSBufferDesc,
    LPDIRECTSOUNDBUFFER *ppDSBuffer, LPUNKNOWN pUnkOuter);

// DirectSound 全局变量与定义
struct DirectSoundStreamState {
  std::unique_ptr<soundtouch::SoundTouch> st;
  WAVEFORMATEX wfx;
  std::vector<BYTE> stagingBuffer;
  ThreadSafeQueue<BYTE> playbackQueue;
  std::jthread workerThread;
  std::stop_source stopSource;
  std::atomic<bool> isPlaying{false};
  DWORD bufferSizeBytes = 0;
  PFN_IDirectSoundBuffer8_Lock pfnLock = nullptr;
  PFN_IDirectSoundBuffer8_Unlock pfnUnlock = nullptr;
  PFN_IDirectSoundBuffer8_Play pfnPlay = nullptr;
  PFN_IDirectSoundBuffer8_Stop pfnStop = nullptr;
  PFN_IDirectSoundBuffer8_Release pfnRelease = nullptr;
};

static std::map<IDirectSoundBuffer8 *, DirectSoundStreamState> g_dsoundStreams;
static std::mutex g_dsoundMutex;

// 原始函数指针
typedef HRESULT(WINAPI *PFN_DirectSoundCreate8)(LPCGUID pcGuidDevice,
                                                LPDIRECTSOUND8 *ppDS8,
                                                LPUNKNOWN pUnkOuter);
static PFN_DirectSoundCreate8 Real_DirectSoundCreate8 =
    (PFN_DirectSoundCreate8)GetProcAddress(GetModuleHandleA("dsound.dll"),
                                           "DirectSoundCreate8");
static PFN_IDirectSound8_CreateSoundBuffer
    Real_IDirectSound8_CreateSoundBuffer = nullptr;

// --- DirectSound Hook 函数 ---

void dsound_worker_thread(IDirectSoundBuffer8 *pBuffer,
                          DirectSoundStreamState &state,
                          std::stop_token stoken) {
  using namespace std::chrono_literals;
  DWORD lastWriteCursor = 0;

  while (!stoken.stop_requested() && state.isPlaying) {
    DWORD playCursor, writeCursor;
    if (SUCCEEDED(pBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
      lastWriteCursor = writeCursor;
      break;
    }
    std::this_thread::sleep_for(10ms);
  }

  while (!stoken.stop_requested()) {
    float speed = SharedMemory::GetSpeedRatio();
    if (speed == 1.0f || !state.isPlaying) {
      std::this_thread::sleep_for(100ms);
      continue;
    }

    DWORD playCursor, writeCursor;
    if (FAILED(pBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
      break;

    DWORD bytesToFill;
    if (lastWriteCursor >= playCursor) {
      bytesToFill = (state.bufferSizeBytes - lastWriteCursor) + playCursor;
    } else {
      bytesToFill = playCursor - lastWriteCursor;
    }
    bytesToFill = (bytesToFill > 2048) ? (bytesToFill - 2048) : 0;

    if (bytesToFill > 0) {
      DWORD bytesInQueue = (DWORD)state.playbackQueue.size_bytes();
      if (bytesInQueue > 0) {
        DWORD bytesToWrite = std::min(bytesToFill, bytesInQueue);
        LPVOID ptr1, ptr2;
        DWORD size1, size2;
        if (SUCCEEDED(state.pfnLock(pBuffer, lastWriteCursor, bytesToWrite,
                                    &ptr1, &size1, &ptr2, &size2, 0))) {
          state.playbackQueue.pop(static_cast<BYTE *>(ptr1), size1);
          if (ptr2) {
            state.playbackQueue.pop(static_cast<BYTE *>(ptr2), size2);
          }
          state.pfnUnlock(pBuffer, ptr1, size1, ptr2, size2);
          lastWriteCursor =
              (lastWriteCursor + size1 + size2) % state.bufferSizeBytes;
        }
      }
    }
    std::this_thread::sleep_for(15ms);
  }
}

HRESULT STDMETHODCALLTYPE Hook_IDirectSoundBuffer8_Lock(
    IDirectSoundBuffer8 *pThis, DWORD dwOffset, DWORD dwBytes,
    LPVOID *ppvAudioPtr1, LPDWORD pdwAudioBytes1, LPVOID *ppvAudioPtr2,
    LPDWORD pdwAudioBytes2, DWORD dwFlags) {
  float speed = SharedMemory::GetSpeedRatio();
  std::lock_guard lock(g_dsoundMutex);
  auto it = g_dsoundStreams.find(pThis);
  if (it == g_dsoundStreams.end() || speed == 1.0f) {
    return it->second.pfnLock(pThis, dwOffset, dwBytes, ppvAudioPtr1,
                              pdwAudioBytes1, ppvAudioPtr2, pdwAudioBytes2,
                              dwFlags);
  }

  auto &state = it->second;
  state.stagingBuffer.resize(dwBytes);
  *ppvAudioPtr1 = state.stagingBuffer.data();
  *pdwAudioBytes1 = dwBytes;
  *ppvAudioPtr2 = nullptr;
  *pdwAudioBytes2 = 0;
  return DS_OK;
}

HRESULT STDMETHODCALLTYPE Hook_IDirectSoundBuffer8_Unlock(
    IDirectSoundBuffer8 *pThis, LPVOID pvAudioPtr1, DWORD dwAudioBytes1,
    LPVOID pvAudioPtr2, DWORD dwAudioBytes2) {
  float speed = SharedMemory::GetSpeedRatio();
  std::lock_guard lock(g_dsoundMutex);
  auto it = g_dsoundStreams.find(pThis);
  if (it == g_dsoundStreams.end() || speed == 1.0f) {
    return it->second.pfnUnlock(pThis, pvAudioPtr1, dwAudioBytes1, pvAudioPtr2,
                                dwAudioBytes2);
  }

  auto &state = it->second;
  state.st->setTempo(speed);

  const short *inputSamples =
      reinterpret_cast<const short *>(state.stagingBuffer.data());
  const uint numInputFrames = dwAudioBytes1 / state.wfx.nBlockAlign;
  std::vector<float> floatBuffer(numInputFrames * state.wfx.nChannels);
  for (size_t i = 0; i < floatBuffer.size(); ++i) {
    floatBuffer[i] = inputSamples[i] / 32768.0f;
  }
  state.st->putSamples(floatBuffer.data(), numInputFrames);

  uint samplesToProcess = state.st->numSamples();
  if (samplesToProcess > 0) {
    std::vector<float> processedFloatBuffer(samplesToProcess *
                                            state.wfx.nChannels);
    uint receivedFrames =
        state.st->receiveSamples(processedFloatBuffer.data(), samplesToProcess);
    if (receivedFrames > 0) {
      std::vector<short> processedShortBuffer(receivedFrames *
                                              state.wfx.nChannels);
      for (size_t i = 0; i < processedShortBuffer.size(); ++i) {
        processedShortBuffer[i] = static_cast<short>(std::clamp(
            processedFloatBuffer[i] * 32767.0f, -32768.0f, 32767.0f));
      }
      state.playbackQueue.push(
          reinterpret_cast<BYTE *>(processedShortBuffer.data()),
          receivedFrames * state.wfx.nBlockAlign);
    }
  }
  return DS_OK;
}

HRESULT STDMETHODCALLTYPE
Hook_IDirectSoundBuffer8_Play(IDirectSoundBuffer8 *pThis, DWORD dwReserved1,
                              DWORD dwPriority, DWORD dwFlags) {
  std::lock_guard lock(g_dsoundMutex);
  auto it = g_dsoundStreams.find(pThis);
  if (it != g_dsoundStreams.end()) {
    auto &state = it->second;
    if (!state.isPlaying) {
      state.isPlaying = true;
      if (state.workerThread.joinable()) {
        state.stopSource.request_stop();
        state.workerThread.join();
      }
      state.stopSource = std::stop_source();
      state.workerThread =
          std::jthread(dsound_worker_thread, pThis, std::ref(state),
                       state.stopSource.get_token());
    }
    return state.pfnPlay(pThis, dwReserved1, dwPriority, dwFlags);
  }

  void **vtable = *(void ***)pThis;
  PFN_IDirectSoundBuffer8_Play pfnOriginalPlay =
      (PFN_IDirectSoundBuffer8_Play)vtable[11];
  return pfnOriginalPlay(pThis, dwReserved1, dwPriority, dwFlags);
}

HRESULT STDMETHODCALLTYPE
Hook_IDirectSoundBuffer8_Stop(IDirectSoundBuffer8 *pThis) {
  std::lock_guard lock(g_dsoundMutex);
  auto it = g_dsoundStreams.find(pThis);
  if (it != g_dsoundStreams.end()) {
    auto &state = it->second;
    state.isPlaying = false;
    return state.pfnStop(pThis);
  }

  void **vtable = *(void ***)pThis;
  PFN_IDirectSoundBuffer8_Stop pfnOriginalStop =
      (PFN_IDirectSoundBuffer8_Stop)vtable[16];
  return pfnOriginalStop(pThis);
}

ULONG STDMETHODCALLTYPE
Hook_IDirectSoundBuffer8_Release(IDirectSoundBuffer8 *pThis) {
  PFN_IDirectSoundBuffer8_Release pfnRealRelease = nullptr;

  {
    std::lock_guard lock(g_dsoundMutex);
    auto it = g_dsoundStreams.find(pThis);
    if (it != g_dsoundStreams.end()) {
      pfnRealRelease = it->second.pfnRelease;
    } else {
      void **vtable = *(void ***)pThis;
      pfnRealRelease = (PFN_IDirectSoundBuffer8_Release)vtable[2];
    }
  }

  ULONG refCount = pfnRealRelease(pThis);

  if (refCount == 0) {
    std::lock_guard lock(g_dsoundMutex);
    auto it = g_dsoundStreams.find(pThis);
    if (it != g_dsoundStreams.end()) {
      it->second.stopSource.request_stop();
      if (it->second.workerThread.joinable()) {
        it->second.workerThread.join();
      }

      PFN_IDirectSoundBuffer8_Lock pfnLock = it->second.pfnLock;
      PFN_IDirectSoundBuffer8_Unlock pfnUnlock = it->second.pfnUnlock;
      PFN_IDirectSoundBuffer8_Play pfnPlay = it->second.pfnPlay;
      PFN_IDirectSoundBuffer8_Stop pfnStop = it->second.pfnStop;

      g_dsoundStreams.erase(it);

      DetourTransactionBegin();
      DetourUpdateThread(GetCurrentThread());
      DetourDetach(&(PVOID &)pfnLock, Hook_IDirectSoundBuffer8_Lock);
      DetourDetach(&(PVOID &)pfnUnlock, Hook_IDirectSoundBuffer8_Unlock);
      DetourDetach(&(PVOID &)pfnPlay, Hook_IDirectSoundBuffer8_Play);
      DetourDetach(&(PVOID &)pfnStop, Hook_IDirectSoundBuffer8_Stop);
      DetourDetach(&(PVOID &)pfnRealRelease, Hook_IDirectSoundBuffer8_Release);
      DetourTransactionCommit();

      OutputDebugStringA(
          "[AudioHook] Cleaned up and detached DirectSound buffer instance.");
    }
  }
  return refCount;
}

HRESULT STDMETHODCALLTYPE Hook_IDirectSound8_CreateSoundBuffer(
    IDirectSound8 *pThis, LPCDSBUFFERDESC pcDSBufferDesc,
    LPDIRECTSOUNDBUFFER *ppDSBuffer, LPUNKNOWN pUnkOuter) {

  DSBUFFERDESC desc = *pcDSBufferDesc;
  desc.dwFlags |= DSBCAPS_GETCURRENTPOSITION2;

  HRESULT hr =
      Real_IDirectSound8_CreateSoundBuffer(pThis, &desc, ppDSBuffer, pUnkOuter);
  if (SUCCEEDED(hr) && ppDSBuffer && *ppDSBuffer) {
    IDirectSoundBuffer *pDSB = *ppDSBuffer;
    IDirectSoundBuffer8 *pDSB8 = nullptr;
    if (SUCCEEDED(
            pDSB->QueryInterface(IID_IDirectSoundBuffer8, (void **)&pDSB8))) {
      WAVEFORMATEX wfx = {0};
      DWORD wfxSize = 0;
      if (SUCCEEDED(pDSB8->GetFormat(&wfx, sizeof(wfx), &wfxSize)) &&
          wfx.wFormatTag == WAVE_FORMAT_PCM && wfx.wBitsPerSample == 16) {
        std::lock_guard lock(g_dsoundMutex);
        if (g_dsoundStreams.find(pDSB8) == g_dsoundStreams.end()) {
          auto &state = g_dsoundStreams[pDSB8];
          state.wfx = wfx;
          state.bufferSizeBytes = desc.dwBufferBytes;
          state.st = std::make_unique<soundtouch::SoundTouch>();
          state.st->setSampleRate(wfx.nSamplesPerSec);
          state.st->setChannels(wfx.nChannels);

          void **vtable = *(void ***)pDSB8;
          state.pfnLock = (PFN_IDirectSoundBuffer8_Lock)vtable[10];
          state.pfnUnlock = (PFN_IDirectSoundBuffer8_Unlock)vtable[18];
          state.pfnPlay = (PFN_IDirectSoundBuffer8_Play)vtable[11];
          state.pfnStop = (PFN_IDirectSoundBuffer8_Stop)vtable[16];
          state.pfnRelease = (PFN_IDirectSoundBuffer8_Release)vtable[2];

          DetourTransactionBegin();
          DetourUpdateThread(GetCurrentThread());
          DetourAttach(&(PVOID &)state.pfnLock, Hook_IDirectSoundBuffer8_Lock);
          DetourAttach(&(PVOID &)state.pfnUnlock,
                       Hook_IDirectSoundBuffer8_Unlock);
          DetourAttach(&(PVOID &)state.pfnPlay, Hook_IDirectSoundBuffer8_Play);
          DetourAttach(&(PVOID &)state.pfnStop, Hook_IDirectSoundBuffer8_Stop);
          DetourAttach(&(PVOID &)state.pfnRelease,
                       Hook_IDirectSoundBuffer8_Release);
          if (DetourTransactionCommit() == NO_ERROR) {
            OutputDebugStringA("[AudioHook] Successfully hooked a new "
                               "DirectSound buffer instance.");
          } else {
            OutputDebugStringA("[AudioHook] Failed to hook a new DirectSound "
                               "buffer instance.");
            g_dsoundStreams.erase(pDSB8);
          }
        }
      }
      pDSB8->Release();
    }
  }
  return hr;
}

HRESULT WINAPI Hook_DirectSoundCreate8(LPCGUID pcGuidDevice,
                                       LPDIRECTSOUND8 *ppDS8,
                                       LPUNKNOWN pUnkOuter) {
  HRESULT hr = Real_DirectSoundCreate8(pcGuidDevice, ppDS8, pUnkOuter);
  if (SUCCEEDED(hr) && ppDS8 && *ppDS8) {
    void **vtable = *(void ***)*ppDS8;
    if (Real_IDirectSound8_CreateSoundBuffer == nullptr) {
      Real_IDirectSound8_CreateSoundBuffer =
          (PFN_IDirectSound8_CreateSoundBuffer)vtable[3];
      DetourTransactionBegin();
      DetourUpdateThread(GetCurrentThread());
      DetourAttach(&(PVOID &)Real_IDirectSound8_CreateSoundBuffer,
                   Hook_IDirectSound8_CreateSoundBuffer);
      DetourTransactionCommit();
    }
  }
  return hr;
}

// --- 5. DLL 入口函数 ---

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  if (DetourIsHelperProcess())
    return TRUE;

  if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    SharedMemory::Initialize();

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

    if (Real_XAudio2Create)
      DetourAttach(&(PVOID &)Real_XAudio2Create, Hook_XAudio2Create);
    if (Real_DirectSoundCreate8)
      DetourAttach(&(PVOID &)Real_DirectSoundCreate8, Hook_DirectSoundCreate8);

    if (DetourTransactionCommit() != NO_ERROR) {
      MessageBoxW(NULL, L"Failed to attach hooks!", L"Hook Error", MB_OK);
    }

  } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());

    if (Real_XAudio2Create)
      DetourDetach(&(PVOID &)Real_XAudio2Create, Hook_XAudio2Create);
    if (Real_DirectSoundCreate8)
      DetourDetach(&(PVOID &)Real_DirectSoundCreate8, Hook_DirectSoundCreate8);

    DetourTransactionCommit();

    {
      std::lock_guard lock(g_dsoundMutex);
      for (auto &pair : g_dsoundStreams) {
        pair.second.stopSource.request_stop();
      }
      g_dsoundStreams.clear();
    }
    {
      std::lock_guard lock(g_xaudio2Mutex);
      g_xaudio2Streams.clear();
    }

    SharedMemory::Cleanup();
  }
  return TRUE;
}