package org.nunotaba.Impl;

import org.nunotaba.Proxy.MyHandler;
import org.nunotaba.Proxy.MyInterface;

import java.lang.reflect.Field;


public class LogHandler implements MyHandler {

    MyInterface myInterface;
    public LogHandler(MyInterface myInterface){
        this.myInterface = myInterface;
    }

    @Override
    public String functionBody(String methodName) {
        return "System.out.println(\"before\");\n" +
                "        myInterface." + methodName +"();\n" +
                "        System.out.println(\"after\");";
    }

    /**
     * 利用反射给仅有的属性进行赋值的操作
     * @param proxy 实现的对象
     */
    @Override
    public void setProxy(MyInterface proxy) throws IllegalAccessException, NoSuchFieldException {
        Class<? extends MyInterface> aClass = proxy.getClass();
        Field field = aClass.getDeclaredField("myInterface");
        // 可以访问
        field.setAccessible(true);
        field.set(proxy, myInterface);
    }
}
