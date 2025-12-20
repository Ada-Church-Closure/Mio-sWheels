package org.nunotaba.BehavioralPattern.StrategyPattern;

// 1. 定义策略接口 (Strategy)
interface PaymentStrategy {
    void pay(int amount);
}

// 2. 具体策略实现 (Concrete Strategy)
class AliPayStrategy implements PaymentStrategy {
    @Override
    public void pay(int amount) {
        System.out.println("支付宝支付: " + amount + " 元");
    }
}

class WechatPayStrategy implements PaymentStrategy {
    @Override
    public void pay(int amount) {
        System.out.println("微信支付: " + amount + " 元");
    }
}

// 3. 上下文环境 (Context) - 负责“使用”策略
class PaymentContext {
    private PaymentStrategy strategy;

    // 构造时注入策略，或者通过 setter 切换策略
    public PaymentContext(PaymentStrategy strategy) {
        this.strategy = strategy;
    }

    public void setStrategy(PaymentStrategy strategy) {
        this.strategy = strategy;
    }

    // 执行策略
    public void executePayment(int amount) {
        if (strategy == null) {
            throw new IllegalStateException("Payment method not set");
        }
        strategy.pay(amount);
    }
}

// 4. 客户端调用
public class StrategyDemo {
    public static void main(String[] args) {
        // 可以便捷的调整行为
        // 今天想用支付宝
        PaymentContext context = new PaymentContext(new AliPayStrategy());
        context.executePayment(100);

        // 突然想换微信了（运行时切换行为）
        context.setStrategy(new WechatPayStrategy());
        context.executePayment(200);
    }
}