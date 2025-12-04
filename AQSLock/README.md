# 实现简单AQS

## 第一部分：什么是 CAS (Compare-And-Swap)

### 1. 核心思想：乐观锁

CAS 是一种实现并发控制的机制，它体现了 **乐观锁 (Optimistic Locking)** 的思想。

* **悲观锁 (Pessimistic Locking)**：认为并发冲突总是会发生，所以在访问数据前，先上锁，独占资源。例如 `synchronized`和 `ReentrantLock`。
* **乐观锁 (Optimistic Locking)**：认为并发冲突不经常发生，所以先直接进行操作，在操作结束时检查一下是否有冲突发生。如果没有冲突，操作成功；如果有冲突，就进行重试或采取其他措施。

**CAS 就是乐观锁思想的完美实现。**

### 2. CAS 的定义与操作流程

**CAS (Compare-And-Swap)**，即“比较并交换”，是 CPU 提供的一条**原子指令**（整个操作不可分割，要么全做，要么全不做）。

它包含三个操作数：

1. **内存位置 (V)**：需要读写的内存地址（例如，一个变量 `count`的地址）。
2. **预期原值 (A)**：线程认为该内存位置当前应该是什么值。
3. **新值 (B)**：如果内存位置的当前值与预期原值相匹配，那么将其更新为新值。

**操作流程像一个“君子协定”：**

1. 我从内存 `V`中读取值，记作 `current = V`。
2. 我在心里盘算：如果 `current`等于我预期的 `A`，那么我就认为在我读完后、想写回前，没有其他线程动过这个值。那么，我就可以放心地把 `V`更新为 `B`。
3. 我把 `V`, `A`, `B`这三个参数告诉 CPU，执行 CAS 指令。
4. **CPU 原子性地完成以下步骤**：

   * 检查 `V`的当前值是否真的等于 `A`。
   * **如果相等**：则将 `V`的值设置为 `B`，并返回 `true`（表示成功）。
   * **如果不相等**：说明在我“盘算”期间，有其他线程修改了 `V`。那么，我不做任何修改，并返回 `false`（表示失败）。

### 3. 代码示例（伪代码）

假设我们有一个变量 `value`，我们想把它从 10 增加到 11。

```java
// 初始状态：value = 10
int value = 10;

// 线程 Thread-A 想执行 value++
// 它“预期” value 现在是 10，并想把它改成 11
boolean success = compareAndSwap(&value, 10, 11);

if (success) {
    System.out.println("增加成功！");
} else {
    System.out.println("增加失败，需要重试...");
    // 通常会在这里进行循环重试，直到成功
}
```

**在真实的 Java 代码中**，你不会直接调用 `compareAndSwap`，而是通过 `java.util.concurrent.atomic`包下的原子类来间接使用它。

---

## 第二部分：什么是原子变量 (Atomic Variables)

### 1. 定义

**原子变量 (Atomic Variables)** 是 Java 提供的一组包装类，它们在 `java.util.concurrent.atomic`包下。这些类的特点是：**对它们内部值的单个操作（如 `get`, `set`, `incrementAndGet`）是“原子的”和“线程安全的”**，无需使用 `synchronized`关键字。

常见的原子变量类有：

* `AtomicInteger`：原子性的 `int`
* `AtomicLong`：原子性的 `long`
* `AtomicBoolean`：原子性的 `boolean`
* `AtomicReference<T>`：原子性的对象引用
* `AtomicIntegerArray`：原子性的 `int`数组

### 2. 原子变量的实现原理

**原子变量的线程安全，正是通过底层的 CAS 操作来实现的！**

以 `AtomicInteger`的 `incrementAndGet()`(相当于 `i++`) 方法为例，我们来看它是如何用 CAS 实现的（简化版源码）：

