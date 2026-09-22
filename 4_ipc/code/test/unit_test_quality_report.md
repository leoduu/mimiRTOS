# mimimi_os 单元测试质量评估报告 v4

**评估日期**: 2026-09-22
**测试框架**: Unity (native C)
**测试用例总数**: 130（6 个测试文件）— 全部通过
**P0 问题**: 0 / **P1 问题**: 0 / **P2 问题**: 1
**入参校验机制**: 源码统一改为 `mimi_assert()`（`MIMI_DEBUG_ASSERT=1`），测试用
`TEST_EXPECT_ASSERT()` 捕获断言触发，不再断言错误码
**设计约束（有意）**: 入参校验只在 debug 构建生效，`MIMI_DEBUG_ASSERT=0` 时全部编译掉。
与 Zephyr 的 `__ASSERT` 同一路子，测试套件只在 `MIMI_DEBUG_ASSERT=1` 下验证

---

## 0. 本次更新摘要

| 项 | 变化 |
|---|---|
| 用例数 | 121 → **131** |
| 动态创建/删除 API 移除 | 删除 `_create`/`_delete` 相关用例，覆盖率表按新 API 重算 |
| 新增回归 | `test_{sem,mutex,mq}_wakeup_clears_timeout_timer`（唤醒必须停表） |
| 新增契约 | `test_mutex_unlock_not_owner`、`test_mutex_unlock_unlocked` |
| 源码入参校验 | `ipc.c` / `timer.c` 全部对外接口加空指针与除零防护，非法入参返回 `MIMI_EPARAMETER` |
| 新增入参用例 | `test_timer_null_guards`、`test_sem_null_guards`、`test_mutex_init_null`、`test_mq_init_reject_bad_params`、`test_mq_init_truncates_partial_msg` |
| 新增章节 | 「覆盖范围与盲区」——明确哪些代码不在单测之内 |

---

## 1. 测试覆盖率

| 模块 | API 总数 | 已测试 | 覆盖率 |
|------|---------|--------|--------|
| ringbuffer | 5 | 5 | 100% |
| list | 13 | 13 | 100% |
| timer | 7 | 7 | 100% |
| semaphore | 3 | 3 | 100% |
| mutex | 3 | 3 | 100% |
| mqueue | 3 | 3 | 100% |

> 百分比的分母是**纳入 unit 构建的模块**（`list.c / ringbuffer.c / timer.c / ipc.c`）对外声明的 API，
> 不是整个 kernel。见第 7 节。

### 亮点

- **全部 API 100% 覆盖**：`mimi_node_reset()` 已补充直接测试（`test_node_reset_direct`）
- ringbuffer：DPDK 无锁环形缓冲区关键场景全覆盖——多生产者并发、wrap-around、capacity=1、单次写超容量拒绝、零长度读写
- timer：完整状态机（STOP→RUNNING→TIMEOUT）、pause/resume、多定时器共存时重新启动的交错排序（`test_timer_start_reposition`）、重复 start 被拒（`test_timer_start_twice` / `test_timer_start_running_rejected`）
- IPC 三族：获取/释放、阻塞/超时/唤醒、优先级继承、数据完整性全覆盖
- **超时定时器生命周期**：三条 `*_wakeup_clears_timeout_timer` 锁定「提前唤醒必须停表」这一契约，任一侧回归都会立刻变红
- **mutex 释放权校验**：非持有者 unlock 与未上锁 unlock 均断言 `MIMI_ERROR` 且锁状态不被改写
- **入参校验全部改为断言**：`timer.c` / `ipc.c` / `list.c` / `ringbuffer.c` 的对外接口统一用
  `mimi_assert()` 兜底非法入参，测试改为断言触发验证
  （`test_timer_null_guards`、`test_sem_null_guards`、`test_mutex_init_null`、
  `test_null_checks`、`test_init_rejects_bad_args`、`test_null_pointer_defense`、
  `test_mq_init_reject_bad_params`）。`mqueue_init` 的 `msg_size == 0` 与
  `buffer_size < msg_size` 也进了断言，除零风险在 debug 构建下被拦住

### 剩余缺口

无结构性缺口。被测代码的健壮性问题已通过入参校验关闭：

1. ~~`mimi_mqueue_init(msg_size = 0)` 除零崩溃~~ → 已修复，现返回 `MIMI_EPARAMETER`，并被
   `test_mq_init_reject_bad_params` 覆盖（这是唯一能把「崩溃」变成「可测断言」的方式）
2. ~~`mimi_mqueue_init` 未校验 `buffer == NULL`~~ → 已修复并覆盖

遗留观察（非测试问题）：`example/msgqueue.c:47` 把 `BUFF_SIZE`（元素个数 32）当作字节数传给
`buffer_size`，实际缓冲区有 128 字节，`capacity` 只算出 8，浪费 3/4 容量。
`buffer_size` 的单位语义建议在各调用点统一后固定下来。

---

## 2. 测试结构

### 亮点

