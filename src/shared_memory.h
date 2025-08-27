#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <windows.h>

#include <stdexcept> // 用于在构造失败时抛出异常
#include <string>

template <typename T> class SharedMemoryValue {
public:
  // 定义两种模式：创建（用于主程序）和打开（用于DLL）
  enum class Mode { Create, Open };

  // 构造函数：根据模式创建或打开共享内存
  // name: 共享内存的全局名称
  // mode: Create 或 Open
  explicit SharedMemoryValue(const std::wstring &name, Mode mode)
      : mode_(mode), name_(name) {

    if (mode == Mode::Create) {
      // 主程序（创建者）的逻辑
      hMapFile_ = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                     0, sizeof(T), name.c_str());

      if (hMapFile_ == NULL) {
        // 抛出异常比返回bool或设置错误码更符合现代C++风格
        throw std::runtime_error("Could not create file mapping object.");
      }

      pSharedData_ = static_cast<T *>(
          MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(T)));
    } else { // Mode::Open
      // DLL（消费者）的逻辑
      hMapFile_ = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str());

      if (hMapFile_ == NULL) {
        // 如果打开失败，这不一定是致命错误（可能主程序还没启动），
        // 所以我们只将指针设为nullptr，让IsValid()返回false。
        pSharedData_ = nullptr;
        return;
      }

      pSharedData_ = static_cast<T *>(
          MapViewOfFile(hMapFile_, FILE_MAP_READ, 0, 0, sizeof(T)));
    }

    if (pSharedData_ == nullptr) {
      // 如果映射视图失败，需要关闭已打开的句柄
      CloseHandle(hMapFile_);
      hMapFile_ = NULL;
      // 如果是创建模式失败，则抛出异常
      if (mode == Mode::Create) {
        throw std::runtime_error("Could not map view of file.");
      }
    }
  }

  // 析构函数：自动清理资源 (RAII核心)
  ~SharedMemoryValue() {
    if (pSharedData_) {
      UnmapViewOfFile(pSharedData_);
    }
    if (hMapFile_) {
      CloseHandle(hMapFile_);
    }
  }

  // --- 阻止拷贝，因为我们管理着独一无二的系统资源 ---
  SharedMemoryValue(const SharedMemoryValue &) = delete;
  SharedMemoryValue &operator=(const SharedMemoryValue &) = delete;

  // --- 允许移动，以支持所有权转移 ---
  SharedMemoryValue(SharedMemoryValue &&other) noexcept
      : hMapFile_(other.hMapFile_), pSharedData_(other.pSharedData_),
        mode_(other.mode_), name_(std::move(other.name_)) {
    other.hMapFile_ = NULL;
    other.pSharedData_ = nullptr;
  }

  SharedMemoryValue &operator=(SharedMemoryValue &&other) noexcept {
    if (this != &other) {
      // 先释放自己的资源
      if (pSharedData_)
        UnmapViewOfFile(pSharedData_);
      if (hMapFile_)
        CloseHandle(hMapFile_);

      // 再接管对方的资源
      hMapFile_ = other.hMapFile_;
      pSharedData_ = other.pSharedData_;
      mode_ = other.mode_;
      name_ = std::move(other.name_);

      // 将对方置为空状态
      other.hMapFile_ = NULL;
      other.pSharedData_ = nullptr;
    }
    return *this;
  }

  // 检查共享内存是否成功初始化
  bool IsValid() const { return pSharedData_ != nullptr; }

  // 设置值（仅在Create模式下有效）
  bool SetValue(const T &value) {
    if (!IsValid() || mode_ != Mode::Create) {
      return false;
    }
    *pSharedData_ = value;
    return true;
  }

  // 获取值，如果无效则返回一个默认值
  T GetValue(const T &defaultValue) const {
    if (!IsValid()) {
      return defaultValue;
    }
    // 使用volatile读取以防止编译器过度优化，确保每次都从内存读取
    return *const_cast<volatile T *>(pSharedData_);
  }

private:
  HANDLE hMapFile_ = NULL;
  T *pSharedData_ = nullptr;
  Mode mode_;
  std::wstring name_;
};

#endif // SHARED_MEMORY_H