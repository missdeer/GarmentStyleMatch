## C++ Coding Standards (tools/crawler-webengine 专用)

`tools/crawler-webengine/` 下的所有 C++ 代码都必须符合 [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)，以仓库根目录的 `.clang-tidy` 启用的检查清单为准（包括 `cppcoreguidelines-*`、`modernize-*`、`performance-*`、`readability-*` 等）。生成或修改代码时主动遵循以下要点，**写之前就避免引入问题**，比写完再修要省事得多：

### 资源与所有权
- 优先 RAII；禁止裸 `new`/`delete`，用 `std::unique_ptr` / `std::make_unique` / `std::shared_ptr` 管理堆对象。
- Qt 对象使用 Qt 的 parent-child 所有权机制，不要再叠加智能指针。
- 不写 C 风格指针算术；需要连续内存视图用 `std::span` 或 `gsl::span`。

### 类型与可变性
- 默认 `const`：函数参数、成员函数、局部变量能 const 就 const；可在编译期求值的写 `constexpr`。
- 返回值类型默认加 `[[nodiscard]]`，除非确认调用方可以丢弃。
- 优先 `enum class`，不用裸 `enum`。
- 不要用 `using namespace std;`；头文件中绝不要用任何 `using namespace`。
- 头文件用 `#pragma once`（项目已统一此风格——`llvm-header-guard` / `portability-avoid-pragma-once` 这两条仍会报，但属于全局风格冲突，**不要为单个文件改成 `#ifndef` 宏卫**，保持与项目其它头文件一致即可）。

### 函数与控制流
- 单一返回点不是教条，但短函数优先 early-return；避免深层嵌套。
- 单语句 `if`/`for`/`while` 也要带花括号（`readability-braces-around-statements`）。
- 标识符长度至少 3 个字符（`readability-identifier-length`）；循环变量除外（`i`/`j`/`k` 仍可接受）。

### 现代化语法
- 用 `auto` 推导显然的类型（迭代器、`make_*` 返回值），但不要滥用让读者猜类型。
- 范围 for 替代下标循环；`std::*_v` / `std::*_t` 替代 `::value` / `::type`。
- 用 `nullptr`，不用 `NULL` / `0`。
- 重写虚函数加 `override`；不再被继承的类标 `final`。
- `noexcept` 标记真正不会抛的函数（特别是移动构造/赋值、析构）。

### 全局与静态
- 避免非 const 全局可变状态（`cppcoreguidelines-avoid-non-const-global-variables`）。日志互斥锁等不可避免的，集中在 anonymous namespace 中并明确加注释说明。

### 日志与错误处理
- 不要 `throw` 一般异常用作控制流；Qt 信号槽 + 返回值/`std::optional`/`std::expected` 风格更契合本项目。
- 不要吞 `try/catch`；要么处理，要么向上传播。