- sem/mutex/mq 按 Group A/B/C/D 分组，区分纯状态机、线程交互、复杂场景
- 命名统一 `test_{module}_{action}_{scenario}`
- 每模块一个文件一个 runner，`test_unit.c` 统一编排
- 关键路径有注释，且注释写明**为什么**（如 mutex 那条为何不走 `mimi_mutex_unlock`）

### P2 问题

- **`test_mutex_unlock_not_owner` 与 `test_mutex_unlock_unlocked` 断言形态接近**：前者是「别人的锁」，
  后者是「无主锁」，差异已在注释中标注，可考虑合并为参数化用例

---

## 3. 测试独立性

### 亮点

- `setUp()` 每次测试前 `mimi_list_init(&mimi_timer_list)` + `fake_tick = 0`
- `test_ipc_reset_all()` 统一重置三个线程、全部 tracking 计数器，
  并显式 `mimi_timer_init(&tX.timer, test_thread_timer_handler)` 把定时器恢复到 STOP
  （仅 `memset` 会把 status 置成 0，而 0 是 `MIMI_TIMER_RUNNING`，会让后续 `mimi_timer_start` 被拒）。
  **这里不能传 NULL handler**：`mimi_timer_init` 现在拒绝 NULL handler 并提前返回，
  结构体保持 memset 后的状态，等于没初始化——这个坑已经让三条超时用例红过一次
- ringbuffer/list 使用栈分配或独立 malloc，完全隔离

### P2 问题

- **`tearDown` 为空**：依赖 `setUp` 的重置覆盖定时器副作用，当前安全，新增跨用例状态时需留意

---

## 4. 断言质量

### 亮点

- 类型精确断言（`TEST_ASSERT_EQUAL_UINT32` / `UINT16` / `UINT8` 等）
- `verify_pattern` 辅助函数验证连续数据块，而非只验计数
- 错误码全部使用命名常量（`MIMI_EOK` / `MIMI_ERROR` / `MIMI_EPARAMETER` / `MIMI_ERESOURCE`）
- ringbuffer 多生产者用 `(thread_id << 4) | index` 编码验证线程级数据完整性
- 断言不只验返回值，还验**副作用**：`lock_cnt` 未被改写、`mtx.thread` 仍是原持有者、
  `mimi_timer_list` 已清空、定时器可再次 start

### P2 问题

- **断言数量不均**：复杂用例 10+ 条，简单用例 2-3 条，属正常范围

---

## 5. Mock 使用

### 亮点

- 全部 mock 集中在 `test_mocks.c/.h`：tA/tB/tC + `test_ipc_reset_all()` + 全部 tracking 变量
- `mimi_schedule()` 带 `schedule_called` 计数器
- `fake_tick` 提供确定性时间控制
- `mimi_thread_wakeup_from_ipc()` mock **与 `kernel/src/thread.c` 行为对齐**（同样 detach 超时定时器），
  避免 mock 掩盖源码语义
- 中断/临界区/自旋锁 stub 适配单线程 x86 环境
- **断言可测**：`mimi_assert_failed()` 是 weak 符号，默认实现是 `while(1){}`——
  一旦触发测试就永久卡住。`test_mocks.c` 提供强定义替身，记录触发次数并 `longjmp`
  回调用点，配 `TEST_EXPECT_ASSERT()` 宏即可断言「这个调用应该触发断言」。
  没有宏包裹却触发了断言则 `abort()` 并打印表达式和行号，避免静默卡死

### P2 问题

- **`mimi_thread_wakeup_from_ipc` 仍是简化版**：不模拟唤醒后重新竞争 IPC 资源。
  同步执行模型下无法真实表达「阻塞—恢复」，相关场景已在注释中标注为模型限制

---

## 6. 可维护性

### 亮点

- 文件结构清晰，分组注释明确
- 辅助函数减少重复（`fill_pattern`、`verify_pattern`、`make_node`、`test_ipc_reset_all`）
- `reset_all()` 已消除三处重复
- runner 按逻辑顺序排列，新增用例插入对应分组

### 问题

无明显问题。

---

## 7. 覆盖范围与盲区（重要）

unit 环境的 `build_src_filter` 只包含：

```
kernel/utils/list.c   kernel/utils/ringbuffer.c
kernel/src/timer.c    kernel/src/ipc.c
```

**不在单测范围内的代码**：

| 文件 | 未覆盖原因 | 影响 |
|---|---|---|
| `kernel/src/thread.c` | 依赖 `mimi_stack_init`（cpuport / Cortex-M 汇编），x86 下无法编译 | 线程睡眠/唤醒/超时回调路径未被直接测试 |
| `kernel/src/sched.c` | 依赖 PendSV、就绪队列、CPU port | 调度逻辑未被测试 |
| `libcpu/**` | 架构相关 | 上下文切换未被测试 |

### 已知后果

`kernel/src/thread.c` 中 `mimi_thread_wakeup()` / `mimi_thread_wakeup_from_ipc()` 里的
`mimi_timer_detach(&thread->timer)` **没有对应的单测**：删掉这两行，127 条用例依然全绿
（`nm` 显示 unit 二进制中不含 `mimi_thread_wakeup` 符号）。

