package org.nunotaba;

import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.LockSupport;

/**
 * 我们自己实现的Lock
 */
public class MyLock {

    // 这个变量就是维护lock状态.
    // AtomicBoolean flag = new AtomicBoolean(false);

    // 我们来实现可重入lock
    AtomicInteger state = new AtomicInteger(0);


    // 这个目的是使得只有持有lock的线程才能进行unlock的操作
    Thread owner = null;
    // 默认是非公平锁
    boolean isFair = false;
    public MyLock(){

    }

    public MyLock(boolean isFair){
        this.isFair = isFair;
    }

    // 对于引用,也要使用CAS的操作.
    // 因为多线程修改引用,加入队列这个操作也是需要原子的
    AtomicReference<Node> head = new AtomicReference<>(new Node());
    AtomicReference<Node> tail = new AtomicReference<>(head.get());

    /**
     * 我们希望把等待的线程管理起来,而不是一直进行自旋地等待.
     */
    void lock(){
        // 先检查可重入: 当前线程已经持有锁则直接递增计数返回
        if (owner == Thread.currentThread()) {
            state.incrementAndGet();
            return;
        }

        // 非公平锁的快速路径：尝试直接抢占
        if (!isFair && state.compareAndSet(0, 1)) {
            System.out.println(Thread.currentThread().getName() + " gets lock DIRECTLY");
            owner = Thread.currentThread();
            return;
        }

        // 这一段if判断逻辑决定了lock是否是公平的
        // 因为后来的thread有可能直接不排队,趁机拿走了lock
        // 但是没有这段逻辑的话,后来就必须先加入CLS排队来取得lock.


        Node current = new Node();
        current.thread = Thread.currentThread();
        // Node currentTail = tail.get();

        while(true){
            // 每次我们都要查询到最新的尾节点进行CAS的操作
            Node currentTail = tail.get();
            // 我们获取lock失败,要把自己放在CLS队列中去
            if(tail.compareAndSet(currentTail, current)){
                System.out.println(Thread.currentThread().getName() + " adds itself to CLS Queue......");
                current.prev = currentTail;
                currentTail.next = current;
                break;
            }
        }

        // 这里是获取了lock之后的逻辑......
        while(true){
            // condition
            // 说白了,这里头节点是持有lock并且可以运行的线程......
            if(current.prev == head.get() && state.compareAndSet(0, 1)){
                owner = Thread.currentThread();
                // 更换头节点并且断开连接.
                head.set(current);
                current.prev.next = null;
                current.prev = null;
                System.out.println(Thread.currentThread().getName() + " after being waken up, get the LOCK......");
                return;
            }
            // 把自己放入队列之后就直接阻塞了
            // 前面拿不到lock--->说明有竞争--->说明有线程可以唤醒我们--->我们这个线程才敢阻塞
            // 要不然是不能直接阻塞的.
            LockSupport.park();
        }

    }

    void unlock(){
        // 当前线程不持有lock,不能进行释放
        if(Thread.currentThread() != this.owner){
            throw new IllegalStateException(Thread.currentThread().getName() + ": This thread does not have a lock!");
        }

        int lockCount = state.get();
        if(lockCount > 1){
            state.set(lockCount - 1);
            return;
        }

        if(lockCount <= 0){
            throw new IllegalStateException("ReentrantLock Unlock Error!");
        }

        // 我释放lock,就是我要唤醒队列的下一个节点
        Node headNode = head.get();
        Node next = headNode.next;
        // 一旦释放了flag,那么相当于突破了屏障,任何线程都可能比本线程先执行flag之后的逻辑.
        // 完全释放锁
        owner = null;
        state.set(0);
        if(next != null){
            // wake up next thread......
            System.out.println(Thread.currentThread().getName() + " wakes up " + next.thread.getName());
            LockSupport.unpark(next.thread);
        }
    }

    // CLH队列的节点
    class Node{
        // 可见性要求：不同线程间读写需要可见
        volatile Node prev;
        volatile Node next;
        Thread thread;
    }
}
