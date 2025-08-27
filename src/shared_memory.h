#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <windows.h>

// 创建用于速度控制的全局共享内存
// 返回值: 成功返回 true，失败返回 false
bool CreateSharedMemory();

// 通过共享内存设置速度
void SetSpeed(float speed);

// 清理和关闭共享内存句柄
void CleanupSharedMemory();

#endif // SHARED_MEMORY_H