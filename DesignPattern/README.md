# 伟大的设计模式

## 创建型设计模式

### 单例模式

> 梦开始的地方.

```java
// 购物车管理器（单例模式）
class ShoppingCart {
    // 1. 私有静态变量，保存唯一实例
    private static ShoppingCart instance;

    // 2. 商品存储：商品名称 -> 数量
    private List<String> names = new ArrayList<>();
    private List<Integer> count = new ArrayList<>();

    // 3. 私有构造函数（防止外部 new ShoppingCart()）
    // 主要还是做构造函数的私有化操作.
    private ShoppingCart() {}

    // 4. 公共静态方法，获取唯一实例
    // 这里我们是懒加载,调用了才会加载,性能比较好......
    public static ShoppingCart getInstance() {
        if (instance == null) {
            instance = new ShoppingCart();
        }
        return instance;
    }

    // 5. 添加商品
    public void addItem(String name, int quantity) {
        names.add(name);
        count.add(quantity);
    }

    // 6. 打印购物清单（适配 ACM 输出，无多余文字，直接每行一个商品）
    public void printCart() {
        int size = names.size();
        for(int index = 0; index < size; ++index){
            System.out.print(names.get(index) + " " + count.get(index) + "\n");
        }
    }
}
```

> 只允许持有一个对象.

**单例模式**是一种**创建型设计模式**， 它的核心思想是保证**一个类只有一个实例，并提供一个全局访问点来访问这个实例。**

- 只有一个实例的意思是，在整个应用程序中，只存在该类的一个实例对象，而不是创建多个相同类型的对象。
- 全局访问点的意思是，为了让其他类能够获取到这个唯一实例，该类提供了一个全局访问点（通常是一个静态方法），通过这个方法就能获得实例。

#### 为什么要使用单例设计模式呢

简易来说，单例设计模式有以下几个优点让我们考虑使用它：

- **全局控制**：保证只有一个实例，这样就可以**严格的控制客户怎样访问它以及何时访问它**，简单的说就是**对唯一实例的受控访问**（引用自《大话设计模式》第21章）
- **节省资源**：也正是因为只有一个实例存在，就**避免多次创建了相同的对象**，从而节省了系统资源，而且多个模块还可以通过单例实例共享数据。
- **懒加载**：单例模式可以实现**懒加载**，只有在需要时才进行实例化，这无疑会提高程序的性能。

那么在并发的情况下我们会这样处理:

#### synchronized + 双重lock检查机制

```java
// 4. 公共静态方法，获取唯一实例
    public static ShoppingCart getInstance() {
        if (instance == null) {
            // 检查为null,直接给整个class上lock
            synchronized (ShoppingCart.class){
                // 二重检查
                if (instance == null) {
                    instance = new ShoppingCart();
                }
            }
        }
        return instance;
    }
```

> **线程安全并且性能会更好**.

### 工厂模式 (Factory Pattern)

这里我们以最常用的 **“简单工厂 (Simple Factory)”** 和 **“工厂方法 (Factory Method)”** 的混合概念来演示，这是面试手写最常见的形式。

1. 场景

假设你在做一个数据库组件（类似 ShardingSphere），你需要根据配置连接不同的数据库（MySQL, Oracle, PostgreSQL）。用户不应该关心 `new MySQLConnection()` 这种细节，用户只想要一个连接。

2. 手写代码 (Java)

```java
// 1. 定义产品接口 (Product)
interface DatabaseConnection {
    void connect();
}

// 2. 具体产品实现 (Concrete Product)
class MySQLConnection implements DatabaseConnection {
    @Override
    public void connect() {
        System.out.println("Connecting to MySQL...");
    }
}

class OracleConnection implements DatabaseConnection {
    @Override
    public void connect() {
        System.out.println("Connecting to Oracle...");
    }
}

// 3. 工厂类 (Factory) - 负责“生产”对象
class ConnectionFactory {
    // 静态方法，根据参数决定生产什么产品
    public static DatabaseConnection createConnection(String type) {
        if ("MySQL".equalsIgnoreCase(type)) {
            return new MySQLConnection();
        } else if ("Oracle".equalsIgnoreCase(type)) {
            return new OracleConnection();
        }
        throw new IllegalArgumentException("Unknown Database Type");
    }
}

// 4. 客户端调用
public class FactoryDemo {
    public static void main(String[] args) {
        // 用户不需要知道 new MySQLConnection()，只管找工厂要
        DatabaseConnection conn = ConnectionFactory.createConnection("MySQL");
        conn.connect();
    }
}
```

