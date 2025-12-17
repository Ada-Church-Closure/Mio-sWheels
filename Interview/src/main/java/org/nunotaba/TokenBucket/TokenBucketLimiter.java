package org.nunotaba.TokenBucket;

import com.sun.org.apache.xerces.internal.impl.dv.xs.SchemaDVFactoryImpl;
import sun.tools.jstat.Token;

import java.awt.image.AreaAveragingScaleFilter;

// 手写一个令牌桶(限流器):主要是考察多线程编程的能力.
// 主要就是两个方法:1.获取一个令牌 2.refill装桶--->懒加载的核心
public class TokenBucketLimiter {
    private double capacity;

    private double currTokens;

    private double tokenFillingVelocity;

    // 上一次refill的时间
    private long lastFilledBucketTime;

    public TokenBucketLimiter(double capacity, double currTokens, double tokenFillingVelocity, long lastFilledBucketTime){
        this.capacity = capacity;
        this.currTokens = currTokens;
        this.tokenFillingVelocity = tokenFillingVelocity;
        this.lastFilledBucketTime = lastFilledBucketTime;
    }

    public synchronized boolean tryAcquireToken(){
        refill();
        
        if(currTokens >= 1){
            // System.out.println("Number of Tokens comes " + currTokens + " to " + (currTokens - 1));
            --currTokens;
            System.out.println("线程 " + ThreadLocal.class.getName() + "拿走一个令牌");
            return true;
        } else {
            System.out.println("限流了,现在很挤,拿不到令牌");
            return false;
        }
    }

    public synchronized void refill(){
        long timeGap = System.currentTimeMillis() - lastFilledBucketTime;
        double filledTokens = Double.min(capacity, timeGap * tokenFillingVelocity / 1000);
        System.out.println("补充了 " + (filledTokens) + " 个tokens");
        currTokens += filledTokens;

        lastFilledBucketTime = System.currentTimeMillis();
    }




    public static void main(String[] args) throws InterruptedException {
        TokenBucketLimiter tokenBucketLimiter = new TokenBucketLimiter(20, 20, 5, System.currentTimeMillis());

        for(int index = 0; index < 50; ++index){
            new Thread(tokenBucketLimiter::tryAcquireToken, "Thread " + index).start();
            Thread.sleep(100);
        }

    }
}
