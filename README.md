# xgen-memory

面向 MCU、Boot 和主机工具的独立 C11 内存组件。调用者明确选择存储、容量、生命周期和后端，不存在默认堆、内部线程或全局可变分配状态。

| Target | 公开头 | 作用与依赖 |
| --- | --- | --- |
| `xgm::allocator` | `xgen/memory/allocator.h` | 显式分配接口；无组件依赖 |
| `xgm::pool` | `xgen/memory/pool.h` | 固定块池；allocator、status |
| `xgm::size_class` | `xgen/memory/size_class_allocator.h` | 严格分档、向大档借用；pool |
| `xgm::arena` | `xgen/memory/arena.h` | 一组对象共享生命周期；仅 status |
| `xgm::tracking` | `xgen/memory/tracking_allocator.h` | 可选请求字节与阶段统计；allocator、status |
| `xgm::libc_allocator` | `xgen/memory/libc_allocator.h` | 显式 malloc/free 后端；allocator |

`XGM_COMPONENTS` 控制构建目标，自动补齐组件内部依赖。默认构建除 libc 后端以外的五项；仅选择 allocator 不查找 status。开启 `XGM_BUILD_TESTS` 明确构建全部六项用于主机验证。安装包按 `COMPONENTS` 加载，未指定组件时只加载 allocator。

安装消费：

```cmake
find_package(xgen_memory 0.1.0 EXACT CONFIG REQUIRED COMPONENTS pool arena)
target_link_libraries(firmware PRIVATE xgm::pool xgm::arena)
```

源码消费由产品提供已验证的 `xgs::status`，或者通过 `XGM_STATUS_SOURCE_DIR` 指定 checkout，或者安装 `xgen_status 0.1.0`。配置与编译不下载依赖，不搜索固定兄弟目录。生产只启用 C；GoogleTest 使用 C++17。

固定池示例：

```c
#include <xgen/memory/pool.h>

int example(void) {
    _Alignas(xgm_max_align_t) unsigned char storage[128];
    xgm_pool_t pool;
    if (xgm_pool_init(&pool, storage, sizeof(storage), 32,
                      _Alignof(xgm_max_align_t), 4) != XGS_OK) {
        return 1;
    }
    void* block = xgm_pool_alloc(&pool);
    if (block == NULL) {
        return 2;
    }
    if (xgm_pool_free(&pool, block) != XGS_OK) {
        return 3;
    }
    return xgm_pool_deinit(&pool) == XGS_OK ? 0 : 4;
}
```

使用前阅读 [接口与资源契约](docs/contracts.md)、[迁移说明](docs/migration.md) 和 [验证记录](docs/validation.md)。工程流程见 [贡献指南](CONTRIBUTING.md)，来源见 [PROVENANCE.md](PROVENANCE.md)。实际 MCU 镜像、栈和板级运行由消费者继续验证。
