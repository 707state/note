# RN

1. 虚拟DOM（Virtual DOM）的抽象层设计
React Native 基于 React 的虚拟 DOM 机制，通过 JavaScript 构建轻量级的 UI 描述（即 Virtual DOM），而不是直接操作原生 UI 组件。

核心原理：
开发者编写的 JSX 会被编译为虚拟 DOM 树（JavaScript 对象表示）。
当状态变化时，React 会生成新的虚拟 DOM，并通过 Diff 算法 计算出最小变更集（仅更新必要的部分）。
最终通过 Bridge 将变更映射到原生组件（如 Android 的 View 或 iOS 的 UIView）。
跨平台意义：
虚拟 DOM 是平台无关的抽象层，同一套 JavaScript 逻辑可生成不同平台的原生 UI。

2. JavaScript 与原生模块的通信桥梁（Bridge）
Bridge 是 React Native 实现跨平台的核心模块，负责 JS 线程与原生线程的异步通信。

工作流程：
序列化通信：JS 和原生代码通过 JSON 消息传递（如调用 UIManager 创建视图）。
异步队列：Bridge 将通信任务放入队列，避免阻塞主线程。
模块注册：原生模块（如相机、蓝牙）需提前注册到 Bridge，供 JS 调用。
性能瓶颈：
频繁的 Bridge 通信会导致延迟（如滚动列表），因此高性能场景需直接使用原生组件。

3. 平台特定组件（Platform-Specific Components）的封装策略
React Native 通过分层设计兼容平台差异：

通用组件：如 <View>、<Text>，在 JS 层统一，但渲染为不同原生组件（Android 的 ViewGroup/iOS 的 UIView）。
平台扩展：
文件后缀区分（如 Component.ios.js 和 Component.android.js）。
使用 Platform.select() 动态加载不同代码。
原生封装：
复杂功能（如地图）需通过原生模块（Java/Objective-C）实现，再暴露接口给 JS。

4. 线程模型与异步渲染机制
React Native 的多线程架构确保流畅性：

线程分工：
JS 线程：运行 JavaScript 逻辑和虚拟 DOM 计算。
UI 线程（主线程）：处理原生渲染和用户交互。
Shadow 线程（后台）：计算布局（Yoga 引擎）。
异步渲染：
JS 线程将样式和布局信息通过 Bridge 发送到 Shadow 线程，计算完成后由 UI 线程渲染，避免阻塞。

## RN新老架构比较

老架构	新架构（Fabric）
基于 Bridge 的异步通信（JSON 序列化消息）	使用 JSI（JavaScript Interface） 实现 同步通信
JS 与原生代码通过 Bridge 传递消息，存在序列化/反序列化开销	JS 可直接调用原生方法（C++ 层），无需序列化
高频操作（如滚动列表）易卡顿	减少延迟，提升交互流畅度

老架构的 Bridge 类似“邮局”，消息需排队处理；
JSI 允许 JS 直接持有原生对象的引用（如 UIManager），类似 React DOM 中的 Fiber 架构。

老架构	新架构（Fabric）
三线程模型（JS、UI、Shadow 线程）	简化线程模型，Shadow 树计算移至 C++ 层
布局计算（Yoga）在 Shadow 线程完成	Yoga 布局直接集成到 C++ 核心，减少线程切换
UI 更新需跨线程多次通信	直接由 C++ 驱动渲染，减少中间环节

新架构通过 TurboModules 按需加载原生模块，减少启动时间。

老架构	新架构（Fabric）
组件依赖 Bridge 的运行时绑定	静态类型化组件（通过 Codegen 生成接口）
反射机制动态查找原生模块	编译时生成 C++ 和 JS 的类型化接口
灵活性高，但调试困难	类型安全，减少运行时错误


老架构中 <View> 的属性需通过 Bridge 传递；
新架构中属性直接映射为 C++ 结构体，编译时验证。

老架构	新架构（Fabric）
平台差异由 JS 层适配（如 Platform.select）	C++ 核心统一跨平台逻辑
不同平台可能行为不一致	布局和事件处理在 C++ 层标准化
依赖社区维护平台兼容性	官方控制核心实现，减少碎片化

手势系统、动画（如 Reanimated）直接基于 C++ 实现，避免 Bridge 瓶颈

老架构	新架构（Fabric）
依赖 Chrome DevTools 间接调试	原生调试支持（如 Flipper 深度集成）
Bridge 通信问题难追踪	JSI 提供更直接的错误堆栈
热重载不稳定	改进的 Fast Refresh 机制
