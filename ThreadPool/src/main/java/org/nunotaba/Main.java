package org.nunotaba;


import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.TimeUnit;

public class Main {
    List<Integer> integers = new ArrayList<>();
    public static void main(String[] args) {
        MyThreadPool myThreadPool = new MyThreadPool(2, 4, 1, TimeUnit.SECONDS, new ArrayBlockingQueue<>(2), new DiscardRejectHandle());

        for(int index = 0; index < 6; ++index){
            myThreadPool.execute(() -> {
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
                System.out.println(Thread.currentThread().getName());
            });
        }




        System.out.println("Main Thread is not blocked");

    }
}