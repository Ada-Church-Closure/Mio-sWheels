package org.nunotaba;


import java.util.ArrayList;
import java.util.List;

// 手写AQS
public class Main {
    public static void main(String[] args) throws InterruptedException {
        int[] count = new int[] {1000};
        List<Thread> threads = new ArrayList<>();

        MyLock lock = new MyLock();

        for(int i = 0; i < 100; ++i){
            threads.add(new Thread(() -> {
//                try {
//                    lock.lock();
//                    for(int j = 0; j < 10; ++j){
//                        --count[0];
//                    }
//                } catch (RuntimeException e) {
//                    throw new RuntimeException(e);
//                } finally {
//                    lock.unlock();
//                }


                // 我们来做可重入lock的实验
                for(int index = 0; index < 10; ++index){
                    lock.lock();
                    --count[0];
                }

                for(int index = 0; index < 10; ++index){
                    lock.unlock();
                }

            }));
        }

        for(Thread t : threads){
            t.start();
        }

        for(Thread t : threads){
            t.join();
        }

        // 结果肯定不是0,因为--不是原子的
        System.out.println(count[0]);


    }
}