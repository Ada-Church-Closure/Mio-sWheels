package org.nunotaba.Proxy;

import org.nunotaba.Compiler;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.nio.file.Files;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * 创建我们需要对象的工厂.
 */
public class MyInterfaceFactory {
    static AtomicInteger count = new AtomicInteger(0);
    private static File createJavaFile(String className, MyHandler myHandler) throws IOException {

        // 在创建类加载的过程中,类名应该是动态的
        String func1Body = myHandler.functionBody("func1");
        String func2Body = myHandler.functionBody("func2");
        String func3Body = myHandler.functionBody("func3");

        String context = "package org.nunotaba;\n" +
                "\n" +
                "import org.nunotaba.Proxy.MyInterface;\n" +
                "\n" +
                "public class " + className +" implements MyInterface {\n" +
                "MyInterface myInterface;\n" +
                "    @Override\n" +
                "    public void func1() {\n" +
                "        " + func1Body +"\n" +
                "    }\n" +
                "\n" +
                "    @Override\n" +
                "    public void func2() {\n" +
                "        " + func2Body + "\n" +
                "    }\n" +
                "\n" +
                "    @Override\n" +
                "    public void func3() {\n" +
                "        " + func3Body + "\n" +
                "    }\n" +
                "}";
        File javaFile = new File("./src/main/java/org/nunotaba/" + className + ".java");
        Files.writeString(javaFile.toPath(), context);
        return javaFile;
    }

    /**
     * 计数器保证每次生成的className不一样
     * @return 新生成的className
     */
    private static String getClassName(){
        return "MyInterface$proxy" + count.incrementAndGet();
    }


    /**
     * 生成class字节码文件之后,我们就可以把这个文件加载进入jvm
     * @param className 类名
     * @throws Exception 不处理所有异常
     * 你发现,通过自己指定一些实现,你创建了一个接口的实例......
     */
    private static MyInterface newInstance(String className, MyHandler handler) throws Exception{
        Class<?> aClass = MyInterfaceFactory.class.getClassLoader().loadClass(className);
        Constructor<?> constructor = aClass.getConstructor();
        MyInterface proxy = (MyInterface) constructor.newInstance();
        handler.setProxy(proxy);
        return proxy;
    }

    /**
     * 返回动态代理的对象
     * @return 利用接口创建的实例
     * @throws Exception 不处理异常
     */
    public static MyInterface createProxyObject(MyHandler myHandler) throws Exception{
        String className = getClassName();
        File javaFile = createJavaFile(className, myHandler);
        Compiler.compile(javaFile);
        // 注意这里要使用全类名.
        return newInstance("org.nunotaba." + className, myHandler);
    }

}
