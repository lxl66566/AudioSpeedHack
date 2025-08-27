#include "shared_memory.h"
#include <iostream>

const wchar_t *MAPPING_NAME = L"GlobalAudioSpeedControl";
HANDLE hMapFile = NULL;
float *pSharedSpeed = NULL;

bool CreateSharedMemory() {
  hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                sizeof(float), MAPPING_NAME);
  if (hMapFile == NULL) {
    std::wcerr << L"Could not create file mapping object: " << GetLastError()
               << std::endl;
    return false;
  }
  pSharedSpeed = (float *)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0,
                                        sizeof(float));
  if (pSharedSpeed == NULL) {
    std::wcerr << L"Could not map view of file: " << GetLastError()
               << std::endl;
    CloseHandle(hMapFile);
    hMapFile = NULL;
    return false;
  }
  return true;
}

void SetSpeed(float speed) {
  if (pSharedSpeed) {
    *pSharedSpeed = speed;
    std::wcout << L"Speed set to: " << speed << std::endl;
  }
}

void CleanupSharedMemory() {
  if (pSharedSpeed) {
    UnmapViewOfFile(pSharedSpeed);
    pSharedSpeed = NULL;
  }
  if (hMapFile) {
    CloseHandle(hMapFile);
    hMapFile = NULL;
  }
}