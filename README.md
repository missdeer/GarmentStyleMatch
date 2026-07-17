# GarmentStyleMatch

服装实拍图与款号手绘图的浏览、匹配和确认工具。应用以三栏工作区组织输入实拍图、当前图片与属性、PowerPoint 款号图库，并将匹配结果持久化到输入目录。

当前版本号：`0.1.0`。

## 当前功能

- 递归扫描实拍图目录，扫描已分类输出目录，并提供列表筛选、图片翻页和原图打开。
- 展示文件、图像、IPTC 和 EXIF 信息；输出目录中的多张图片以缩略图切换。
- 在 Windows 上通过 Microsoft PowerPoint 导出 `.pptx` 页面预览，记住各文件的页面选择，并在后台从所选页面提取款号和手绘图。
- 按 `baby`、`kids`、`adult` 分类或款号关键词筛选提取出的款号小图库。
- 使用 SegFormer B2 Clothes 分割上衣/裤裙区域，再用 FashionCLIP 图像特征和余弦相似度自动匹配款号。
- 支持匹配当前实拍图、批量匹配全部实拍图、停止批量任务，以及为批量任务配置 1–8 个匹配线程。
- 上衣和裤裙结果可分别确认或删除；可跳转到上一张/下一张未匹配或未确认图片。
- 可在当前图片和相邻图片之间复制全部、上衣或裤裙款号；目标已有已确认款号时可选择全部覆盖、仅覆盖未确认项或取消。
- 将匹配与确认状态保存到实拍图目录下的 `gsm.db`，并在相邻预览及输入列表中显示状态。
- 记住输入/输出目录、PPT 路径和页面选择、当前索引、界面风格、推理引擎及批量线程数。
- 三栏工作区及中间上下区域均可拖动调整大小。

## 平台与技术栈

- C++20、CMake 3.21+
- Qt 6.10+：Concurrent、Core、Gui、Network、Qml、Quick、QuickControls2、Widgets；Windows 额外使用 AxContainer
- LibArchive：读取 `.pptx` Open XML/ZIP 内容
- OpenCV：自动匹配的图像预处理
- SQLite：匹配结果和款号图库特征缓存
- ONNX Runtime：CPU、DirectML、CUDA、TensorRT 和 Windows ML 推理后端

Windows 提供完整功能；PPT 页面预览依赖本机安装的 Microsoft PowerPoint。macOS arm64 和 Linux x64/aarch64 使用 CPU 推理，且当前不支持 PPT 页面预览。ONNX Runtime 1.24.4 没有官方 macOS x64 发布资产，Intel Mac 需要手动提供 `GSM_ONNXRUNTIME_ROOT`。

第三方 C++ 依赖由根目录的 [`vcpkg.json`](vcpkg.json) 声明。ONNX Runtime 和 Windows ML SDK/运行时由 `3rdparty/CMakeLists.txt` 在配置阶段按平台下载并校验，下载临时文件只写入项目的 `tmp/`。

## 目录结构

```text
.
├── CMakeLists.txt          根 CMake 工程
├── vcpkg.json              LibArchive、OpenCV、SQLite 依赖清单
├── src/
│   ├── main.cpp            应用入口与 QML 上下文对象注册
│   ├── core/               控制器、列表模型、PPT 提取、匹配与持久化
│   └── qml/                主界面与可复用 QML 组件
├── tests/                  CTest 测试
├── scripts/                Runtime/模型准备脚本
├── 3rdparty/               CMake 配置及本地下载的 Runtime/模型
├── doc/                    设计资料
└── install/                默认本地安装输出目录
```

`cmake-msvc-build/`、`cmake-macos-build/`、根目录的 `compile_commands.json`、`tmp/` 和填充后的 `install/` 都是生成内容，不应手工编辑。

## Windows 开发构建

当前开发机使用 MSVC 2022、Ninja、Qt 6.10+ 和 vcpkg，并始终复用 `cmake-msvc-build/`，不要另建 `build/`。修改 `CMakeLists.txt`、依赖或第三方配置后先重新配置：

```bash
bash -lc "cmake-reconfigure cmake-msvc-build"
```

构建：

```bash
bash -lc "cmake-build cmake-msvc-build"
```

运行全部已注册测试：

```bash
ctest --test-dir cmake-msvc-build --output-on-failure
```

没有模型时会注册 7 个基础测试；准备好模型后还会注册推理运行时测试。当前完整 Windows 配置共注册 13 个测试，可用以下命令核对实际列表：

```bash
ctest --test-dir cmake-msvc-build -N
```

运行应用：

```powershell
.\cmake-msvc-build\bin\GarmentStyleMatch.exe
```

如果应用正在运行，Windows 会锁定输出文件并导致链接器报 `LNK1168`；重新构建前需先关闭应用。

## macOS 开发构建

macOS 上使用独立的 `cmake-macos-build/` 目录，避免与 Windows 的 `cmake-msvc-build/` 混淆。前置条件：

- Xcode Command Line Tools（`xcode-select --install`）
- Qt 6.10+，通过官方 online installer 或 `aqt` 安装
- vcpkg，例如安装在 `$HOME/vcpkg`（`git clone https://github.com/microsoft/vcpkg.git $HOME/vcpkg && $HOME/vcpkg/bootstrap-vcpkg.sh -disableMetrics`）

首次配置会通过 vcpkg 编译 LibArchive、OpenCV（core/imgcodecs/imgproc）和 SQLite，用时较久：