​	**核心逻辑：** 客户端原本依赖具体的 `MySQLConnection`，现在变成了依赖 `ConnectionFactory` 和 `DatabaseConnection` 接口。**创建的控制权交给了工厂。**

## 行为型设计模式

### 策略模式 (Strategy Pattern)

1. 场景

假设你在做一个电商支付系统。用户付款时，可以选择“支付宝”、“微信支付”或者“银联”。虽然都是“付款”这个动作，但具体的算法（调用的API、加密方式）不同。**在运行时，用户想切什么方式就切什么方式。**

2. 手写代码 (Java)

```java
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
        // 今天想用支付宝
        PaymentContext context = new PaymentContext(new AliPayStrategy());
        context.executePayment(100);

        // 突然想换微信了（运行时切换行为）
        context.setStrategy(new WechatPayStrategy());
        context.executePayment(200);
    }
}
```

**核心逻辑：** `PaymentContext` 并不关心具体的支付细节，它只负责调用接口。**具体的算法逻辑被封装在策略类里，可以互相替换。**

------

三、 灵魂拷问：它俩到底有什么区别？

你看上面的代码，感觉结构很像对不对？都是一个 Interface，几个 Impl，然后有一个类去调用它们。

**根本区别在于“目的”：**

| **特性**       | **工厂模式 (Factory)**                                       | **策略模式 (Strategy)**                                      |
| -------------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
| **关注点**     | **创建对象**                                                 | **执行行为**                                                 |
| **返回值**     | 工厂方法**返回一个对象**给你。                               | 策略模式通常**不返回值**（或者返回计算结果），而是去执行某个逻辑。 |
| **客户端认知** | 客户端通常**不知道**具体类是什么，只知道传个参数给工厂（比如传字符串 "MySQL"）。 | 客户端**必须知道**有哪些策略，并**主动选择**一个策略传进去（比如 `new AliPayStrategy()`）。 |
| **一句话总结** | **帮我造个东西出来。**                                       | **帮我把这事儿办了。**                                       |

四、 结合实战 (ShardingSphere 中的应用)

既然你熟悉 ShardingSphere，我们把这两个模式结合起来看，这才是高手的用法：

在 ShardingSphere 中，**工厂模式和策略模式通常是“好基友”**。

1. **策略模式：** ShardingSphere 支持多种分片算法（`Inline`, `Mod`, `Standard`...）。这些算法都实现了 `ShardingAlgorithm` 接口。这就是策略模式，用户可以在 **YAML** 里配置使用哪种算法。
2. **工厂模式：** 但是，用户在 YAML 配置文件里写的是字符串 `"INLINE"`。谁来把这个字符串变成 `InlineShardingAlgorithm` 对象呢？**工厂模式**！

**伪代码示例：**

```java
// 结合使用：用工厂来创建策略
class AlgorithmFactory {
    public static ShardingAlgorithm createAlgorithm(String type) {
        if ("INLINE".equals(type)) return new InlineShardingAlgorithm();
        if ("MOD".equals(type)) return new ModShardingAlgorithm();
        return null;
    }
}

// 客户端
String configType = "INLINE"; // 来自 YAML
ShardingAlgorithm algorithm = AlgorithmFactory.createAlgorithm(configType); // 工厂负责创建
Context context = new Context(algorithm); // 策略负责执行
context.doSharding();
```

#### 总结

- 想**封装创建过程**，不想让外部知道 `new` 的细节 $\rightarrow$ **工厂模式**。
- 想**消除大量的 `if-else`**，让算法可以灵活切换 $\rightarrow$ **策略模式**。





