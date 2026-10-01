# 来源与授权记录

allocator、pool、size_class、tracking 和 libc 后端来自 xgen-core 提交 `dc5eb1167ba21de384e7a760a8586b87502b73b2`。前缀改为 `xgm_` / `XGM_`，状态改用独立 `xgs_status_t`，头路径改为 `xgen/memory/`。

- `tests/unit/baseline.c` 保存原 `tests/test_core.c` 的 allocator、pool、size_class、tracking 回归场景；零容量档位是统一引擎的明确行为变化，期望已更新。
- tiered 的分档借用、独立缓冲、空档、所有权与失败清理由统一 size_class 新测试覆盖；不保留 tiered 名称或固定三档实现。
- arena、对齐适配验证、统计溢出与重叠保护、生命周期增强及工程消费检查是本次新实现。
- 构建、质量调用器、CI 和配置组织参考本地 xgen-bytes、xgen-crc、xgen-status；共享检查实现属于 xgen-quality。
- 排版和 EditorConfig 的初始来源为 Nexus 提交 `7a203266082ad1f655b6686713b2b7950ba31ec6`；初次受控副本保留格式选项，仅调整标题与工具验证说明。后续空行增补另记，不宣称当前配置与初始来源全部选项相同。
- GoogleTest 采用上游 1.16.0 提交 `ff6133ab49b364a883a55ba75c39e520fea6245b`，作为开发依赖独立准备，保留上游许可，不安装到本组件。

原有 X-Gen Lab 作者标记保留。core 的来源记录指出原工作区缺少顶层 LICENSE；本次不虚构 SPDX、授权或复制其他仓库许可证。正式公开发布前由所有者确认授权。

## 2026-10-01 格式配置增补

按 xgen-roadmap 尚未发布的工程规范 1.0.0 local 增补 C-020、C-021、DOC-013，受控 `.clang-format` 增加 `SeparateDefinitionBlocks: Always`、`KeepEmptyLines` 三项 false 和 `LineEnding: LF`，保留 `MaxEmptyLinesToKeep: 1`。当前格式模板 SHA-256 为 `ffdb331b03ae4f6c5f75ee54d4afaa6d4741f5ac3ec57f55b0f04ad8ec396a2a`；`.editorconfig` 的来源不变。

本轮源码迁移仅调整空行和 LF，不改变代码行为；来源、作者和许可证事实保持。共享质量工具的当前固定来源见 [tools/quality.json](tools/quality.json)，格式检查边界见 [规范采用记录](docs/standards.md)。这份记录不代表远端 CI 已执行。
