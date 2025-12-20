package org.nunotaba.CreationalPattern.Singleton;

import java.util.ArrayList;
import java.util.List;

// 购物车管理器（单例模式）
class ShoppingCart {
    // 1. 私有静态变量，保存唯一实例
    private static ShoppingCart instance;

    // 2. 商品存储：商品名称 -> 数量
    private final List<String> names = new ArrayList<>();
    private final List<Integer> count = new ArrayList<>();

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
