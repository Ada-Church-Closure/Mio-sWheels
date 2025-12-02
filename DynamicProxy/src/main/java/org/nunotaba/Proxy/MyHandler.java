package org.nunotaba.Proxy;

public interface MyHandler {
    /**
     * 给函数名,返回String串
     * @param methodName 函数名
     * @return 执行逻辑的String串
     */
    String functionBody(String methodName);

    /**
     * 初始化传入代理类中的对象
     * @param proxy 实现的对象
     */
    default void setProxy(MyInterface proxy) throws IllegalAccessException, NoSuchFieldException {

    }
}
