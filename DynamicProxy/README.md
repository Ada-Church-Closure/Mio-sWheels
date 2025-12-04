# 手写动态代理

## 代理基本机制--->替换

## 传什么参数?

## 事务失效原理--->spring不熟.

> 简单来说就是没有走代理对象......

这三个概念的关系是层层递进的：

1. **反射 (Reflection)** 是地基。
2. **动态代理 (Dynamic Proxy)** 是基于反射构建的机制，也是 Spring AOP 的核心。
3. **事务失效** 是由于动态代理的**“类内调用”**特性导致的一个经典 Bug。

结合你的背景，我们来深挖一下。

------

### 一、 底层地基：反射与动态代理

Spring 的声明式事务（`@Transactional`）本质上就是 **AOP（面向切面编程）**。而 AOP 在 Java 中的实现主要依赖于**动态代理**。

Spring 会在启动时，为那些加了 `@Transactional` 的 Bean 创建一个**代理对象（Proxy）**。

#### 1. 两种代理方式（必考）

- **JDK 动态代理：**
  - **基于接口**：目标类必须实现接口。
  - **原理**：利用 `java.lang.reflect.Proxy` 类和 `InvocationHandler` 接口。JVM 在运行时动态生成一个实现了该接口的 `$Proxy0` 类。
  - **反射的体现**：在 `invoke()` 方法中，使用 `method.invoke(target, args)` 来调用原始对象的方法。
- **CGLIB 动态代理：**
  - **基于继承**：目标类不需要接口。
  - **原理**：利用 ASM 字节码技术，动态生成一个目标类的**子类**，并重写其中的方法。
  - **反射的体现**：使用 `MethodProxy` 调用父类（目标类）的方法。

------

### 二、 Spring 事务的执行原理（伪代码级理解）

当你在 `UserService` 上加了 `@Transactional` 时，Spring 容器里存的 `userService` Bean 实际上**不是**你写的那个 `UserService` 对象，而是一个**代理对象**。

**代理对象的逻辑大概是这样的：**

Java

```java
// 这是 Spring 生成的代理类逻辑（简化版）
public class UserServiceProxy {
    
    private UserService target; // 指向原本的 Service 对象
    private TransactionManager txManager;

    // 当你调用 userService.saveUser() 时，实际执行的是这个：
    public void saveUser() {
        try {
            // 1. 【前置增强】开启事务
            txManager.begin();
            
            // 2. 【反射调用】执行目标对象的方法
            // 这就是你写的业务逻辑
            target.saveUser(); 
            
            // 3. 【后置增强】提交事务
            txManager.commit();
        } catch (Exception e) {
            // 4. 【异常增强】回滚事务
            txManager.rollback();
            throw e;
        }
    }
}
```

------

### 三、 事务失效的经典场景：自调用 (Self-Invocation)

这是面试的重灾区。

#### 场景描述

假设你有一个类，里面有两个方法 `A` 和 `B`。

- 方法 A：**没有** `@Transactional`。
- 方法 B：**有** `@Transactional`。
- **关键点：** 方法 A 在内部调用了方法 B。

Java

```java
@Service
public class OrderService {

    // 方法 A：普通方法
    public void createOrder() {
        // 逻辑...
        // 关键点：这里发生了【类内部调用】
        this.saveOrder(); 
    }

    // 方法 B：事务方法
    @Transactional
    public void saveOrder() {
        // 数据库操作...
        throw new RuntimeException("故意报错");
    }
}
```

#### 问：外部调用 `createOrder()` 时，`saveOrder()` 的事务会生效吗？会回滚吗？

**答：不会生效，不会回滚。事务失效了。**

------

### 四、 失效原理深度剖析

为什么会失效？核心在于 **`this`** 这个关键字。

#### 1. 调用流程还原

当你从 Controller 调用 `orderService.createOrder()` 时：

1. 第一步（过代理）：

   调用的是 代理对象 的 createOrder() 方法。

   - 代理对象检查 `createOrder()` 方法 —— **发现没有 `@Transactional` 注解**。
   - 于是代理对象**不开启事务**，直接通过反射调用 **目标对象（Target）** 的 `createOrder()`。

2. 第二步（进目标）：

   现在代码执行权已经到了 目标对象（原始对象） 内部。

   Java

   ```java
   public void createOrder() {
       // ...
       this.saveOrder(); // 这里的 this 指的是谁？
   }
   ```

   - 这里的 `this` 指的是 **目标对象本身 (Target Object)**，而不是代理对象。

3. 第三步（绕过代理）：

   执行 this.saveOrder() 时，相当于直接在原始对象上调用方法。

   - **根本没有经过代理对象的拦截逻辑！**
   - 没有经过代理，就没有 `txManager.begin()` 和 `try-catch-rollback`。
   - 所以，虽然 `saveOrder` 上有注解，但它只是作为一个普通方法被执行了。

#### 形象的比喻

- **代理对象**就像**大门的安检员**。
- **@Transactional** 就像**安检流程**。
- **方法 A** 是一个**不需要安检的侧门**。
- **方法 B** 是一个**核心机房（需要安检）**。

你从侧门（方法 A）进去了，绕过了安检员。进到楼里后，你自己走到了核心机房（方法 B）。因为你已经在楼里了（类内部），安检员（代理）根本管不到你，所以你直接就进去了，没有触发安检（事务）。

------

### 五、 怎么解决？

面试官问完原理，肯定会问怎么解决。

**方法 1：把自己注入给自己（最常用）**

Java

```java
@Service
public class OrderService {
    @Autowired
    private OrderService self; // 注入代理对象

    public void createOrder() {
        // 调用代理对象的 saveOrder，而不是 this.saveOrder
        self.saveOrder(); 
    }
}
```

方法 2：从 AopContext 获取代理对象（最硬核）

需要开启 @EnableAspectJAutoProxy(exposeProxy = true)。

Java

```java
public void createOrder() {
    ((OrderService) AopContext.currentProxy()).saveOrder();
}
```

方法 3：拆分文件（最规范）

将 saveOrder 移到另一个 Service (OrderDbService) 中，然后注入调用。这是符合单一职责原则的最佳实践。

------

### 六、 总结

如果面试官问：“Spring 事务失效的原理是什么？”

**你的回答逻辑：**

1. **定性：** Spring 事务基于 **AOP 动态代理**实现。
2. **机制：** 只有通过**代理对象**调用的方法，才能被拦截，从而在方法前后插入事务开启和提交的逻辑。
3. **原因：** 当发生**类内部调用（`this.method()`）时，`this` 指向的是目标对象**而非代理对象。这意味着代码直接在原始对象上执行，**绕过了代理对象的增强逻辑**，导致事务注解被忽略，从而失效。
4. **补充：** 解决办法是获取当前的代理对象去调用，或者将方法拆分到不同的 Bean 中。