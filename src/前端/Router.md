## SPA的路由

传统的HTML的路由是通过多个HTML文件之间进行切换实现的，而在SPA的设计中，一个应用只有一个HTML文件，在HTML文件中包含一个占位符，占位符对应的内容由每个视图决定，页面之间的切换就是视图的切换。

但是SPA带来了两个问题：

1. SPA 无法记住用户的操作记录，无论是刷新、前进还是后退，都无法展示用户真实的期望内容。

2. SPA 中虽然由于业务的不同会有多种页面展示形式，但只有一个 url，对 SEO 不友好，不方便搜索引擎进行收录。

所以前端路由诞生了。

## 什么是前端路由

保证只有一个 HTML 页面，且与用户交互时不刷新和跳转页面的同时，为 SPA 中的每个视图展示形式匹配一个特殊的 url。在刷新、前进、后退和SEO时均通过这个特殊的 url 来实现。

要实现这一目标，需要做到：

1. 改变URL且不让浏览器发送请求。

2. 监听URL的变化。

### Hash模式

这里的 hash 就是指 url 后的 # 号以及后面的字符。比如说 "www.baidu.com/#hashhash" ，其中 "#hashhash" 就是我们期望的 hash 值。
由于 hash 值的变化不会导致浏览器像服务器发送请求，而且 hash 的改变会触发 hashchange 事件，浏览器的前进后退也能对其进行控制，所以在 H5 的 history 模式出现之前，基本都是使用 hash 模式来实现前端路由。

### History模式

HTML5引入了 history.pushState() 和 history.replaceState() 方法，它们分别可以添加和修改历史记录条目。这些方法通常与window.onpopstate 配合使用。

history.pushState() 和 history.replaceState() 均接收三个参数（state, title, url）
参数说明如下：

state：合法的 Javascript 对象，可以用在 popstate 事件中
title：现在大多浏览器忽略这个参数，可以直接用 null 代替
url：任意有效的 URL，用于更新浏览器的地址栏

history.pushState() 和 history.replaceState() 的区别在于：

history.pushState() 在保留现有历史记录的同时，将 url 加入到历史记录中。
history.replaceState() 会将历史记录中的当前页面历史替换为 url。

由于 history.pushState() 和 history.replaceState() 可以改变 url 同时，不会刷新页面，所以在 HTML5 中的 histroy 具备了实现前端路由的能力。

对于单页应用的 history 模式而言，url 的改变只能由下面四种方式引起：

点击浏览器的前进或后退按钮
点击 a 标签
在 JS 代码中触发 history.pushState 函数
在 JS 代码中触发 history.replaceState 函数


# 总结

前端路由本质上就是在单页面内进行切换，需要用hash或者history模式。
