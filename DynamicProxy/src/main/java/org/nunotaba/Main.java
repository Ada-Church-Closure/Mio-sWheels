package org.nunotaba;

import org.nunotaba.Impl.LogHandler;
import org.nunotaba.Proxy.MyHandler;
import org.nunotaba.Proxy.MyInterface;
import org.nunotaba.Proxy.MyInterfaceFactory;

public class Main {
    public static void main(String[] args) throws Exception {
        MyInterface proxyObject = MyInterfaceFactory.createProxyObject(new PrintFunctionName());
        proxyObject = MyInterfaceFactory.createProxyObject(new LogHandler(proxyObject));
        proxyObject.func1();
        proxyObject.func2();
        proxyObject.func3();
    }

    static class PrintFunctionName implements MyHandler{
        @Override
        public String functionBody(String methodName){
            return "System.out.println(\"" + methodName + "\");";
        }
    }

    /**
     * 还可以进行一些其余的功能增强,注意,我们一直都在写class,也就是字符串
     * 而不是在书写真正的逻辑
     */
    static class getTimeAndPrintFunctionName implements MyHandler{

        @Override
        public String functionBody(String methodName) {
            StringBuilder stringBuilder = new StringBuilder();
            stringBuilder.append("System.out.println(System.currentTimeMillis());\n").append("System.out.println(\"" + methodName + "\");");

            return stringBuilder.toString();
        }
    }

}