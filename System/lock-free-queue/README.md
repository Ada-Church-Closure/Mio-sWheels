# 有界 MPMC 无锁队列（CAS + 序号环）

本目录实现的是经典的 **Dmitry Vyukov 风格有界多生产者多消费者（MPMC）环形队列**：用 **单调递增的全局位置** 分配槽位，用 **每个槽位上的序号（`sequence_number`）** 表达「空 / 已写入待取 / 已取走待下一轮写入」三种逻辑状态；槽位争用通过 **`enqueue_pos` / `dequeue_pos` 上的 CAS** 解决，槽位与 `data` 的可见性通过 **`release` / `acquire` 成对** 保证。

下文按「数据结构 → 缓存行 → 环上序号与轮转 → CAS 流程 → 内存序」的顺序说明，并与 `mpmc.h` / `mpmc.c` 中的实现对齐。
<img width="5828" height="6110" alt="Image" src="https://github.com/user-attachments/assets/08f635c4-cb9e-4197-95c4-5574f8e2faf8" />
---

## 1. 核心思路（为什么同时需要「位置」和「槽位序号」）

- **全局位置** `enqueue_pos` / `dequeue_pos`：只增不减，多线程用 **CAS** 抢占「下一个要操作的逻辑序号」`pos`。同一时刻只有一个线程能成功把 `enqueue_pos` 从 `pos` 更新为 `pos + 1`（出队同理用 `dequeue_pos`）。
- **槽位索引**：物理数组下标是 `pos & buffer_mask`，因此 **容量必须是 2 的幂**，`buffer_mask = capacity - 1` 才能把任意 `pos` 映射到 `[0, capacity)` 的环上。
- **槽位序号** `cell->sequence_number`：描述 **该槽在当前这一轮「全局进度」下是否允许写 / 是否已有可读数据**。它不替代 CAS，而是与 CAS 配合，避免在槽位状态不对时误写/误读，并在无锁情况下把 **`cell->data` 的普通写** 与 **「数据就绪」** 联系起来（见第 5 节内存序）。

---

## 2. 数据结构（`mpmc.h`）

### 2.1 `cell_t`：一个槽位 = 状态 + 载荷

```9:12:System/lock-free-queue/mpmc.h
typedef struct {
    atomic_size_t sequence_number;
    void* data;
} cell_t;
```

- **`sequence_number`（原子）**：该槽在环上的 **逻辑代次**；与当前线程持有的 `pos` 比较，得到「空 / 可写 / 可读 / 满」等分支（见第 4、5 节）。
- **`data`（非原子指针）**：实际存放的元素指针。正确性依赖 **先写 `data`、再以 `release` 更新 `seq`**，以及消费者侧 **以 `acquire` 观察到就绪后再读 `data`**，而不是对 `data` 使用原子类型。

### 2.2 `mpmc_queue_t`：环、两个热计数器

```14:20:System/lock-free-queue/mpmc.h
typedef struct {
    size_t buffer_mask; // mask_number
    cell_t* buffer; // ring buffer

    _Alignas(CACHE_LINE_SIZE) atomic_size_t enqueue_pos; // cpu cache line friendly.
    _Alignas(CACHE_LINE_SIZE) atomic_size_t dequeue_pos;
} mpmc_queue_t;
```

- **`buffer` + `buffer_mask`**：环形缓冲区本体；在容量为 2 的幂时 `buffer_mask = capacity - 1`，用于 `pos & mask` 快速折返到下标。
- **`enqueue_pos` / `dequeue_pos`**：被多线程高频 **读、CAS** 的计数器，容易与其它热数据发生 **伪共享**，因此用 **缓存行对齐** 单独布置（第 3 节）。

---

## 3. 缓存行对齐（`_Alignas(CACHE_LINE_SIZE)`）的原理

```18:19:System/lock-free-queue/mpmc.h
    _Alignas(CACHE_LINE_SIZE) atomic_size_t enqueue_pos; // cpu cache line friendly.
    _Alignas(CACHE_LINE_SIZE) atomic_size_t dequeue_pos;
```

