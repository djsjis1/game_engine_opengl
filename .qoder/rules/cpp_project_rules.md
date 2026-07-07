---
trigger: always_on
---
# C++ 企业级项目规范

## 适用范围

本规范适用于所有 C++ 企业级项目，与 engineering_rules.md 配合使用。
engineering_rules.md 定义通用编码原则，本文件定义 C++ 工程实践细节。

---

# C++ 标准规范

统一使用 C++17，新项目可使用 C++20。

禁止：

* 混用不同 C++ 标准特性导致编译器兼容性问题
* 使用已废弃的 C++ 特性（如 `auto_ptr`、`register`）
* 依赖未定义行为（UB）

推荐使用 C++17 特性：

* `std::optional` 替代可空指针返回值
* `std::variant` 替代 union
* `if constexpr` 替代 SFINAE 模板特化
* 结构化绑定 `auto [a, b] = pair`

---

# 类型安全规范

使用固定宽度整型，禁止依赖平台相关类型大小：

| 禁止 | 必须 |
|------|------|
| `int` | `int32_t` |
| `unsigned int` | `uint32_t` |
| `long` | `int64_t` |
| `unsigned long` | `uint64_t` |

禁止隐式类型转换：

```cpp
// 禁止
int32_t a = 100;
int64_t b = a;  // 隐式转换

// 必须
int64_t b = static_cast<int64_t>(a);
```

禁止 C 风格强制转换：

```cpp
// 禁止
int* p = (int*)ptr;

// 必须
int* p = static_cast<int*>(ptr);
```

---

# 初始化规范

所有变量必须在声明时初始化：

```cpp
// 禁止
int count;
bool flag;

// 必须
int32_t count = 0;
bool flag = false;
```

优先使用花括号初始化（防止窄化转换）：

```cpp
// 推荐
int32_t value{42};
std::vector<int32_t> items{1, 2, 3};

// 类成员初始化
class VideoDecoder {
    int32_t frame_count_{0};
    bool is_running_{false};
};
```

---

# 枚举规范

必须使用 `enum class`，禁止裸 `enum`：

```cpp
// 禁止
enum Status { OK, FAILED, TIMEOUT };

// 必须
enum class DecodeStatus : int32_t {
    kSuccess = 0,
    kFailed = -1,
    kTimeout = -2,
};
```

枚举值必须有明确的底层类型。

---

# Rule of 0 / 3 / 5

**Rule of 0**：优先设计不需要手动管理资源的类（用智能指针和 RAII）。

**Rule of 5**：若类定义了析构函数，必须同时定义或删除：

* 拷贝构造函数
* 拷贝赋值运算符
* 移动构造函数
* 移动赋值运算符

```cpp
class VideoBuffer {
public:
    VideoBuffer() = default;
    ~VideoBuffer();

    // 必须明确声明以下四个
    VideoBuffer(const VideoBuffer&) = delete;             // 禁止拷贝
    VideoBuffer& operator=(const VideoBuffer&) = delete;

    VideoBuffer(VideoBuffer&&) noexcept;                  // 允许移动
    VideoBuffer& operator=(VideoBuffer&&) noexcept;
};
```

移动构造函数和移动赋值运算符必须标记 `noexcept`。

析构函数必须标记 `noexcept`（默认即是，不得抛出异常）。

---

# noexcept 规范

以下函数必须标记 `noexcept`：

* 析构函数
* 移动构造函数
* 移动赋值运算符
* 不会抛出异常的工具函数

```cpp
// 必须
VideoDecoder(VideoDecoder&&) noexcept = default;
~VideoDecoder() noexcept;
int32_t getFrameCount() const noexcept { return frame_count_; }
```

---

> 测试框架、覆盖率、四类必测场景、用例命名等完整规范请参阅 `testing_rules.md`

---

# 异常策略

项目统一选择一种错误处理策略，禁止混用：

**策略一（推荐用于性能敏感项目）**：使用错误码 + `std::optional` / `std::expected`

