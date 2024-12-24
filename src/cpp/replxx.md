# C++的补全库

怎么实现tab补全呢？一般而言，很少有人会考虑这个问题，但是怎么实现确实有点讲究。

## 一般的实现机制

completion一般来说，有字典树方案、正则匹配方案等。

字典树方案就是匹配相同字符，根据相同前缀来提示。

正则匹配涉及到正则引擎的工作方式，常规的实现和AC自动机这样的实现会带来不同的效果。

## replxx

set_completion_callback, set_hint_callback, set_highlighter_callback这三个接口能够让用户自定义补全和颜色，从而简化补全的实现。