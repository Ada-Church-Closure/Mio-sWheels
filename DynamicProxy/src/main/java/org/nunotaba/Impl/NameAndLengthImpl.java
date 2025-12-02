package org.nunotaba.Impl;

import org.nunotaba.Proxy.MyInterface;

public class NameAndLengthImpl implements MyInterface {
    @Override
    public void func1() {
        String method = "func1";
        System.out.println(method);
        System.out.println(method.length());
    }

    @Override
    public void func2() {
        String method = "func2";
        System.out.println(method);
        System.out.println(method.length());
    }

    @Override
    public void func3() {
        String method = "func3";
        System.out.println(method);
        System.out.println(method.length());
    }
}
