package org.nunotaba;

// 拒绝策略的实现
public interface RejectHandle {
    void reject(Runnable rejectedCommand, MyThreadPool pool);
}
