<!--toc:start-->
- [Emacs常用快捷键](#emacs常用快捷键)
  - [magit相关](#magit相关)
  - [elisp相关](#elisp相关)
  - [Bookmarks](#bookmarks)
  - [buffer相关](#buffer相关)
- [一些lisp](#一些lisp)
  - [内置的一些函数](#内置的一些函数)
  - [describe-of-function](#describe-of-function)
  - [describe-of-variable](#describe-of-variable)
<!--toc:end-->

# Emacs常用快捷键

## magit相关

1. c-x g 打开magit的状态面板。

2. s 暂存 S 所有暂存

3. u 取消暂存 U 取消所有

4. c-c 开始提交，输入后用c-c c-c完成提交

5. c-a修改最后一次提交

6. b 是branch相关的操作的触发键

7. F 是拉取代码相关的

8. d 展示diff

9. m 是Merge相关的

10. r是rebase相关的

11. t是tags相关的

12. z是暂存区相关的

13. !是reflog

14. E是编辑git配置文件

## elisp相关

1. c-x b切换buffer/file。

2. c-h f查看函数文档

## Bookmarks

1. C-x r m: 在当前位置设置书签

2. C-x r b: 跳转到书签

3. C-x r l: 列出所有书签

## buffer相关

1. c-x h拷贝整个buffer

# 一些lisp

## 内置的一些函数

buffer-name，能获取到当前这个buffer的名称。总之就是buffer相关的方法都会带有buffer。

## describe-of-function

c-h f 查看一个函数的信息。

## describe-of-variable

c-h v查看一个变量的消息。
