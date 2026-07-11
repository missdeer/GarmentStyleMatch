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

## 自动匹配款号

自动匹配使用以下本地 ONNX 模型：

- SegFormer B2 Clothes：从实拍图提取上衣、裤子、裙子或连衣裙。
- FashionCLIP：生成实拍裁剪与款号手绘图的 512 维特征，并以余弦相似度匹配。

配置 CMake 时会自动下载并校验 CUDA、DirectML 两套 ONNX Runtime 1.24.4，临时文件只写入项目 `tmp/`。要在本机启用自动匹配，首次运行前需在 PowerShell 中运行以下命令准备模型；没有模型时仍可编译应用，但不会部署模型或注册 ONNX Runtime 推理测试：

```powershell
.\scripts\setup-onnxruntime.ps1
```

脚本同样只使用项目 `tmp/`，校验 SHA-256 后把模型部署到被 Git 忽略的 `3rdparty/`；它也可以提前准备 ONNX Runtime，避免 CMake 配置阶段再次下载。CUDA 使用官方 GitHub Release，DirectML 使用微软官方 NuGet。构建会把两套运行时和模型复制到可执行文件目录。

CI 发布包还包含 `scripts/download-models.ps1`。该脚本可在源码树或解压后的 Windows 包中运行，模型会写入对应的 `models/` 目录。

运行时在 NVIDIA Windows 机器上优先选择 CUDA，在其他 Windows GPU 上选择 DirectML，Session 创建失败时回退 CPU；Linux x64/aarch64 和 macOS arm64 使用官方 CPU Runtime。ONNX Runtime 1.24.4 没有官方 macOS x64 Release 资产，Intel Mac 需要手动提供 `GSM_ONNXRUNTIME_ROOT`。Windows 可通过 QSettings 的 `matching/provider` 将后端覆盖为 `cuda`、`directml` 或 `cpu`。首次匹配会把图库特征写入应用缓存目录的 `style_embeddings.sqlite`；图片或模型修改后对应特征会自动重建。上下装命中不同款号时，输入框以英文逗号连接，例如 `TOP001,BOTTOM002`。