```cpp
// 推荐
std::optional<VideoFrame> decodeFrame(const AVPacket* packet);
// 失败返回 std::nullopt，调用方必须检查
```

**策略二**：使用异常，但必须：

* 只在构造函数、无法恢复的错误中抛出
* 禁止在析构函数中抛出
* 禁止跨线程传递异常

不论选择哪种策略，必须在 `docs/adr/` 中记录决策。

---

# 命名空间规范

每个模块必须定义独立命名空间：

```cpp
namespace player {
namespace codec {

class VideoDecoder { ... };

} // namespace codec
} // namespace player
```

禁止：

* 在头文件中使用 `using namespace`
* 全局命名空间中定义非 main 的业务符号
* 命名空间嵌套超过 3 层

推荐使用 C++17 嵌套命名空间语法：

```cpp
namespace player::codec {
    class VideoDecoder { ... };
}
```

---

# 全局状态规范

禁止全局变量（包括 `static` 全局对象）。

禁止裸单例模式（`getInstance()` 返回静态局部变量除外）：

```cpp
// 禁止
static VideoDecoder* g_decoder = nullptr;

// 允许（Meyers Singleton，线程安全）
class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }
};
```

单例必须有明确的销毁时机，不得依赖程序退出自动销毁。

---

# C++ 命名约定

## 常量命名

全局常量、类成员常量、constexpr 常量统一使用 `k` 前缀 + PascalCase：

```cpp
constexpr int32_t kMaxRetryCount = 3;
constexpr int32_t kReconnectDelayMs = 1000;
constexpr double kPi = 3.14159265;

class VideoDecoder {
    static constexpr int32_t kDefaultBufferSize = 4096;
};
```

## 成员变量命名

类成员变量使用尾缀下划线 `_`：

```cpp
class VideoDecoder {
    int32_t frame_count_{0};
    bool is_running_{false};
    std::unique_ptr<Impl> impl_;  // pImpl 模式
};
```

## 函数/方法命名

函数使用 camelCase，表达动作 + 对象：

```cpp
class VideoDecoder {
    bool initialize(const Config& config);
    std::optional<VideoFrame> decodeFrame(const AVPacket* packet);
    int32_t getFrameCount() const noexcept;
    void releaseBuffers();
};
```

布尔 getter 使用 `is` / `has` / `can` 前缀。

## 文件名

* 头文件：`.h` 扩展名（非 `.hpp`）
* 源文件：`.cpp` 扩展名
* 文件名与类名一致，使用 PascalCase：`VideoDecoder.h`、`VideoDecoder.cpp`
* 工具类/函数文件使用 snake_case：`string_utils.h`、`frame_pool.cpp`

---

# C++ 并发实现规范

## 锁使用

锁的持锁时间必须尽量短：

```cpp
// 禁止：持锁期间调用外部回调
std::lock_guard<std::mutex> lock(mutex_);
callback_(data);  // 可能引发死锁

// 必须：先拷贝数据，释放锁后再回调
{
    std::lock_guard<std::mutex> lock(mutex_);
    local_data = shared_data_;
}
callback_(local_data);
```

多锁获取顺序必须全局一致（防止死锁）。

优先使用 `std::lock_guard` / `std::unique_lock`，禁止裸 `mutex.lock()` / `mutex.unlock()`。

## 条件变量

条件变量必须配合循环检查（防止虚假唤醒）：

```cpp
// 禁止
cv.wait(lock);

// 必须
cv.wait(lock, [this] { return condition_; });
```

## 原子操作

简单标志位优先使用 `std::atomic<bool>` 而非 mutex：

```cpp
std::atomic<bool> running_{true};  // 线程安全退出标志
```

---

# C++ 注释规范

## Doxygen 格式

公共接口必须使用 Doxygen 格式文档注释：

```cpp
/**
 * @brief 解码一帧视频数据
 * @param packet 输入的编码包，不能为空
 * @return 解码后的帧，失败返回 std::nullopt
 */
std::optional<VideoFrame> decodeFrame(const AVPacket* packet);
```

