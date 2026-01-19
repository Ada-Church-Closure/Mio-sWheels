package org.nunotaba;

import java.util.LinkedList;
import java.util.Queue;

// 面试的时候要会层序遍历建树的方法即可,这本身就相当于一个中等的力扣了
public class Main {
    static class Node{
        Node left;
        Node right;
        int value;
        Node(){

        }
        Node (int value) {
            this.value = value;
        }
    }

    public static Node buildTree(Integer[] nums) {
        if (nums == null || nums.length == 0) {
            return null;
        }

        Node root = new Node(nums[0]);
        Queue<Node> q = new LinkedList<>();
        q.offer(root);
        int index = 1;
        while (index < nums.length) {
            Node node = q.poll();
            if (nums[index] != null) {
                Node leftNode = new Node(nums[index]);
                q.offer(leftNode);
                node.left = leftNode;
            }
            ++index;

            if (index < nums.length && nums[index] != null) {
                Node rightNode = new Node(nums[index]);
                q.offer(rightNode);
                node.right = rightNode;
            }
            ++index;
        }
        return root;
    }
    public static void main(String[] args) {
        Integer[] input = {1, 2, 3, null, null, 4, 5};
        Node root = buildTree(input);
        // System.out.println("Root val: " + root.value); // 验证一下
        dfs(root);
    }

    private static void dfs(Node root) {
        if (root == null) {
            return;
        }

        dfs(root.left);
        System.out.println("Node:val " + root.value);
        dfs(root.right);
    }
}