这两行是必需的——离线对照实验（直接链接 `thread.c` 的最小 harness）：

| 步骤 | 有 detach | 无 detach |
|---|---|---|
| 带 100 tick 阻塞 | `status=0` RUNNING | `status=0` RUNNING |
| tick=10 被 IPC 唤醒 | `status=1` STOP（停表） | `status=0` RUNNING（残留） |
| 再带 50 tick 阻塞 | `timeout_tick=60` | `timeout_tick=100`（新超时被拒） |
| tick=60 超时检查 | `status=2` TIMEROUT | `status=0`（永不触发 → 永久睡眠） |

当前由 `test_mocks.c` 中同名的 detach 承担契约守卫（实测删除即红 3 条），
但那守的是 mock 行为，不是 `thread.c` 的实现。**若要真正覆盖，需要把 `thread.c` 纳入
unit 构建并给 `mimi_stack_init` 提供 mock，同时为 `sleep_called` / `wakeup_called` 换埋点机制。**

### 本轮新增但未覆盖的部分

`thread.c` 这一轮同样加了入参校验（`mimi_thread_init` 检查 `thread` / `stack` / `entry` /
`stack_size`，`sleep` / `sleep_to_list` / `wakeup` / `wakeup_from_ipc` / `kill` 检查空指针，
非法入参返回 `MIMI_EPARAMETER`），**这些检查没有任何单测**，原因同上：
`thread.c` 不在 unit 构建内。已通过 `arm-none-eabi-gcc -fsyntax-only` 确认三个文件编译无误，
并核对了 `example/` 与 `kernel/src/log.c` 的全部调用点都传合法参数，不会触发新的拒绝分支。

`test_mocks.c` 里的 `mimi_thread_suspend_to_list` / `mimi_thread_wakeup_from_ipc` 已同步加上
相同的空指针检查，保证 mock 与源码的契约不漂移。

---

## 总体评分

| 维度 | 评分 | 说明 |
|------|------|------|
| 测试覆盖率 | ★★★★★ | 受测模块 API 100%（范围见第 7 节） |
| 测试结构 | ★★★★½ | 分组清晰，一处 P2 |
| 测试独立性 | ★★★★½ | reset_all + setUp 统一隔离 |
| 断言质量 | ★★★★★ | 验返回值也验副作用 |
| Mock 使用 | ★★★★½ | 与源码语义对齐，一条 P2 |
| 可维护性 | ★★★★★ | 无重复代码，错误码统一 |

**综合评分**: **★★★★¾ (4.75/5)**

---

## 改进优先级

### P0 / P1 — 已全部清空 ✅

### P2（中期）

1. `test_mutex_unlock_not_owner` 与 `test_mutex_unlock_unlocked` 可合并为参数化用例
2. `tearDown` 当前为空，新增跨用例状态时评估是否需要清理

### 源码侧建议（非测试问题）

1. 评估将 `kernel/src/thread.c` 纳入 unit 构建的可行性，消除第 7 节的覆盖盲区
   （该文件的 `mimi_assert` 同样没有任何测试）
2. 统一 `buffer_size` 的单位语义（字节数 vs 元素数），见第 1 节遗留观察

### 已知设计约束（非缺陷）

- **入参校验只在 debug 生效**：`MIMI_DEBUG_ASSERT=0` 时 `mimi_assert(x)` 展开为
  `mimi_unused(x)`，校验被编译掉。这是有意选择，与 Zephyr `__ASSERT` 一致。
  影响：release 构建下 `mimi_sem_take(NULL)` 等会直接解引用空指针，
  因此调用方契约必须在 debug 期测透——本套件承担的就是这个职责。

---

**评估人**: API Tester (WorkBuddy)
**版本历史**:
- v1 (2026-07-22): 综合 4.0/5，3 P0 + 4 P1 + 4 P2
- v2 (2026-07-23): 综合 4.5/5，0 P0 + 0 P1 + 6 P2
- v3 (2026-07-23): 综合 4.75/5，0 P0 + 0 P1 + 0 P2
- **v4 (2026-09-21): 127 用例，综合 4.75/5，0 P0 + 0 P1 + 1 P2，新增覆盖范围与盲区章节**
- **v5 (2026-09-21): 131 用例，综合 4.75/5，0 P0 + 0 P1 + 1 P2；ipc.c / timer.c / thread.c
  对外接口补齐入参校验（空指针 + 除零 → MIMI_EPARAMETER），除零崩溃已转为可测断言**
- **v7 (2026-09-22): 130 用例，综合 4.75/5，0 P0 + 0 P1 + 1 P2；源码入参校验由错误码改为
  mimi_assert，测试改用 TEST_EXPECT_ASSERT 捕获断言（覆盖 weak 的 mimi_assert_failed）；
  mqueue_init 随后也统一为断言，同模块校验方式一致；release 下断言失效确认为有意设计**
**项目**: mimimi_os — 嵌入式 RTOS