## 注释原则

* 注释解释「为什么」，而非「做什么」
* 复杂算法必须在函数上方说明算法思想和时间复杂度
* 禁止注释掉的死代码长期保留（应直接删除）
* 禁止无意义注释，如 `// 加1` 对应 `i++`
* 禁止翻译代码的注释

---

# C++ 安全规范

## 禁止不安全的 C 函数

| 禁止 | 替代 |
|------|------|
| `strcpy` | `strncpy` / `std::string` |
| `sprintf` | `snprintf` / `std::format` |
| `gets` | `fgets` |
| `scanf("%s")` | 限定长度版本 |

## 边界检查

* 数组下标访问前必须检查边界
* `std::vector::at()` 替代 `operator[]`（Debug 模式越界抛异常）
* 外部传入的偏移量、长度必须验证合法性

## 整数安全

* 禁止有符号/无符号混用比较
* 整数运算注意溢出（优先使用 `std::size_t` 处理大小）
* 缓冲区大小计算必须检查溢出

---

# C++ 编译规范

## 零警告原则

* 必须在 `-Wall -Wextra -Werror` 下无警告编译
* 禁止使用 `#pragma warning(disable)` 屏蔽警告
* 禁止强制类型转换绕过编译器警告

## 头文件规范

* 必须有头文件保护（`#pragma once` 或 include guard）
* 包含顺序：自身头文件 → 项目头文件 → 第三方库 → 标准库
* 禁止在头文件中使用 `using namespace`
* 尽量使用前置声明减少不必要的 `#include`（降低编译依赖）

---

# Git 工作流规范

> Commit 规范、分支策略、PR/MR 要求、Tag 版本、AI 提交建议等完整规范
> 请参阅 `git_rules.md`

---

# 代码审查规范

## 必须审查的场景

* 所有 PR 合并前必须至少 1 人 Review
* 以下场景必须至少 2 人 Review：
  * 公共接口变更
  * 并发相关代码
  * 安全敏感代码（输入解析、权限控制）
  * 性能关键路径变更

## Review Checklist

审查者必须检查：

**正确性**

* [ ] 逻辑是否正确，边界条件是否处理
* [ ] 错误路径是否有返回值
* [ ] 资源是否在所有路径上正确释放

**并发安全**

* [ ] 共享数据是否有锁保护
* [ ] 锁的获取顺序是否一致
* [ ] 是否存在 TOCTOU（检查时/使用时）竞争

**性能**

* [ ] 热路径是否引入不必要的分配
* [ ] 是否有不必要的数据拷贝

**可维护性**

* [ ] 命名是否清晰表达意图
* [ ] 是否有必要的注释（为什么，而非做什么）
* [ ] 测试是否覆盖新增逻辑

**安全**

* [ ] 外部输入是否已验证
* [ ] 是否使用了不安全的 C 函数

---

# 工具链规范

## 代码格式化

必须使用 `clang-format`：

* `.clang-format` 配置文件必须纳入版本控制
* 提交前必须自动格式化（建议配置 pre-commit hook）
* 禁止手动格式化与工具格式化混用
* CI 流水线中必须检查格式化合规

推荐基础配置：

```yaml
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
```

## 静态分析

必须使用 `clang-tidy`：

* `.clang-tidy` 配置文件纳入版本控制
* CI 流水线中 `clang-tidy` 必须零错误通过
* 禁止使用 `// NOLINT` 跳过检查（特殊情况需注释原因）

推荐启用的检查组：

* `bugprone-*`（常见 bug 模式）
* `cppcoreguidelines-*`（C++ Core Guidelines）
* `modernize-*`（现代 C++ 用法）
* `performance-*`（性能问题）
* `readability-*`（可读性）

## 内存与运行时检测

Debug 构建必须开启 Sanitizer：

```cmake
# CMakeLists.txt
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -fsanitize=address,undefined
    )
endif()
```

