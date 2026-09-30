# 接口与资源契约

完整函数参数与失败行为以公开头为准。所有状态对象与借用存储保持固定地址；调用者负责串行化，禁止在后台 DMA 仍访问对象时重置其存储。组件不会自动创建线程、堆或锁。后端回调同步执行，能否在 ISR 调用由其最坏时间和平台规则决定。

| 能力 | 生命周期 | 资源与复杂度 |
| --- | --- | --- |
| allocator | 同一服务申请和释放；上下文覆盖所有分配 | 描述符含 context 和两回调；时限取决于后端 |
| pool | 调用者持有块存储；有存活块时 deinit 返回 BUSY | 空闲块内保存 next；初始化 O(N)、分配 O(1)、重复释放校验 O(N) |
| size_class | 描述符由引擎独占；禁止外部直接调用 pool 操作 | 分配 O(档数)，释放 O(档数 + 所属池块数)，无单块头 |
| arena | 只能整体 reset/deinit，所有旧指针同时失效 | O(1) 分配；对齐填充计入 used；没有虚假 free 适配 |
| tracking | 所有统计区、上下文必须互不重叠并覆盖分配寿命 | 每块前缀；分配释放除后端外 O(1)，初始化/重置 O(阶段数) |
| libc | 明确选择 malloc/free 后端 | 主机或允许堆的平台使用，不作为 Boot 默认 |

pool 接受明确的块对齐，通用 allocator 适配只有存储起点和块步长都满足最大标量对齐才有效。`xgm_max_align_t` 在标准编译器上对应 C/C++ 最大对齐；对 MSVC 缺失 C `max_align_t` 的环境提供 ABI 一致的类型。包含头文件不要求提前包含 GoogleTest 或 C++ 标准库。

size_class 的 strict 模式在最小适合档耗尽时失败；fallback 从剩余容量中选择最小适合档。规格不要求排序，重复大小按输入顺序尝试，零数量档禁用。借用模式既支持连续工作区，也支持相互不重叠的分离缓冲。measure 只计算块存储，不包含状态或池描述符。

拥有存储时使用 `xgm_size_class_owned_t`，一次向显式 backend 申请连续块存储。其 allocator 成员仍是同一引擎；backend 与拥有的指针只出现在装配对象中，静态用户不承担这些字段。通过 allocator.service 分配，通过 deinit_owned 释放整个装配对象。失败不发布半初始化状态。

size_class 的 used_memory/peak_memory 记录对齐后块字节与真实同时峰值，不包括状态、池描述符及后端舍入。tracking 的统计记录业务请求字节；`xgm_tracking_overhead()` 返回每块前缀，后端实际保留字节仍由后端确定。历史计数饱和并设置 saturated；当前存活量保持精确。reset 保留 live_blocks，因此 alloc_count - free_count 不能用于推算存活块数。

pool contains 只表示合法块边界，不表示当前已分配。tracking 的 magic 不构成任意地址检测；外部地址、块内指针和重复释放不属于有效输入。

本仓库不承诺所有配置可满足 64 KB Flash / 8 KB RAM。产品必须用最终工具链测量完整 ELF、静态数据、峰值栈及后端缓冲，并将 DMA 可访问性、缓存维护和中断时限纳入板级验收。
