# createSignal

响应式基础，类似于ref或者useState。

但是原理与Vue/React并不一样。

首先Solid并不操作VDOM而是直接操作DOM, 开销更小；

React支持Functional Component和Class Component, 而SolidJS只有Functional Component；

React的核心是state和props, 其中props是JSX所带来的特性。

React的生命周期里面的几个API: componentDidMount, componentDidUpdate, componentWillUnmount这些在Solid里面都要清晰得多，有onMoun方法。