```bash
cmake -S . -B cmake-macos-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_PREFIX_PATH=$HOME/Qt/6.10.0/macos/lib/cmake
```

构建与测试：

```bash
cmake --build cmake-macos-build
ctest --test-dir cmake-macos-build --output-on-failure
```

运行：

```bash
./cmake-macos-build/bin/GarmentStyleMatch.app/Contents/MacOS/GarmentStyleMatch
```

macOS 使用 CPU 推理，且不支持 PPT 页面预览。ONNX Runtime 1.24.4 没有官方 macOS x64 发布资产，Intel Mac 需在配置时通过 `-DGSM_ONNXRUNTIME_ROOT=<path>` 手动指定。

开发目录中的 `qt.conf` 会让应用直接使用检测到的 Qt 安装。生成包含 Qt QML 模块、插件、推理运行时和可用模型的本地可分发目录：

```bash
cmake --install cmake-msvc-build --prefix install
```

安装结果位于 `install/bin/`。

## 准备自动匹配模型

应用没有模型也可以构建和浏览图片/PPT。首次使用自动匹配前，需要准备以下两个最终模型文件：

- `clothes_segformer_b2.onnx`：SegFormer B2 Clothes 分割模型。
- `fashion_clip_vision.onnx`：从 FashionCLIP 提取的图像编码器。

最直接的方式是在应用顶部打开“下载模型”菜单。应用会顺序下载并校验源模型、显示进度、支持停止下载，并在覆盖已有文件前请求确认；随后使用本机 Python 将 FashionCLIP 图像编码器提取到应用本地数据目录。该方式需要可用的 Python 3 和 `pip`。

也可以在 PowerShell 中从源码树运行只准备模型的脚本：

```powershell
.\scripts\download-models.ps1
```

脚本会校验 SHA-256，并把源码树中的模型写入被 Git 忽略的 `3rdparty/models/`。如果模型是在 CMake 配置后才准备的，需要重新配置并构建，模型才会复制到可执行文件目录并启用运行时测试。安装包的 `bin/scripts/` 中也会包含该脚本，此时模型写入安装包的 `bin/models/`。

要在 Windows 上一次性提前准备 ONNX Runtime 1.27.1 CUDA 13、ONNX Runtime 1.24.4 DirectML 和两个模型，可运行：

```powershell
.\scripts\setup-onnxruntime.ps1
```

该脚本同样校验下载内容，并把结果部署到被 Git 忽略的 `3rdparty/`，从而避免后续 CMake 配置重复下载这些 Runtime。它也需要 Python 3 和 `pip` 来提取 FashionCLIP 图像编码器。

## 推理后端

Windows 自动选择顺序为 TensorRT、CUDA、DirectML、CPU。顶部“推理引擎”菜单还可以显式选择可用后端；一次进程生命周期只加载启动时选定的一套 ONNX Runtime，因此修改推理引擎后必须重启应用。

- TensorRT：需要 NVIDIA 驱动、TensorRT 10、CUDA 13 和 cuDNN 9。可用 `TENSORRT_ROOT` 指向 TensorRT 安装目录，其 `lib/` 中应包含 `nvinfer_10.dll` 和 `nvonnxparser_10.dll`。Session 创建失败时依次回退 CUDA 和 CPU。
- CUDA：使用随工程准备的 ONNX Runtime 1.27.1 CUDA 13 Runtime。
- DirectML：使用 ONNX Runtime 1.24.4 DirectML Runtime，适用于其他 Windows GPU。
- Windows ML：内置 CPU/DirectML 选项；“Windows ML EP”面板还可查询和安装本机兼容的动态执行提供程序。动态 EP 需要 Windows 11 24H2 或更高版本及兼容驱动，下载由 Windows Update 完成。
- CPU：Windows 复用已部署的 ONNX Runtime，macOS/Linux 使用官方 CPU Runtime。

单张和批量匹配会在进程内复用 Runtime 与图库特征。图库图片变化时只刷新相关特征，模型、Runtime 版本或推理后端变化时对应特征会自动重建。特征保存在应用缓存目录的 `style_embeddings.sqlite`。

TensorRT 使用 FP16 engine，并将 engine 缓存写入应用缓存目录中的版本命名空间，例如 `tensorrt-engines/ort-1.27.1_trt-<TensorRT版本>_cuda13_fp16/`。首次运行需要构建 engine；升级 ONNX Runtime、TensorRT 或 CUDA 构建后会自然使用新的缓存目录，不会污染当前工作目录。

## 核心架构

应用采用“C++ 控制器/列表模型 + QML 展示”的结构，业务逻辑不放在 QML 中：

- `MatchController`：路径、选择、导航、PPT 缓存与提取、自动匹配、确认、持久化和 UI 状态的中心控制器。
- `PhotoListModel` / `CandidateListModel`：输入实拍图和分类输出目录。
- `PptPageListModel` / `GalleryListModel`：PPT 页面选择及提取后的款号小图库。
- `ImageMetadata`：文件、图像、IPTC 和 EXIF 信息。
- `PptStyleExtractor`：通过 LibArchive 读取 `.pptx`，按所选页面提取款号与图片。
- `GarmentMatcher`：服装分割、FashionCLIP 编码、相似度匹配、Runtime 和特征缓存。
- `MatchResultStore`：将上衣/裤裙款号及确认状态保存到输入目录的 `gsm.db`。

`src/main.cpp` 在栈上创建这些对象并通过 `QQmlContext` 暴露给 QML，最后使用 `engine.loadFromModule("GarmentStyleMatch", "Main")` 加载界面。
