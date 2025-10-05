<!--toc:start-->
- [guard](#guard)
- [willSet](#willset)
- [didSet](#didset)
- [五种访问控制级别](#五种访问控制级别)
  - [open](#open)
  - [public](#public)
  - [internal](#internal)
  - [fileprivate](#fileprivate)
  - [private](#private)
- [actor](#actor)
- [SwiftUI相关](#swiftui相关)
  - [MainActor](#mainactor)
    - [背景](#背景)
  - [AVFoundation](#avfoundation)
    - [元数据解析](#元数据解析)
<!--toc:end-->


# guard

这个特性我确实没见过。

guard是Swift中的一个条件语句，主要用于提前退出函数、方法或循环，它帮助开发者处理代码中的前置条件。

```swift
guard 条件 else {
    // 条件不满足时执行的代码
    // 必须包含控制转移语句（return, break, continue, throw等）
}
// 条件满足时继续执行的代码
```

- 提前退出：不满足条件时立即退出当前作用域
- 变量绑定：可以在条件中绑定可选值，绑定的变量在guard语句后仍然可用
- 提高代码可读性：减少嵌套层级，使代码更清晰
- 必须转移控制：else分支必须转移控制流（如return、throw等）

# willSet

Swift中的一个属性观察器(Property Observer)，它允许你在属性值即将被改变之前执行自定义代码。

```swift
var propertyName: Type = initialValue {
    willSet(newName) {
        // 属性值即将被改变前执行的代码
        // newName是即将设置的新值
    }
}
```

- 执行时机：在属性值被设置之前触发
- 访问新值：可以通过参数（默认为newValue）访问即将设置的新值
- 只读性：在willSet代码块中，属性仍然保持旧值
- 适用范围：可用于存储属性（不适用于计算属性）
- 继承性：子类继承的属性观察器会在父类的属性观察器之后被调用

# didSet

在属性值改变之后调用。

# 五种访问控制级别

## open

最高级别的访问权限

适用范围：
- 类及其成员
模块外访问：
- 可以
模块外继承/重写：
- 可以
特点：
- 只能应用于类和类成员
- 允许其他模块继承该类并重写其方法
- 框架设计中用于明确指定可被外部继承的基类

## public

公开访问级别

适用范围：
- 所有实体
模块外访问：
- 可以
模块外继承/重写：
- 不可以
特点：
- 可用于任何类型、函数或属性
- 外部模块可以使用但不能继承或重写
- 适合定义框架的公共API

## internal

内部访问级别（默认）

适用范围：
- 所有实体
模块内访问：
- 可以
模块外访问：
- 不可以
特点：
- Swift 的默认访问级别
- 限制实体只能在定义它的模块内使用
- 适合模块内部实现细节

## fileprivate

文件私有访问级别

适用范围：
- 所有实体
文件内访问：
- 可以
文件外访问：
- 不可以
特点：
- 限制实体只能在定义它的源文件内使用
- 适合隐藏同一文件中多个类型实现的共享细节

## private

私有访问级别

适用范围：
- 所有实体
封闭声明内访问：
- 可以
封闭声明外访问：
- 不可以
特点：
- 最严格的访问级别
- 限制实体只能在定义它的声明内使用
- 适合隐藏特定类、结构体或枚举的实现细节

# actor

修改 actor 的状态需要发邮件，actor 会在收到邮件之后一个一个处理并异步返回给你结果，这个叫做 actor-isolated（即属性隔离）。

actor 的状态只能在自己的函数内部修改，是因为 actor 的函数的调用是在对应的 executor 上安全地执行的。如果外部的函数也能够满足这个调用条件，那么理论上也是安全的。

Swift 提供了 actor-isolated paramters 这样的特性，字面意思即满足 actor 状态隔离的参数，如果我们在定义外部函数时将需要访问的 actor 类型的参数声明为 isolated，那么我们就可以在函数内部修改这个 actor 的状态了。

# SwiftUI相关

这两天写了一个minimal的音乐播放器，因为主力电脑是一个Mac mini，所以这里就直接用了swift来写，不考虑什么跨平台之类的破事。

但是没想到用swift也遇上了一些抽象问题。

这里记录一下。

## MainActor
自 Swift 6 起，所有会导致 UI 更新的操作必须在主线程（MainActor）上执行。

### 背景

Swift 6 加强了并发模型的类型安全检查，引入了「actor 隔离」的严格模式。

SwiftUI 的视图更新系统本质上是 主线程（MainActor）隔离的。

因此，如果在非主线程（比如后台任务）里直接修改 UI 状态（例如 @State、@Published、@EnvironmentObject），编译器会报错。

这里可以采用Task包裹一个MainActor.run的方式来写，或者干脆就标记为@MainActor。

这个机制我认为是合理的，但是上手还是有点难度。

## AVFoundation

音频/视频不可避免地需要使用到Apple在自家平台上的AVFoundation了，这个东西其实还是好用的，问题在于文档太难懂了。

### 元数据解析

[Apple的文档上对于元数据解析](https://developer.apple.com/documentation/avfoundation/retrieving-media-metadata) 的描述可以说是非常简陋。

不过确实是好好看了文档结合debug才搞定音频文件的cover的加载。

```swift
// A local or remote asset to inspect.
let asset = AVURLAsset(url: url)
for format in try await asset.load(.availableMetadataFormats) {
    let metadata = try await asset.loadMetadata(for: format)
    // Process the format-specific metadata collection.
    print("The available metadata format is \(metadata)")
}
```

这段代码实际上是说的尝试所有可能的AssetLoader，换句话说你不能事先知道这个文件怎么才能被解析。

在我的情况下，我需要加载mp3/flac文件，我问AI非常多次，并且也搜索了大量资料，然而没有一个资料对于AssetLoader的内容讲清楚的。这就导致我不停的获得了空的metadata。

后来看了Apple官方文档，发现这个东西居然是需要试出来的，并没有一个固定的。

```swift
let metadata=try await asset.loadMetadata(for: .init(rawValue: "org.xiph.vorbis-comment"))
```

最后试出来这样做就可以了。

只能说还得是官方文档。
