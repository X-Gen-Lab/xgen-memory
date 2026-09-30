# 本地验证记录

本次验证在 2026-10-01 执行；这些结果不代表远端 CI、发布、目标板执行或全部产品组合已经完成。

| 验证 | 结果与证据 |
| --- | --- |
| GCC 13.2.0，C11/C++17 | 22 组 GoogleTest 加 1 项实际消费集成检查，共 23 项 CTest；`out/coverage-final/Testing/Temporary/LastTest.log` |
| MSVC 19.40，C11/C++17 | 同一测试集及每个公开头的 C/C++ 独立包含；`out/msvc/Testing/Temporary/LastTest.log` |
| 源码与安装消费 | 安装完整包和 allocator-only 包；默认、可选组件、缺失组件、版本/ABI/type/identity 拒绝、重复 find_package、重复源码接入；真实 C 可执行文件运行成功 |
| 无隐式 status 依赖 | allocator-only 生产构建与安装消费者显式禁用 status 查找，验证未生成 status/pool target |
| GCC 生产覆盖 | 行 452/455（99.3%）、函数 60/60（100%）、分支 397/426（93.2%）；`out/reports/coverage-summary.json` |
| 固定工具检查 | text、clang-format 19.1.5、Cppcheck 2.21.0、clang-tidy 19.1.0、严格 Doxygen 1.16.0、pre-commit；报告在 `out/reports/quality-*.json` |

MSVC 分析在真实 VS 开发环境运行，使用对应 MSVC 编译数据库；MinGW 编译数据库搭配不完整的 Clang Windows 头文件搜索曾失败，不作为通过证据。

可达 TDD 检查点：

| RED | GREEN | 场景 |
| --- | --- | --- |
| `592d32c` | `721b725` | 旧四组回归通过，同时重现池对齐服务、统计重叠和历史累计回绕 |
| `3028639` | `b34af62` | 统一 strict/fallback、分离缓冲、空档、owned 与存活统计 |
| `6b68cad` | `b270aa9` | arena 的共享生命周期、对齐和失败原子性 |
| `e4b1b6b` | `cc44af3` | owned 装配分离、pool 单一 checked deinit |
| `6f82d6e`、`a37fc33` | `60ca5d5` | 最大对齐类型与 MSVC C++ 头自包含；缺失接口和真实编译器错误都已实际重现 |

四组原有 C 测试的 CHECK 返回值由 GoogleTest 检验，不依赖 assert，因此 NDEBUG 不会绕过回归。新增测试覆盖 owned 后端失败/错对齐回滚、统计计数饱和与 live/reset/phase、严格与借用档位、重复档位、容量耗尽、缓冲重叠、arena 回绕计算和非法对齐。

采用真实目标编译器的布局测量由集成层记录：ARM GCC 15.2.1 Cortex-M0 得到 allocator 12、pool 24、size_class 32、arena 16、tracking 40、allocator_stats 32 字节。该结果来自目标对象常量段读取，属于交叉编译证据，不是 MCU 运行或栈预算。集成证据位于 xgen-link 的 `build/independent-migration/component-layout.json`；跨仓产物不复制为本仓库的独立硬件结果。

尚未执行远端工作流、真实 MCU 运行、硬件 ISR 时限测量及 sanitizer 运行。本包没有 DMA 驱动或板级存储配置，相关产品验收由集成仓库维护。
