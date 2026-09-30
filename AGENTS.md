# 仓库工作约定

- 始终使用简体中文沟通，修改前阅读 CONTRIBUTING.md、docs/contracts.md 和 PROVENANCE.md。
- 生产代码为 C11；公开头同时支持 C/C++ 独立包含。保持受控排版、英文反斜杠 Doxygen 和普通块注释。
- 六项能力独立 target；allocator 不依赖 status，arena 不依赖 allocator，libc 后端显式可选。
- 不重新引入 tiered 实现或别名包装。strict/fallback 共用 size_class 引擎；拥有存储的装配对象与借用状态分离。
- 新行为和缺陷修复遵循 TDD，保留真实 RED/GREEN checkpoint；测试使用 GoogleTest 链接生产 C target。
- 存储所有权、失败状态、并发责任和最坏工作量属于接口契约，不凭通用名称宣称 O(1) 或 ISR 安全。
- 质量检查使用 tools/quality.py 的共享实现，禁止复制通用 runner 或隐式下载依赖。
- 本地、远端 CI、硬件执行和发布证据分别记录；保护工作区中的其他修改。