```java
public class AtomicInteger extends Number implements java.io.Serializable {
    // 使用 volatile 保证内存可见性
    private volatile int value;

    public final int incrementAndGet() {
        // 这是一个循环，也就是著名的 CAS 自旋
        for (;;) {
            // 1. 获取当前最新的值 (预期值 A)
            int current = get(); // current = 10
            // 2. 计算出目标新值 (B)
            int next = current + 1; // next = 11
            // 3. 调用 Unsafe 类的 native CAS 方法
            //    如果 value 的当前值还是 current (10)，就把它更新为 next (11)
            //    如果失败了，说明有其他线程改了 value，就回到循环开头重试
            if (compareAndSet(current, next))
                return next;
        }
    }

    // 这个 compareAndSet 方法内部就是调用了 CPU 的 CAS 指令
    public final boolean compareAndSet(int expect, int update) {
        return unsafe.compareAndSwapInt(this, valueOffset, expect, update);
    }
}
```

**这个过程就是一个典型的“乐观锁 + 自旋”：**

1. 线程读取 `value`的当前值 (`current`)。
2. 计算新值 (`next`)。
3. 尝试用 CAS 更新。
4. 如果成功，皆大欢喜。
5. 如果失败（因为其他线程插队修改了 `value`），**不气馁，不阻塞**，而是**立即重试**（回到步骤 1）。这个重试过程就是 **自旋 (Spin)**。

### 3. CAS 的优缺点

#### 优点：

* **高性能**：在低至中度竞争的情况下，CAS 的性能远超 `synchronized`。因为它避免了线程的阻塞和唤醒所带来的巨大开销（涉及操作系统内核态切换）。
* **无锁**：不会导致线程挂起，减少了死锁的可能性。

#### 缺点：

* **ABA 问题**：

  * **场景**：线程 1 读取值 A，然后被挂起。线程 2 将值从 A 改为 B，然后又改回 A。线程 1 恢复后，执行 CAS，发现值还是 A，于是操作成功。
  * **问题**：看起来没问题，但实际上值已经被别人动过了。在某些场景下（如链表操作），这可能引发严重错误。
  * **解决方案**：使用带有版本号的原子引用类 `AtomicStampedReference`，它不仅比较值，还比较一个“时间戳/版本号”。
* **循环时间长，开销大**：在高强度竞争下，如果 CAS 长时间失败，自旋会消耗大量 CPU 资源。相比之下，阻塞线程可能更划算。
* **只能保证一个共享变量的原子操作**：对于多个变量的复合操作，CAS 也无能为力，需要用锁。

---

## 总结与类比


| 概念                       | 比喻                               | 说明                                                                                                                                               |
| -------------------------- | ---------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| **`synchronized`(悲观锁)** | **进厕所关门上锁**                 | 认为总有人抢坑位，所以一进来就把门锁上，独占资源。简单粗暴，但开销大。                                                                             |
| **CAS (乐观锁机制)**       | **看一眼，没人就进去**             | 先探头看看坑位是否空闲（比较），如果没人，就进去并占住（交换）。如果发现有人（比较失败），就退出来等会儿再试（自旋）。                             |
| **原子变量**               | **一个自带“无锁”机制的智能马桶** | 你按“冲水+计数”按钮（`incrementAndGet`），它内部会自动执行“看一眼、没人就冲水并+1”的逻辑，并保证整个过程不被打扰。你不需要关心它是怎么实现的。 |

## 搞清楚了之后,我们来理解AQS

AQS (AbstractQueuedSynchronizer) 的核心就两样东西：

1. **资源状态 (`state`)**：一个 `volatile int`，代表锁是被占用还是空闲。
2. **CLH 队列**：一个双向链表，用来存放抢不到锁、需要排队的线程。

------

### 一、 核心数据结构：CLH 队列的变体

原生的 CLH 锁是一种**自旋锁**，基于单向链表。但 AQS 为了实现**阻塞锁**（让线程挂起而不是死循环空转），对 CLH 做了改造，变成了**双向链表**。

