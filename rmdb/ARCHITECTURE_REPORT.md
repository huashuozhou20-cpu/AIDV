# RMDB 架构深度分析报告

**分析范围**: 全量源码（不修改任何代码）  
**分析日期**: 2026-05-18  
**基准 Commit**: `a4f2419`

---

## 目录

1. [2PL 锁管理器（LockManager）](#一2pl-锁管理器lockmanager)
2. [Auto-Commit 完整调用链](#二auto-commit-完整调用链)
3. [缓冲区架构（Buffer Pool / Log Buffer）](#三缓冲区架构)
4. [编译与测试](#四编译与测试)
5. [现状总结](#五现状总结)

---

## 一、2PL 锁管理器（LockManager）

### 1.1 核心文件与类

| 文件 | 角色 |
|------|------|
| `src/transaction/concurrency/lock_manager.h` | `LockManager` 类声明（第19-76行），含 `LockMode`/`GroupLockMode` 枚举、`LockRequest`、`LockRequestQueue` |
| `src/transaction/concurrency/lock_manager.cpp` | 全部实现：`lock_common`、`unlock`、`find_deadlock_victim`、`is_compatible` 等 |
| `src/transaction/txn_defs.h` | `LockDataId`（第74-112行）：表级锁和行级锁的统一标识 |
| `src/transaction/transaction.h` | `Transaction` 类持有 `lock_set_`（第139行），通过 `get_lock_set()` 访问（第98行） |

### 1.2 锁模式层次

```
LockMode（请求）: SHARED, EXLUCSIVE, INTENTION_SHARED, INTENTION_EXCLUSIVE, S_IX
GroupLockMode（队列状态）: NON_LOCK, IS, IX, S, X, SIX

优先级: X > SIX > S+IX(SIX) > S > IX > IS > NON_LOCK
```

兼容矩阵（`lock_manager.cpp:31-38`）：

```
               NL  IS  IX   S   X  SIX
S  (SHARED)    ✓   ✓   ✗   ✓   ✗   ✗
X  (EXCLUSIVE) ✓   ✗   ✗   ✗   ✗   ✗
IS (INT_SHARED)✓   ✓   ✓   ✓   ✗   ✓
IX (INT_EXCL)  ✓   ✓   ✓   ✗   ✗   ✗
SIX            ✓   ✓   ✗   ✗   ✗   ✗
```

### 1.3 条件变量阻塞与唤醒机制

**声明**: `LockRequestQueue::cv_` — `std::condition_variable`（`lock_manager.h:41`）

每个 `LockDataId`（表或行）在全局 `lock_table_` 中拥有独立的 `condition_variable`。

#### 阻塞路径：`lock_common`（`lock_manager.cpp:65-133`）

```
lock_common(txn, lock_data_id, lock_mode)
  │
  ├─ std::unique_lock<std::mutex> lock(latch_);     // 第66行：获取全局互斥锁
  ├─ 检查现有锁（重复获取 / S→X升级 / 锁覆盖）       // 第71-101行
  │
  └─ while (!is_compatible(lock_mode, group_mode))   // 第105行：等待循环
       ├─ find_deadlock_victim(txn_id, queue)         // 第107行：DFS死锁检测
       │   └─ 若发现环 → throw TransactionAbortException
       ├─ 插入 ungranted LockRequest 到队列          // 第112-114行
       ├─ queue.cv_.wait(lock);                       // 第116行：阻塞 + 释放 latch_
       │   └─ 等待 unlock() 中的 notify_all()
       └─ 唤醒后移除 ungranted 请求                  // 第118-123行
```

#### 唤醒路径：`unlock`（`lock_manager.cpp:257-287`）

```
unlock(txn, lock_data_id)
  │
  ├─ std::unique_lock<std::mutex> lock(latch_);      // 第258行
  ├─ 查找 lock_table_ 中的队列                        // 第260-263行
  ├─ 擦除已授予请求                                   // 第270-271行
  ├─ update_group_lock_mode(queue)                    // 第272行
  ├─ queue.cv_.notify_all()                           // 第275行：唤醒所有等待者
  └─ 若队列为空 → lock_table_.erase(it)              // 第278-280行
```

#### 锁覆盖规则（`lock_manager.cpp:92-101`）

| 已持有 | 请求 | 结果 |
|--------|------|------|
| X | S, IX, IS, SIX | 立即返回 true（X 覆盖一切） |
| SIX | S, IX, IS | 立即返回 true |
| IX | IS | 立即返回 true |

### 1.4 死锁检测

**函数**: `find_deadlock_victim`（`lock_manager.cpp:187-255`）

- 从 `lock_table_` 全局构建等待图（waiter → holder）
- 对请求者执行 DFS 递归栈检测
- 若发现环：选择环中 **最高 txn_id（最年轻事务）** 作为牺牲者
- 通过 `TransactionAbortException` 抛出牺牲者 txn_id

### 1.5 RmFileHandle 中的锁集成

**文件**: `src/record/rm_file_handle.cpp`

| 方法 | 行号 | 表锁 | 行锁 | nullptr 守卫 |
|------|------|------|------|-------------|
| `get_record` | 19-33 | ~~IS~~（已注释） | ~~S~~（已注释） | N/A |
| `insert_record` | 41-63 | IX（第43行） | X（第60行） | 第42/59行 |
| `delete_record` | 90-107 | IX（第92行） | X（第93行） | 第91行 |
| `update_record` | 116-129 | IX（第118行） | X（第119行） | 第117行 |

> **注意**: `get_record`（读操作）的锁调用已被注释，当前读操作完全无锁。

---

## 二、Auto-Commit 完整调用链

### 2.1 完整调用链（从 SQL 到磁盘）

```
用户发送: INSERT INTO ... VALUES(...);
│
├─[1] rmdb.cpp:272      Context *context = new Context(lock_manager, log_manager, ...)
│    rmdb.cpp:273        SetTransaction(&txn_id, context)
│      └─ rmdb.cpp:62      txn_manager->begin(nullptr, log_mgr_)
│           └─ transaction_manager.cpp:39    new BeginLogRecord
│           └─ transaction_manager.cpp:40    log_manager->add_log_to_buffer(begin_log)
│
├─[2] rmdb.cpp:310      analyze->do_analyze(parse_tree)
├─[3] rmdb.cpp:315      optimizer->plan_query(query, context)
├─[4] rmdb.cpp:317      portal->start(plan, context)
│    rmdb.cpp:318        portal->run(portalStmt, ql_manager, &txn_id, context)
│      └─ portal.h:136     ql->run_dml(std::move(portal->root))
│           └─ execution_manager.cpp:214    exec->Next()
│                └─ executor_insert.h:134     fh_->insert_record(rec.data, context_)
│                     └─ rm_file_handle.cpp:43  lock_IX_on_table()
│                     └─ rm_file_handle.cpp:45  create_page_handle() → buffer_pool新页
│                     └─ rm_file_handle.cpp:47  memcpy(rec → page slot)
│                     └─ rm_file_handle.cpp:55  unpin_page(dirty=true)
│                     └─ rm_file_handle.cpp:60  lock_exclusive_on_record()
│                     【DML操作本身无WAL日志记录】
│
├─[5] rmdb.cpp:374      if (auto-commit mode)
│    rmdb.cpp:376          txn_manager->commit(context->txn_, context->log_mgr_)
│      │
│      └─ transaction_manager.cpp:52    txn->set_state(SHRINKING)
│      └─ transaction_manager.cpp:54    new CommitLogRecord
│      └─ transaction_manager.cpp:55    log_manager->add_log_to_buffer(commit_log)
│      │    └─ log_manager.cpp:21          lsn = global_lsn_++
│      │    └─ log_manager.cpp:24-30       若buffer满 → disk_manager->write_log() [溢出刷盘]
│      │    └─ log_manager.cpp:31          commit_log->serialize() 写入buffer
│      │
│      └─ transaction_manager.cpp:56    log_manager->flush_log_to_disk()
│           └─ log_manager.cpp:42          disk_manager->write_log(buffer_, offset_)
│                └─ disk_manager.cpp:252      lseek(log_fd_, 0, SEEK_END)
│                └─ disk_manager.cpp:253      write(log_fd_, log_data, size)
│                └─ 【无 fsync() / fdatasync() 调用】
│
│      └─ transaction_manager.cpp:59-61  释放所有锁（unlock）
│      └─ transaction_manager.cpp:65     txn->set_state(COMMITTED)
│
└─[6] rmdb.cpp:378      delete context
```

### 2.2 关键缺陷

| 缺陷 | 文件:行 | 说明 |
|------|---------|------|
| **无 fsync/fdatasync** | `disk_manager.cpp:253` | `write()` 后直接返回，数据仅到内核页缓存，不保证持久性。崩溃时会丢失已提交事务 |
| **DML WAL 日志从未写入** | 全执行路径 | `InsertLogRecord`/`DeleteLogRecord`/`UpdateLogRecord` 类已完整定义（`log_manager.h:151-341`），但在正常执行路径中从未被实例化。WAL 仅记录 Begin/Commit/Abort |
| **write_set_ 从未填充** | `transaction_manager.cpp:91` | `abort()` 的回滚循环是空操作——`write_set_` 始终为空 |
| **INSERT 延迟 ~101ms** | 实测 | 每个 INSERT 经历完整 parse→plan→execute→commit→log_write 流水线，log write 是主要瓶颈 |

---

## 三、缓冲区架构

### 3.1 Log Buffer（WAL 日志缓冲）

| 组件 | 详情 | 文件:行 |
|------|------|---------|
| **类** | `LogBuffer` | `log_manager.h:345-360` |
| **容量** | `LOG_BUFFER_SIZE = 1024 × PAGE_SIZE = 4 MB` | `config.h:40` |
| **写入函数** | `add_log_to_buffer()` | `log_manager.cpp:19-34` |
| **刷盘函数** | `flush_log_to_disk()` | `log_manager.cpp:39-47` |
| **后台线程** | `flush_thread_` | `log_manager.h:389` |
| **线程函数** | `flush_thread_loop()` | `log_manager.cpp:49-59` |
| **刷盘间隔** | `log_timeout = 1s` | `config.cpp:17` |
| **锁** | `latch_`（全局互斥锁） | `log_manager.h:383` |

工作流程：
1. `add_log_to_buffer()`: 将日志序列化到 4MB 内存缓冲区，满时自动刷盘
2. `flush_log_to_disk()`: 强制将缓冲区内容写入 `db.log`
3. `flush_thread_loop()`: 每 1 秒唤醒，执行 `flush_log_to_disk()`（组提交风格）

### 3.2 Buffer Pool（数据页缓冲池）

| 组件 | 详情 | 文件:行 |
|------|------|---------|
| **类** | `BufferPoolManager` | `buffer_pool_manager.h:26-47` |
| **容量** | `BUFFER_POOL_SIZE = 65536` 帧 × 4KB = **256 MB** | `config.h:37-38` |
| **页大小** | `PAGE_SIZE = 4096` (4KB) | `config.h:37` |
| **淘汰策略** | `LRUReplacer` | `lru_replacer.h/cpp` |
| **核心函数** | `fetch_page()` / `new_page()` / `unpin_page()` / `flush_page()` / `flush_all_pages()` | `buffer_pool_manager.cpp` |
| **后台刷脏** | **无** | |
| **锁** | `latch_`（全局互斥锁） | `buffer_pool_manager.h:34` |

LRU 实现（`lru_replacer.cpp`）：
- `std::list<frame_id_t> LRUlist_`: 队首 = 最近使用，队尾 = 最久未用
- `std::unordered_map<frame_id_t, iterator> LRUhash_`: O(1) 查找
- `victim()`: 取队尾
- `pin()`: 从列表中移除（不可被淘汰）
- `unpin()`: 加入队首（可被淘汰）
- 线程安全：所有方法持有 `std::scoped_lock(latch_)`

### 3.3 DiskManager I/O

| 操作 | 系统调用 | 文件:行 |
|------|---------|---------|
| `read_page()` | `lseek + read` | `disk_manager.cpp:47-56` |
| `write_page()` | `lseek + write` | `disk_manager.cpp:29-38` |
| `write_log()` | `lseek(fd,0,SEEK_END) + write` | `disk_manager.cpp:246-257` |
| **O_DIRECT** | **不存在** | |
| **O_SYNC** | **不存在** | |
| **fsync/fdatasync** | **不存在** | 整个代码库零调用 |

文件描述符管理（`disk_manager.h:94-99`）：
- `path2fd_`: `unordered_map<string, int>` 路径 → fd
- `fd2path_`: `unordered_map<int, string>` fd → 路径
- `fd2pageno_[MAX_FD]`: `atomic<page_id_t>[8192]` 每 fd 的页分配计数
- `free_pages_`: `unordered_map<int, vector<page_id_t>>` 每 fd 的回收空闲页列表
- `log_fd_`: 日志文件 fd（延迟打开，默认 -1）

### 3.4 架构图

```
┌──────────────────────────────────────────────────────┐
│                    RMDB Server                       │
│                                                      │
│  ┌──────────┐   ┌──────────┐   ┌──────────────┐    │
│  │ Executor │──▶│ 2PL Lock │   │    Context    │    │
│  │ (Insert/ │   │ Manager  │   │ (txn/log/buf) │    │
│  │  Update/ │   │ (CV阻塞) │   └──────┬───────┘    │
│  │  Delete) │   └──────────┘          │            │
│  └────┬─────┘                         │            │
│       │                               ▼            │
│       │  ┌──────────────────────────────────────┐  │
│       └─▶│        Buffer Pool (256MB)           │  │
│          │  65536 frames × 4KB, LRU evict       │  │
│          │  (无后台刷脏线程)                      │  │
│          └────────────┬─────────────────────────┘  │
│                       │                            │
│  ┌────────────────────┼────────────────────────┐   │
│  │     Log Buffer     │                        │   │
│  │      (4MB)         │                        │   │
│  │  ┌──────────┐      │                        │   │
│  │  │ 后台刷盘  │      ▼                        │   │
│  │  │ 线程(1s) │─────▶│   DiskManager           │   │
│  │  └──────────┘      │   lseek+write           │   │
│  │                    │   (无fsync/O_DIRECT)    │   │
│  └────────────────────┴────────────────────────┘   │
│                       │                            │
└───────────────────────┼────────────────────────────┘
                        ▼
              ┌─────────────────┐
              │   文件系统       │
              │  内核页缓存      │
              │  db.log (WAL)   │
              │  *.txt (数据文件)│
              └─────────────────┘
```

---

## 四、编译与测试

### 4.1 构建系统

```bash
# 构建命令
cd /home/obuser/Rubase/db2025/rmdb/build
cmake ..
make -j$(nproc)
```

**编译配置**:

| 项 | 值 |
|------|-----|
| 编译器 | g++ |
| C++ 标准 | C++17 |
| 编译标志 | `-Wall -O0 -g -ggdb3` |
| 构建类型 | Debug |

**可执行目标**:

| 目标 | 源码 | 用途 |
|------|------|------|
| `build/bin/rmdb` | `src/rmdb.cpp` | 数据库服务端，TCP 端口 8765 |
| `build/bin/unit_test` | `src/unit_test.cpp` | 5 个 GoogleTest 单元测试 |
| `build/bin/test_parser` | `src/parser/test_parser.cpp` | 解析器 AST 测试（已注册 CTest） |

**库依赖图**:

```
rmdb → parser, execution, readline, pthread, planner, analyze, common
execution → system, record, transaction, planner
system → index, record
record → system, transaction, storage
index → storage
transaction → system, recovery, pthread
recovery → system, pthread
```

### 4.2 测试

```bash
# 单元测试（5 个）
cd build && ./bin/unit_test

# Parser 测试
cd build && ./bin/test_parser
# 或通过 CTest:
ctest -R test_parser

# 集成测试（需先启动服务端）
./bin/rmdb mydb &
python3 tools/cli.py < test/consolidated.test
```

**单元测试列表**（`src/unit_test.cpp`）：

| 测试套件 | 测试名称 | 测试内容 |
|---------|---------|---------|
| `LRUReplacerTest` | `SampleTest` | LRU 淘汰顺序 |
| `BufferPoolManagerTest` | `SampleTest` | BPM 页的创建/读写/淘汰 |
| `BufferPoolManagerConcurrencyTest` | `ConcurrencyTest` | 5 线程并发 BPM 压力 |
| `StorageTest` | `SimpleTest` | DiskManager + BPM 多文件随机操作 |
| `RecordManagerTest` | `SimpleTest` | RmManager 插入/更新/删除/扫描 |

**集成测试文件**: `test/` 目录下 22 个 `.test` 文件  
**核心用例**: `test/consolidated.test`（140 条 SQL，140/140 通过）

### 4.3 关键配置常量

| 常量 | 值 | 文件:行 |
|------|-----|---------|
| `PAGE_SIZE` | 4096 (4KB) | `config.h:37` |
| `BUFFER_POOL_SIZE` | 65536 帧 (256MB) | `config.h:38` |
| `LOG_BUFFER_SIZE` | 4MB | `config.h:40` |
| `LOG_FILE_NAME` | `"db.log"` | `config.h:52` |
| `DB_META_NAME` | `"db.meta"` | `config.h:53` |
| `SOCK_PORT` | 8765 | `rmdb.cpp:28` |
| `MAX_CONN_LIMIT` | 8 | `rmdb.cpp:432` |
| `log_timeout` | 1s | `config.cpp:17` |
| `enable_logging` | true | `config.cpp:15` |
| `INVALID_PAGE_ID` | -1 | `config.h:46` |
| `INVALID_TXN_ID` | -1 | `config.h:48` |

---

## 五、现状总结

### 已正确实现

| 模块 | 状态 | 关键文件 |
|------|------|---------|
| 2PL 锁管理器（条件变量阻塞） | ✅ | `lock_manager.cpp:65-133` |
| 死锁检测（DFS 等待图） | ✅ | `lock_manager.cpp:187-255` |
| DML 写操作锁集成（IX+X） | ✅ | `rm_file_handle.cpp:41-129` |
| Buffer Pool（LRU 淘汰，256MB） | ✅ | `buffer_pool_manager.cpp` |
| Log Buffer（4MB + 后台刷盘线程） | ✅ | `log_manager.cpp:19-66` |
| SQL 解析/优化/执行全流水线 | ✅ | `rmdb.cpp:310-319` |
| 多线程服务端（每连接一线程，最大 8） | ✅ | `rmdb.cpp:431-449` |

### 已知架构缺陷

| 优先级 | 缺陷 | 影响 | 涉及文件 |
|--------|------|------|---------|
| 🔴 P0 | **无 fsync** — commit 不保证持久性 | 崩溃丢数据 | `disk_manager.cpp:253` |
| 🔴 P0 | **DML WAL 日志缺失** — InsertLogRecord 等从未实例化 | 恢复无法重做/撤销 | `executor_insert.h:134` |
| 🔴 P0 | **write_set_ 为空** — abort 是空操作 | 事务回滚无效 | `transaction_manager.cpp:91` |
| 🟡 P1 | **读操作无锁** — get_record 锁已注释 | 读写并发可能读到脏数据 | `rm_file_handle.cpp:20-23` |
| 🟡 P1 | **单条 DML ~100ms** — 每条 auto-commit 串行刷盘 | 吞吐量低（~10 TPS） | `rmdb.cpp:374-377` |
| 🟡 P1 | **Buffer Pool 无后台刷脏** | 淘汰时可能阻塞 | `buffer_pool_manager.cpp:18-24` |
| 🟢 P2 | **无 O_DIRECT** — 双重缓存 | 内存浪费 256MB+ | `disk_manager.cpp:29,47` |
