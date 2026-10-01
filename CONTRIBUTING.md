# 贡献指南

先阅读 [规范采用记录](docs/standards.md)、[接口契约](docs/contracts.md) 和 [来源记录](PROVENANCE.md)。代码行为采用 TDD；先写有意义的 GoogleTest 并运行到预期失败，再实现、回归和整理。测试链接真实 C target，不复制算法作为期望值。已有 C 回归通过 GoogleTest 调用，CHECK 返回错误，Release/NDEBUG 不会将其移除。

独立准备 GoogleTest 1.16.0、xgen-status 0.1.0 和 xgen-quality 0.1.0。GoogleTest 的源码提交固定为 `ff6133ab49b364a883a55ba75c39e520fea6245b`。工具来源固定在 `tools/quality.json`。构建不会替开发者下载或更新它们。

```sh
cmake -S . -B out/host -G Ninja -DXGM_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DXGM_STATUS_SOURCE_DIR=/path/to/xgen-status -DCMAKE_PREFIX_PATH=/path/to/gtest-install
cmake --build out/host
ctest --test-dir out/host --output-on-failure
python tools/quality.py text
python tools/quality.py format
python tools/quality.py cppcheck --build-dir out/host
python tools/quality.py tidy --build-dir out/host
python tools/quality.py docs
python -m pre_commit run --all-files
```

GCC 覆盖构建另外设置 `-DXGM_ENABLE_COVERAGE=ON`，运行完整测试后执行 `python tools/quality.py coverage --build-dir out/host`；GCOV 可通过 `--gcov-executable` 显式指定。行、函数、分支覆盖均受共享规范的最低阈值约束。

Windows 使用对应编译器的开发环境：MSVC 要先启用 VS Developer Command Prompt；GCC 使用独立安装的 GCC/G++ 与对应 GoogleTest。clang-tidy 需要与编译数据库一致的目标头文件环境，不能将 MSVC CRT 与 MinGW 头混用。`XGEN_CLANG_TIDY` 等工具路径覆盖只选择已固定版本，不改变规则。

CI 显式从 `X-Gen-Lab/xgen-quality` 获取配置中固定的工具提交，允许用 `XGEN_QUALITY_REPOSITORY` 覆盖为受控镜像。状态组件同样默认从 `X-Gen-Lab/xgen-status` 获取固定提交，可用 `XGEN_STATUS_REPOSITORY` 覆盖。仓库覆盖、固定提交不可获取或安装版本不符时失败；不跟随依赖主分支，远端验证与本地结果分别记录。