#### 1. 节点（Node）的代码结构

这是 AQS 内部静态类 `Node` 的核心字段：

Java

```java
static final class Node {
    // 线程的状态
    // 0: 初始状态
    // -1 (SIGNAL): 表示我的"后继节点"被挂起了(park)，我释放锁后必须唤醒它
    // 1 (CANCELLED): 表示我等不及了(超时或中断)，要取消排队
    volatile int waitStatus;

    // 前驱指针（双向链表关键）
    // 作用：1. 取消排队时用于移除节点 2. 也是为了能从尾部向前遍历
    volatile Node prev;

    // 后继指针
    // 作用：我释放锁时，通过它找到下一个人去唤醒
    volatile Node next;

    // 真正排队的线程
    volatile Thread thread;
}
```

#### 2. AQS 骨架

Java

```java
public abstract class AbstractQueuedSynchronizer {
    // 锁的状态：0代表无锁，1代表持有锁，>1代表重入次数
    private volatile int state;
    
    // 队列头结点（哨兵节点，不存真实线程，只是占位）
    private transient volatile Node head;
    
    // 队列尾结点（新来的线程都插到这里）
    private transient volatile Node tail;
}
```

------

### 二、 核心流程：加锁与排队 (Acquire)

当一个线程调用 `lock.lock()` 时，底层其实是调用了 AQS 的 `acquire(1)`。

#### 代码逻辑拆解：

Java

```java
// 1. 入口方法
public final void acquire(int arg) {
    // 步骤 A: 尝试直接抢锁 (tryAcquire 由子类如 ReentrantLock 实现)
    // 步骤 B: 抢不到? addWaiter 将自己封装成 Node 加入队尾
    // 步骤 C: acquireQueued 在队列里"死循环"等待
    if (!tryAcquire(arg) &&
        acquireQueued(addWaiter(Node.EXCLUSIVE), arg))
        selfInterrupt();
}

// 2. 加入队列 (addWaiter)
private Node addWaiter(Node mode) {
    Node node = new Node(Thread.currentThread(), mode);
    // ... 省略快速尝试 CAS 插入尾部的代码 ...
    enq(node); // 如果快速尝试失败，进入死循环用 CAS 强行插入队尾
    return node;
}

// 3. 在队列中排队 (acquireQueued) - 這是最核心的“挂起”逻辑
final boolean acquireQueued(final Node node, int arg) {
    for (;;) { // 死循环，直到拿到锁
        // 获取我的前驱节点
        final Node p = node.predecessor();
        
        // 只有一种情况我有资格去尝试抢锁：
        // 我的前驱是 head (说明我是队列里的第一个真实线程)
        if (p == head && tryAcquire(arg)) {
            setHead(node); // 我抢到了！我现在变成 head (哨兵)
            p.next = null; // 断开旧 head，帮助 GC
            return false;
        }

        // 抢失败了？或者我前面还有人？
        // shouldPark... 检查前驱的状态，如果是 SIGNAL，说明前驱承诺会叫醒我
        // parkAndCheckInterrupt 调用 LockSupport.park() 真正让线程挂起（睡着）
        if (shouldParkAfterFailedAcquire(p, node) &&
            parkAndCheckInterrupt())
            interrupted = true;
    }
}
```

**关键点：**

- **自旋 + 阻塞**：AQS 不是一直自旋（浪费 CPU），也不是立马阻塞。它是先检查一下能不能抢（前驱是 head），抢不到再找个“枕头”（前驱状态设为 SIGNAL）然后睡去（park）。

------

### 三、 核心流程：解锁与唤醒 (Release)

当持有锁的线程调用 `lock.unlock()` 时：

Java

