# Widget

在Flutter中几乎所有的对象都是一个 widget 。与原生开发中“控件”不同的是，Flutter 中的 widget 的概念更广泛，它不仅可以表示UI元素，也可以表示一些功能性的组件如：用于手势检测的 GestureDetector 、用于APP主题数据传递的 Theme 等等，而原生开发中的控件通常只是指UI元素。在后面的内容中，我们在描述UI元素时可能会用到“控件”、“组件”这样的概念，读者心里需要知道他们就是 widget ，只是在不同场景的不同表述而已。由于 Flutter 主要就是用于构建用户界面的，所以，在大多数时候，读者可以认为 widget 就是一个控件，不必纠结于概念。

Flutter 中是通过 Widget 嵌套 Widget 的方式来构建UI和进行事件处理的，所以记住，Flutter 中万物皆为Widget。


## Widget接口

在 Flutter 中， widget 的功能是“描述一个UI元素的配置信息”，它就是说， Widget 其实并不是表示最终绘制在设备屏幕上的显示元素，所谓的配置信息就是 Widget 接收的参数，比如对于 Text 来讲，文本的内容、对齐方式、文本样式都是它的配置信息。

## Flutter中的四棵树

既然 Widget 只是描述一个UI元素的配置信息，那么真正的布局、绘制是由谁来完成的呢？Flutter 框架的处理流程是这样的：

    根据 Widget 树生成一个 Element 树，Element 树中的节点都继承自 Element 类。
    根据 Element 树生成 Render 树（渲染树），渲染树中的节点都继承自RenderObject 类。
    根据渲染树生成 Layer 树，然后上屏显示，Layer 树中的节点都继承自 Layer 类。

真正的布局和渲染逻辑在 Render 树中，Element 是 Widget 和 RenderObject 的粘合剂，可以理解为一个中间代理。

## StatelessWidget

StatelessWidget相对比较简单，它继承自widget类，重写了createElement()

StatelessElement 间接继承自Element类，与StatelessWidget相对应（作为其配置数据）。

StatelessWidget用于不需要维护状态的场景，它通常在build方法中通过嵌套其他 widget 来构建UI，在构建过程中会递归的构建其嵌套的 widget 。

### Context

build方法有一个context参数，它是BuildContext类的一个实例，表示当前 widget 在 widget 树中的上下文，每一个 widget 都会对应一个 context 对象（因为每一个 widget 都是 widget 树上的一个节点）。实际上，context是当前 widget 在 widget 树中位置中执行”相关操作“的一个句柄(handle)，比如它提供了从当前 widget 开始向上遍历 widget 树以及按照 widget 类型查找父级 widget 的方法。

## StatefulWidget

和StatelessWidget一样，StatefulWidget也是继承自widget类，并重写了createElement()方法，不同的是返回的Element 对象并不相同；另外StatefulWidget类中添加了一个新的接口createState()。

StatefulElement 间接继承自Element类，与StatefulWidget相对应（作为其配置数据）。StatefulElement中可能会多次调用createState()来创建状态（State）对象。

createState() 用于创建和 StatefulWidget 相关的状态，它在StatefulWidget 的生命周期中可能会被多次调用。例如，当一个 StatefulWidget 同时插入到 widget 树的多个位置时，Flutter 框架就会调用该方法为每一个位置生成一个独立的State实例，其实，本质上就是一个StatefulElement对应一个State实例。

## State

一个 StatefulWidget 类会对应一个 State 类，State表示与其对应的 StatefulWidget 要维护的状态，State 中的保存的状态信息可以：

    在 widget 构建时可以被同步读取。
    在 widget 生命周期中可以被改变，当State被改变时，可以手动调用其setState()方法通知Flutter 框架状态发生改变，Flutter 框架在收到消息后，会重新调用其build方法重新构建 widget 树，从而达到更新UI的目的。


