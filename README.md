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

需要 Qt 6.5 或以上（含 QtQuick / QtQuickControls2 模块）。

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="<path-to-Qt>/lib/cmake"
cmake --build build --config Release
```

生成的可执行文件位于 `build/bin/GarmentStyleMatch(.exe)`。

## 界面概览

主窗口按 2.png 参考图组织：

- 顶部标题栏（款式候选确认台 + 版本 / 日期）
- 左侧：候选款号列表（含置信度、张数）
- 中间：当前实拍图预览 + 翻页、查看原图
- 右侧：小图库（分类下拉 + 搜索 + 2 列网格）
- 底部：确认栏（选中小图 / 确认款号 / 生成微调模型）

后续将由 `MatchController` 承接目录扫描、AI 匹配调用、结果落盘等逻辑。