```java
public final boolean release(int arg) {
    // 1. 尝试释放状态 (state - 1)
    if (tryRelease(arg)) {
        Node h = head;
        // 2. 如果队列里有人 (h != null) 且状态需要唤醒 (waitStatus != 0)
        if (h != null && h.waitStatus != 0)
            unparkSuccessor(h); // 唤醒头结点的后继
        return true;
    }
    return false;
}

// 唤醒逻辑 (unparkSuccessor)
private void unparkSuccessor(Node node) {
    // node 是 head
    Node s = node.next;
    
    // 特殊情况：如果 next 节点是 null 或者被取消了 (CANCELLED)
    if (s == null || s.waitStatus > 0) {
        s = null;
        // 【高频考点】为什么要从 tail 往回找？
        // 为了找到离 head 最近的那个【有效】节点
        for (Node t = tail; t != null && t != node; t = t.prev)
            if (t.waitStatus <= 0)
                s = t;
    }
    
    // 唤醒它
    if (s != null)
        LockSupport.unpark(s.thread);
}
```

------

### 四、 高频面试题（专门针对你的 Level）

#### Q1: 为什么 AQS 的 CLH 队列要用“双向链表”，而原生的 CLH 是单向的？

- **普通回答：** 方便操作。
- **满分回答（结合源码）：**
  1. **唤醒时的可靠性：** 在 `unparkSuccessor` 方法中，如果 `head.next` 节点发生了取消（Cancelled）或者断链，我们需要从 `tail` 指针向前遍历（`prev` 指针）来找到最靠前的有效节点进行唤醒。这是因为节点入队时是**先设置 prev，再设置 next**（`CAS` 操作无法原子性设置双向链接），所以从后往前遍历是数据结构上绝对安全的。
  2. **取消排队：** 如果一个线程等待超时或被中断，需要从队列中移除。双向链表让我们可以快速获取前驱节点，从而完成链表的重连（`prev.next = next`）。

#### Q2: 既然已经有了 Queue，为什么 `acquireQueued` 里还需要一个死循环 `for(;;)`？

- **回答：** 这是一个**自旋**的过程。
  - 当线程被唤醒（unpark）时，它并不是“直接”拿到锁，而是从 `parkAndCheckInterrupt()` 方法返回，重新进入 `for(;;)` 循环的下一次迭代。
  - 它需要再次检查 `p == head && tryAcquire()`。因为在它被唤醒的瞬间，可能有一个**新来的线程**（非公平锁场景）插队抢走了锁。
  - 如果没抢到，它必须再次挂起。这个循环保证了线程被唤醒后能正确地重新竞争锁。

#### Q3: AQS 中的 `state` 为什么要用 `volatile` 修饰？

- **回答：** 保证**可见性**。
  - AQS 的核心就是通过 `state` 变量来判断锁是否被占用。多线程环境下，一个线程修改了 `state`（释放锁），其他线程必须立马看见，否则会导致死锁或重复加锁。
  - 配合 `CAS`（`compareAndSetState`）操作，保证了状态更新的**原子性**。

#### Q4: 简述一下 ReentrantLock 的公平锁和非公平锁在 AQS 层面的区别？

- **回答：** 区别只在于 `tryAcquire`（尝试抢锁）的逻辑不同。
  - **非公平锁 (Nonfair)：** 线程一进来，不管队列里有没有人，先直接 CAS 尝试改 `state` 抢锁。抢不到才排队。
  - **公平锁 (Fair)：** 线程进来前，会先调用 `hasQueuedPredecessors()` 问一下：“队列里有人在排队吗？”。如果有，自己老实去排队，绝不尝试 CAS。

#### Q5: 你觉得 AQS 的设计模式是什么？

- **回答：** **模板方法模式 (Template Method Pattern)**。
  - AQS 提供了 `acquire`, `release` 等顶层流程骨架（这些是 `final` 的，不可重写）。
  - 具体的资源获取/释放逻辑（`tryAcquire`, `tryRelease`）留给子类（如 `ReentrantLock`, `CountDownLatch`）去实现。
