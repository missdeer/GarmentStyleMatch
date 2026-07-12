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

配置 CMake 时会自动下载并校验包含 CUDA 与 TensorRT provider 的 ONNX Runtime 1.27.1 CUDA 13，以及 ONNX Runtime 1.24.4 DirectML；公共头文件固定使用 1.24.4，临时文件只写入项目 `tmp/`。要在本机启用自动匹配，首次运行前需在 PowerShell 中运行以下命令准备模型；没有模型时仍可编译应用，但不会部署模型或注册 ONNX Runtime 推理测试：

```powershell
.\scripts\setup-onnxruntime.ps1
```

脚本同样只使用项目 `tmp/`，校验 SHA-256 后把模型部署到被 Git 忽略的 `3rdparty/`；它也可以提前准备 ONNX Runtime，避免 CMake 配置阶段再次下载。CUDA 使用官方 GitHub Release，DirectML 使用微软官方 NuGet。构建会把两套运行时和模型复制到可执行文件目录。

CI 发布包还包含 `scripts/download-models.ps1`。该脚本可在源码树或解压后的 Windows 包中运行，模型会写入对应的 `models/` 目录。

运行时在具备 TensorRT 10、CUDA 13、cuDNN 9 和 NVIDIA 驱动的 Windows 机器上优先选择 TensorRT，其次选择 CUDA，在其他 Windows GPU 上选择 DirectML；TensorRT Session 创建失败时依次回退 CUDA 和 CPU。Linux x64/aarch64 和 macOS arm64 使用官方 CPU Runtime。ONNX Runtime 1.24.4 没有官方 macOS x64 Release 资产，Intel Mac 需要手动提供 `GSM_ONNXRUNTIME_ROOT`。Windows 可通过 QSettings 的 `matching/provider` 将后端覆盖为 `tensorrt`、`cuda`、`directml` 或 `cpu`。一次进程生命周期只加载启动时选定的一套 Runtime，修改推理引擎后必须重启应用才会生效。首次匹配会把图库特征写入应用缓存目录的 `style_embeddings.sqlite`；图片、模型、Runtime 版本或推理后端修改后对应特征会自动重建。上下装命中不同款号时，输入框以英文逗号连接，例如 `TOP001,BOTTOM002`。

TensorRT 与 CUDA 共用自动下载的 `onnxruntime/cuda/` Runtime。TensorRT 仍需要本机安装 NVIDIA TensorRT 10；可通过 `TENSORRT_ROOT` 指向其安装目录（`lib/` 中应包含 `nvinfer_10.dll` 和 `nvonnxparser_10.dll`）。推理引擎菜单仅在这些依赖可加载时显示 TensorRT。

单张自动匹配会在进程内复用一套 Runtime 和图库特征；图库内容或图片文件变化时只刷新图库特征，模型变化时才重建 Session。“匹配所有”同样复用第一套 Runtime，并仅按“匹配线程”配置补充临时 Runtime。所有后端默认 1 线程。TensorRT 使用 FP16 engine；engine 明确缓存到应用缓存目录的版本命名空间，例如 `tensorrt-engines/ort-1.27.1_trt-10.9.0_cuda13_fp16/`，不会写入当前工作目录。首次运行仍需构建 engine，后续匹配会复用 Session 和磁盘 engine 缓存；升级 ONNX Runtime、TensorRT 或 CUDA 构建后会自然使用新的缓存目录。
