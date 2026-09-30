# 从 xgen-core 迁移

只迁移一次实现，不在消费者中保留重复源文件或 `xgc_` 转发层。include 从 `xgc/` 改为 `xgen/memory/`，状态 include 使用 `xgen/status/status.h`。公开 allocator 类型改变需要一起迁移消费接口和版本说明。

| 原行为 | 新行为 |
| --- | --- |
| size_class 固定 strict | 原 init 仍 strict；init_ex 显式选择 fallback |
| tiered 三个固定档 | 统一规格数组；64/256/1024 可作为配置，不再是实现限制 |
| tiered 分离缓冲 | init_buffers 提供各档存储；最大对齐与重叠在初始化校验 |
| tiered 逐档申请并部分回滚 | owned 装配一次申请连续块存储；失败不发布状态 |
| tiered 强制销毁活对象 | deinit_owned 返回 BUSY，调用者先归还活对象 |
| 各档历史峰值相加 | size_class 真正同时占用峰值；不兼容旧峰值数值含义 |
| size_class 不接受零数量档 | 统一引擎将零数量档视为禁用；count 本身仍须非零 |
| pool void deinit 无条件失效 | deinit 返回 status；存活块时返回 BUSY、状态不变 |
| pool 对齐只靠调用前提 | 创建 allocator 描述符时拒绝不足的起点或步长对齐 |
| tracking 历史计数回绕 | 饱和、saturated 标志；live_blocks 精确且不被 reset 清零 |

四组原有回归被保留；新增生命周期和 tiered 收敛场景链接真实 C target。旧 tiered 的强制销毁、任意字节对齐和峰值求和不再是本包保证，不通过别名伪装兼容。