| Sanitizer | 检测内容 |
|-----------|----------|
| `AddressSanitizer (ASAN)` | 内存越界、use-after-free |
| `UndefinedBehaviorSanitizer (UBSAN)` | 未定义行为、整数溢出 |
| `ThreadSanitizer (TSAN)` | 数据竞争（与 ASAN 互斥） |

发布版本禁止开启 Sanitizer。

---

# 构建系统规范

## CMake 规范

必须使用 target-based CMake（CMake 3.15+）：

```cmake
# 禁止
include_directories(${PROJECT_SOURCE_DIR}/include)
link_libraries(FFmpeg::avcodec)

# 必须
target_include_directories(VideoDecoder
    PRIVATE ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(VideoDecoder
    PRIVATE FFmpeg::avcodec
)
```

## 构建配置

必须支持以下构建类型：

| 类型 | 用途 | Sanitizer |
|------|------|-----------|
| `Debug` | 开发调试 | 必须开启 ASAN/UBSAN |
| `RelWithDebInfo` | 集成测试 | 不开启 |
| `Release` | 生产发布 | 禁止开启 |

## 依赖管理

第三方库引入必须：

1. 评估许可证是否与项目兼容
2. 锁定具体版本号（禁止使用 `latest`）
3. 优先使用系统包管理或 CMake FetchContent，不得直接复制源码到项目（特殊情况需在 ADR 中说明）
4. 在 `docs/architecture.md` 中记录引入原因

```cmake
# 推荐：使用 FetchContent 锁定版本
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0  # 必须锁定版本
)
```

---

# ABI / API 兼容性规范

## 对外接口变更原则

公共头文件中的接口（类、函数、结构体）修改前必须评估：

* 是否破坏二进制兼容性（ABI Break）
* 是否破坏源码兼容性（API Break）

**禁止**在同一大版本内：

* 删除公共接口
* 修改函数签名（参数类型、顺序、数量）
* 修改已导出类的成员布局

## 废弃策略

废弃接口必须：

1. 使用 `[[deprecated("使用 newFunction() 替代")]]` 标注
2. 在至少一个版本内保留废弃接口
3. 在 `docs/api.md` 和 `changelog.md` 中记录

```cpp
// 标记废弃，提示替代方案
[[deprecated("请使用 decodeFrameAsync() 替代，将在 v3.0 移除")]]
VideoFrame* decodeFrame(const AVPacket* packet);
```

## 动态库导出规范

```cpp
// 统一使用宏控制符号导出
#if defined(_WIN32)
    #define PLAYER_API __declspec(dllexport)
#else
    #define PLAYER_API __attribute__((visibility("default")))
#endif

class PLAYER_API VideoDecoder {
    // 仅暴露必要接口
};
```

---

# 测试规范（细化）

> 测试框架、覆盖率、四类必测场景（单元/集成/压力/异常）、用例命名等完整规范
> 请参阅 `testing_rules.md`

---

# CI/CD 规范

每次 PR 必须自动触发以下流水线，全部通过才能合并：

| 检查项 | 工具 |
|--------|------|
| 代码格式 | `clang-format --dry-run` |
| 静态分析 | `clang-tidy` |
| 编译（零警告） | `-Wall -Wextra -Werror` |
| 单元测试 | `ctest` |
| 内存检测（Debug） | ASAN + UBSAN |

CI 构建失败必须在 4 小时内修复，不得带红灯合并。

---

# 最终检查

提交前确认：

✓ C++ 标准版本一致

✓ 所有变量已初始化

✓ 枚举使用 enum class

✓ 遵循 Rule of 0/3/5

✓ 移动语义函数标记 noexcept

✓ 无全局变量

✓ 命名空间正确封装

✓ Commit message 格式正确

✓ clang-format 通过

✓ clang-tidy 零错误

✓ 单元测试覆盖新增逻辑

✓ Debug 构建 Sanitizer 无报告

不满足必须修复后再提交。
