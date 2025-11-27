package org.nunotaba;

public class ThrowRejectHandle implements RejectHandle{

    @Override
    public void reject(Runnable rejectedCommand, MyThreadPool pool) {
        throw new RuntimeException("The Blocking Queue is Full......");
    }
}