State 中有两个常用属性：

    widget，它表示与该 State 实例关联的 widget 实例，由Flutter 框架动态设置。注意，这种关联并非永久的，因为在应用生命周期中，UI树上的某一个节点的 widget 实例在重新构建时可能会变化，但State实例只会在第一次插入到树中时被创建，当在重新构建时，如果 widget 被修改了，Flutter 框架会动态设置State. widget 为新的 widget 实例。

    context。StatefulWidget对应的 BuildContext，作用同StatelessWidget 的BuildContext。

### State生命周期

initState：当 widget 第一次插入到 widget 树时会被调用，对于每一个State对象，Flutter 框架只会调用一次该回调，所以，通常在该回调中做一些一次性的操作，如状态初始化、订阅子树的事件通知等。不能在该回调中调用BuildContext.dependOnInheritedWidgetOfExactType（该方法用于在 widget 树上获取离当前 widget 最近的一个父级InheritedWidget，关于InheritedWidget我们将在后面章节介绍），原因是在初始化完成后， widget 树中的InheritFrom widget也可能会发生变化，所以正确的做法应该在在build（）方法或didChangeDependencies()中调用它。

didChangeDependencies()：当State对象的依赖发生变化时会被调用；例如：在之前build() 中包含了一个InheritedWidget （第七章介绍），然后在之后的build() 中Inherited widget发生了变化，那么此时InheritedWidget的子 widget 的didChangeDependencies()回调都会被调用。典型的场景是当系统语言 Locale 或应用主题改变时，Flutter 框架会通知 widget 调用此回调。需要注意，组件第一次被创建后挂载的时候（包括重创建）对应的didChangeDependencies也会被调用。

build()：此回调读者现在应该已经相当熟悉了，它主要是用于构建 widget 子树的，会在如下场景被调用：

    在调用initState()之后。
    在调用didUpdateWidget()之后。
    在调用setState()之后。
    在调用didChangeDependencies()之后。
    在State对象从树中一个位置移除后（会调用deactivate）又重新插入到树的其他位置之后。

reassemble()：此回调是专门为了开发调试而提供的，在热重载(hot reload)时会被调用，此回调在Release模式下永远不会被调用。

    didUpdateWidget ()：在 widget 重新构建时，Flutter 框架会调用widget.canUpdate来检测 widget 树中同一位置的新旧节点，然后决定是否需要更新，如果widget.canUpdate返回true则会调用此回调。正如之前所述，widget.canUpdate会在新旧 widget 的 key 和 runtimeType 同时相等时会返回true，也就是说在在新旧 widget 的key和runtimeType同时相等时didUpdateWidget()就会被调用。

    deactivate()：当 State 对象从树中被移除时，会调用此回调。在一些场景下，Flutter 框架会将 State 对象重新插到树中，如包含此 State 对象的子树在树的一个位置移动到另一个位置时（可以通过GlobalKey 来实现）。如果移除后没有重新插入到树中则紧接着会调用dispose()方法。

    dispose()：当 State 对象从树中被永久移除时调用；通常在此回调中释放资源。


# 状态管理

响应式的编程框架中都会有一个永恒的主题——“状态(State)管理”，无论是在 React/Vue（两者都是支持响应式编程的 Web 开发框架）还是 Flutter 中，他们讨论的问题和解决的思想都是一致的。

## Widget管理自身状态

我们实现一个TapboxA，在它对应的_TapboxAState 类:

管理TapboxA的状态。
定义_active：确定盒子的当前颜色的布尔值。
定义_handleTap()函数，该函数在点击该盒子时更新_active，并调用setState()更新UI。
实现widget的所有交互式行为。

## 父Widget管理子Widget的状态

对于父Widget来说，管理状态并告诉其子Widget何时更新通常是比较好的方式。 例如，IconButton是一个图标按钮，但它是一个无状态的Widget，因为我们认为父Widget需要知道该按钮是否被点击来采取相应的处理。


