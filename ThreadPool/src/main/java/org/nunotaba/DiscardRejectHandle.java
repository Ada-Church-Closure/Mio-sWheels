package org.nunotaba;

public class DiscardRejectHandle implements RejectHandle {
    @Override
    public void reject(Runnable rejectedCommand, MyThreadPool pool) {
        pool.blockingQueue.poll();
        pool.execute(rejectedCommand);
    }
}