- CPU 缓存以 **缓存行**（常见为 64 字节，与本头文件中 `CACHE_LINE_SIZE` 一致）为粒度在核间迁移与一致性协议下同步。
- 若 `enqueue_pos` 与 `dequeue_pos` 落在 **同一缓存行**，则生产者频繁改写入队计数、消费者频繁改写出队计数时，会在不同核上反复造成 **同一行的失效与回填**（false sharing），放大争用、抖动延迟。
- 对二者分别 `_Alignas(64)`，目的是让它们尽量落在 **不同缓存行**，把「入队热计数」与「出队热计数」在缓存层面拆开。这是 **性能与布局优化**，不改变队列语义。

---

## 4. 初始化与环上轮转（`queue_init`，`mpmc.c`）

```9:28:System/lock-free-queue/mpmc.c
int queue_init(mpmc_queue_t *q, size_t capacity) {
    if (capacity < 2 || (capacity & (capacity - 1)) != 0) {
        return -1;
    }

    q->buffer_mask = capacity - 1;
    q->buffer = (cell_t*)malloc(sizeof(cell_t) * capacity);
    if (!q->buffer) {
        return -1;
    }

    for (int index = 0; index < capacity; ++index) {
        atomic_init(&q->buffer[index].sequence_number, index);
    }

    atomic_init(&q->enqueue_pos, 0);
    atomic_init(&q->dequeue_pos, 0);

    return 0;
}
```

### 4.1 为什么容量必须是 2 的幂

对非负整数 `pos` 与容量 \(C = 2^k\)，有 **`pos & (C - 1) = pos mod C`**。这样逻辑序号 `pos` 可无限递增，物理下标始终在 `[0, C)` 上环状折返，且用按位与代替取模。

### 4.2 为什么第 `i` 个槽的初始序号是 `i`

记 `buffer_mask + 1 = capacity`（常记为 \(C\)）。

1. **初始空队列**  
   槽 `0` 上 `seq = 0`。消费者若取 `dequeue_pos = 0`，期望「数据已就绪」的条件是 **`seq == pos + 1 == 1`**。此时 `seq - (pos+1) < 0`，出队失败，符合「空」。

2. **第一次入队**（`enqueue_pos` 从 `0` CAS 到 `1` 的那次）  
   生产者对槽 `0` 要求 **`seq == pos == 0`**，与初始化一致；写入 `data` 后 **`release` 写 `seq = pos + 1 = 1`**，表示槽 `0` 对本轮消费者可读。

3. **第一次出队**（`dequeue_pos` 从 `0` CAS 到 `1`）  
   读到 `data` 后 **`release` 写 `seq = pos + buffer_mask + 1 = 0 + C = C`**。注意 **`C & buffer_mask == 0`**，仍是槽 `0`，但序号已进入 **下一轮入队** 所期望的「空槽」标号。

4. **下一轮再次写入槽 `0`**  
   下一轮占用该物理槽的全局入队序号是 **`pos = C`（以及之后 `C, 2C, …` 中与 `C` 同余于模 \(C\) 的位置）**。生产者要求 **`seq == pos`**，即 **`seq == C`**，与上一步出队结束时的 `seq` 一致。

对任意物理槽位 `k`：初始化 **`seq = k`** 与第一次 **`pos = k`** 对齐；之后每完整经历「入队写 `pos+1` → 出队写 `pos+C`」，`seq` 与「下一个应占用该槽的 `pos`」始终相差 **整圈 \(C\)**，从而 **`seq == pos`（空、可写）** 与 **`seq == pos + 1`（满、可读）** 交替成立。这就是「**消费者释放时在序号上加一整圈（`+ (buffer_mask+1)`）**」的推导核心。

---

## 5. CAS 主循环与内存屏障

### 5.1 入队：`acquire` 读 `seq`，`release` 发布就绪

```35:40:System/lock-free-queue/mpmc.c
    size_t pos = atomic_load_explicit(&q->enqueue_pos, memory_order_relaxed);

    for (;;) {
        cell = &(q->buffer[pos & q->buffer_mask]);
        // ensure after this clause we have right status of seq
        size_t seq = atomic_load_explicit(&cell->sequence_number, memory_order_acquire);
```

