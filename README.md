# GarmentStyleMatch

服装模特图与手绘图匹配分类工具。

- 目标平台：Windows / macOS
- 技术栈：Qt 6 (QML + Widgets) + C++17，CMake 构建

## 目录结构

```
.
├── CMakeLists.txt      根 CMake 工程
├── src/                应用源码
│   ├── main.cpp
│   ├── core/           C++ 业务逻辑（Controller / Model）
│   └── qml/            QML 界面
├── 3rdparty/           第三方源码或二进制依赖
├── doc/                设计文档、截图、需求
├── install/            打包脚本 / 安装工件
└── tool/               辅助脚本
```

## 构建

需要 Qt 6.10 或以上（含 QtQuick / QtQuickControls2 模块）。

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="<path-to-Qt>/lib/cmake"
cmake --build build --config Release
```
