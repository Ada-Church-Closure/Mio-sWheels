package org.nunotaba;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.TimeUnit;

public class MyThreadPool {

    // 核心线程数
    private final int corePoolSize;
    // 最大线程数
    private final int maxSize;

    // 超时等待时间
    private final int timeout;

    // TimeUnit
    private final TimeUnit timeUnit;

    // 阻塞队列存放到达的所有任务
    public final BlockingQueue<Runnable> blockingQueue;

    // 采用的拒绝策略
    private final RejectHandle rejectHandle;



    public MyThreadPool(int corePoolSize, int maxSize, int timeout, TimeUnit timeUnit, BlockingQueue<Runnable> queue, RejectHandle rejectHandle) {
        this.corePoolSize = corePoolSize;
        this.maxSize = maxSize;
        this.timeout = timeout;
        this.timeUnit = timeUnit;
        this.blockingQueue = queue;
        this.rejectHandle = rejectHandle;
    }



    // 存放核心线程
    List<Thread> coreList = new ArrayList<>();

    // 存放辅助线程
    List<Thread> supportList = new ArrayList<>();




    // 希望线程池执行的任务
    // TODO 解决这里的线程安全问题
    void execute(Runnable command) {
        // 1.当我们的线程小于corePoolSize的时候,我们应当创建线程
        if(coreList.size() < corePoolSize){
            Thread thread = new coreThread();
            coreList.add(thread);
            thread.start();
        }

        // 2.成功放入了阻塞队列内部,直接返回
        if(blockingQueue.offer(command)){
            return;
        }

        // 3.没有大于最大线程数,我暂时可以让辅助线程来执行.
        if(coreList.size() + supportList.size() < maxSize){
            Thread thread = new supportThread();
            supportList.add(thread);
            thread.start();
        }

        if(!blockingQueue.offer(command)){
            // 这里就会触发拒绝策略
            rejectHandle.reject(command, this);
        }
    }


    // Task 任务,是未来我们核心线程要执行的逻辑
    // 把任务抽象成一个类
    class coreThread extends Thread {
        @Override
        public void run() {
            while (true) {
                try {
                    // 如果这里没有数据,就会阻塞住,这就是阻塞等待
                    Runnable command = blockingQueue.take();
                    command.run();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }
        }
    }

    class supportThread extends Thread {
        @Override
        public void run() {
            while(true) {
                try {
                    // 如果这里没有数据,就会阻塞住,这就是阻塞等待
                    // 但是我们只会等待1s,1s结束,直接返回null
                    Runnable command = blockingQueue.poll(timeout, timeUnit);
                    if(command == null){
                        break;
                    }
                    command.run();
                } catch (InterruptedException e) {
                    throw new RuntimeException(e);
                }
            }

            System.out.println("Support Thread " + Thread.currentThread().getName() + " is Over!");
        }
    }


}
