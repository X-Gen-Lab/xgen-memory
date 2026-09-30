# 来源与授权记录

allocator、pool、size_class、tracking 和 libc 后端来自 xgen-core 提交 `dc5eb1167ba21de384e7a760a8586b87502b73b2`。前缀改为 `xgm_` / `XGM_`，状态改用独立 `xgs_status_t`，头路径改为 `xgen/memory/`。

- `tests/unit/baseline.c` 保存原 `tests/test_core.c` 的 allocator、pool、size_class、tracking 回归场景；零容量档位是统一引擎的明确行为变化，期望已更新。
- tiered 的分档借用、独立缓冲、空档、所有权与失败清理由统一 size_class 新测试覆盖；不保留 tiered 名称或固定三档实现。
- arena、对齐适配验证、统计溢出与重叠保护、生命周期增强及工程消费检查是本次新实现。
- 构建、质量调用器、CI 和配置组织参考本地 xgen-bytes、xgen-crc、xgen-status；共享检查实现属于 xgen-quality。
- 排版和 EditorConfig 来源为 Nexus 提交 `7a203266082ad1f655b6686713b2b7950ba31ec6`；保留格式选项，仅调整标题与工具验证说明。来源信息集中记录于本文件。
- GoogleTest 采用上游 1.16.0 提交 `ff6133ab49b364a883a55ba75c39e520fea6245b`，作为开发依赖独立准备，保留上游许可，不安装到本组件。

原有 X-Gen Lab 作者标记保留。core 的来源记录指出原工作区缺少顶层 LICENSE；本次不虚构 SPDX、授权或复制其他仓库许可证。正式公开发布前由所有者确认授权。
