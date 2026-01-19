package org.nunotaba;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

// 1.继承Thread类
class threadTask extends Thread{
    @Override
    public void run() {
        System.out.println("1.Extending Thread class.");
    }
}

// 2.实现Runnable接口
class runnableTask implements Runnable{
    @Override
    public void run() {
        System.out.println("2.Implementing Runnable Interface.");
    }
}

// 3.实现Callable接口
class callableTask implements Callable<String>{
    @Override
    public String call() {
        return "3.Implementing callable method.";
    }
}


public class Main {
    public static void main(String[] args) throws ExecutionException, InterruptedException {
        Thread t1 = new threadTask();
        t1.start();

        runnableTask runnableTask = new runnableTask();
        Thread t2 = new Thread(runnableTask);
        t2.start();

        callableTask callableTask = new callableTask();
        FutureTask<String> futureTask = new FutureTask<>(callableTask);
        Thread t3 = new Thread(futureTask);
        t3.start();
        System.out.println(futureTask.isDone());
        System.out.println(futureTask.get());
    }
}