```60:64:System/lock-free-queue/mpmc.c
    // 一定需要在buffer写入之后才能更新seq
    cell->data = data;
    // 更改cell序列号为pos + 1
    atomic_store_explicit(&cell->sequence_number, pos + 1, memory_order_release);
    return 1;
```

**CAS 与分支含义（与 `diff = seq - pos` 配合）：**

- **`diff == 0`**：本槽对当前 `pos` 为「空」，尝试 **`CAS(&enqueue_pos, pos, pos+1)`** 独占该逻辑槽位；失败则自旋/重试。
- **`diff < 0`**：`seq < pos`，槽位尚未被消费者推进到本轮生产者可见的空状态，**队列满**（无法再入队）。
- **`diff > 0`**：`pos` 偏旧或已被他人推进，重新加载 `enqueue_pos` 更新 `pos`。

**内存序：**

- **`memory_order_acquire` 读取 `sequence_number`**：与对端在释放槽位或发布数据就绪时使用的 **`memory_order_release` 写 `seq`** 建立 **synchronizes-with**。这样当你观察到「槽已空（`seq == pos`）」时，不会看到上一轮残留的不一致中间态；当你随后写入 `data` 并以 **`release` 写 `seq = pos+1`** 发布时，消费者侧 **`acquire` 读到 `pos+1`** 的线程能合法地看到 **`cell->data` 的新值**（C11 下由 happens-before 约束非原子 `data` 的可见性）。
- **`enqueue_pos` 的 CAS 使用 `relaxed`**：「谁占有该 `pos`」由 CAS 保证互斥；槽内载荷的发布仍由 **`seq` 的 release/acquire** 负责，因此成功/失败次序用 `relaxed` 在此设计下通常足够。

### 5.2 出队：对称的 `acquire` / `release`

```76:79:System/lock-free-queue/mpmc.c
    for (;;) {
        cell = &q->buffer[pos & q->buffer_mask];
        // 保证获取的seq是准确的
        size_t seq = atomic_load_explicit(&cell->sequence_number, memory_order_acquire);
```

```97:104:System/lock-free-queue/mpmc.c
    *data = cell->data;
    // 这里加上一整圈即可
    // 最精妙的就在于生产者消费者的序列号问题.
    atomic_store_explicit(&cell->sequence_number, pos + q->buffer_mask + 1, memory_order_release);
    return 1;
```

**CAS 与分支含义（`diff = seq - (pos+1)`）：**

- **`diff == 0`**：`seq == pos+1`，数据已就绪，尝试 **`CAS(&dequeue_pos, pos, pos+1)`**。
- **`diff < 0`**：`seq < pos+1`，尚未就绪，**队列为空**。
- **`diff > 0`**：刷新 `dequeue_pos` 重试。

**内存序：**

- **`acquire` 读 `seq`**：与生产者 **`release` 写 `seq = pos+1`** 配对，保证在判定可取之后读取的 **`cell->data`** 来自对方已发布的写入。
- **`release` 写 `seq = pos + buffer_mask + 1`**：在读出 `data` **之后** 执行，把槽位标回下一轮生产者所需的「空」标号，并与生产者侧对该槽的 **`acquire` 读 `seq`** 配对。

---

## 6. API 与并发约定

| 函数 | 成功 | 常见失败 |
|------|------|----------|
| `queue_init` | `0` | `-1`（容量非法或 `malloc` 失败） |
| `queue_enqueue` | `1` | `0`（满） |
| `queue_dequeue` | `1` | `0`（空），`-1`（`data == NULL`） |
| `queue_destroy` | `0` | `-1`（`q == NULL`） |

**`queue_destroy`：** 必须在 **所有线程停止使用该队列** 之后调用；否则与仍在进行的入队/出队并发释放 `buffer`，属于未定义行为。

---

## 7. 延伸阅读

- Vyukov bounded MPMC：单调 `pos`、每槽 `seq`、CAS 抢占位置、`release`/`acquire` 发布载荷是常见教科书结构。
- C11：`memory_order_acquire` / `memory_order_release` / `memory_order_relaxed` 的语义见 **ISO C11 §7.17**。
