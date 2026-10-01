# 工程规范采用记录

采用 xgen-roadmap `docs/standards/` 的工程规范 1.0.0。本地基线提交 `9ca20873878b0de4893241d4c8c08cce01b40408`，其后同工作区已有经用户确认的文案整理。规范正文不复制到组件仓库。

生产 C11、测试 C++17/GoogleTest；4 空格、同行大括号、指针靠类型；英文反斜杠 Doxygen；普通注释使用块注释。pre-commit 调用固定的共享 xgen-quality；clang-format、clang-tidy、Cppcheck、Doxygen 与 gcovr 版本由工具包控制。

窄范围诊断例外：

| 位置 | 规则与理由 | 补偿验证与复核条件 |
| --- | --- | --- |
| pool.c 的 store_next/load_next 两次 memcpy | Windows insecureAPI 建议可选 Annex K；块容量已验证，memcpy 避免非对齐指针读取；仅这两行抑制该规则族 | 最大对齐/低对齐块和容量边界测试；改变块布局或复制长度时复核 |
| libc_allocator.c 的 libc_free | 回调 ABI 必须同时接收 context 和 block，两者都是 void* | allocator 上下文隔离、释放行为测试；改变回调契约时复核 |
| size_class_allocator.c 的 init_ex | C 枚举可与 size_t 转换；policy 显式校验，保持既有初始化参数顺序 | strict/fallback、容量失败测试；改变公开签名时复核 |

例外仅对应行或函数，不屏蔽整个文件或检查类别；维护职责随对应接口维护者，不虚构独立审批人。授权依据为用户要求完成实现并保持小 MCU、跨平台和明确接口的约束。

## 2026-10-01 空行规范采用

采用上述初始来源之后、尚未发布的工程规范 1.0.0 local 增补 C-020、C-021、DOC-013。本轮仅迁移空行和 LF，不改变生产或测试代码行为。根 `.clang-format` 对应受控模板 SHA-256 `ffdb331b03ae4f6c5f75ee54d4afaa6d4741f5ac3ec57f55b0f04ad8ec396a2a`；配置来源和实际增补见 [来源记录](../PROVENANCE.md)。

`python tools/quality.py format` 是只读检查入口，由固定 clang-format 19.1.5 检查排版，并由共享工具检查其支持的公共 C 头结构分隔、独立 API 注释与声明组，以及 Doxygen 紧邻直接声明的形式。它不声称覆盖任意 C++ AST；条件包装、复杂宏、函数内语义阶段和字段分组仍需人工评审。

共享工具当前版本和固定源码来源只在 [tools/quality.json](../tools/quality.json) 维护；初始规范提交不代表已经包含这次增补。采用记录与实际执行证据分开，本次同步不声明远端 CI 已运行。
