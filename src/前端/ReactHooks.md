<!--toc:start-->
- [常用Hooks](#常用hooks)
  - [useState](#usestate)
  - [useEffect](#useeffect)
  - [useCallback](#usecallback)
  - [useContext](#usecontext)
  - [useMemo](#usememo)
- [原理](#原理)
  - [Fiber 节点与 Hook 链表](#fiber-节点与-hook-链表)
  - [useState 的实现原理](#usestate-的实现原理)
  - [useEffect 的实现原理](#useeffect-的实现原理)
  - [useCallback实现原理](#usecallback实现原理)
  - [useMemo实现原理](#usememo实现原理)
- [为什么Hooks有使用限制](#为什么hooks有使用限制)
  - [规则](#规则)
- [自定义Hook？](#自定义hook)
  - [useAsync Hook](#useasync-hook)
- [Class和Hook？](#class和hook)
  - [代码复用更简单](#代码复用更简单)
  - [逻辑关注点分离](#逻辑关注点分离)
  - [TypeScript支持更好](#typescript支持更好)
  - [更小的包体积](#更小的包体积)
<!--toc:end-->

# 常用Hooks
## useState
用于状态管理。
```jsx
import React, { useState } from 'react';

function Counter() {
    const [count, setCount] = useState(0);

    return (
        <div>
            <p>当前计数: {count}</p>
            <button onClick={() => setCount(count + 1)}>
                增加
            </button>
        </div>
    );
}
```

## useEffect
用于处理函数副作用。
```jsx
function UserProfile({ userId }) {
    const [user, setUser] = useState(null);
    const [loading, setLoading] = useState(true);

    useEffect(() => {
        // 组件挂载和 userId 变化时执行
        async function fetchUser() {
            setLoading(true);
            try {
                const userData = await api.getUser(userId);
                setUser(userData);
            } finally {
                setLoading(false);
            }
        }

        fetchUser();

        // 清理函数（可选）
        return () => {
            // 取消请求或清理订阅
        };
    }, [userId]); // 依赖数组

    if (loading) return <div>加载中...</div>;
    return <div>用户: {user?.name}</div>;
}
```

## useCallback
用于缓存函数引用，避免在每次渲染时创建新的函数实例。

```jsx
import React, { useState, useCallback } from 'react';

function Parent() {
    const [count, setCount] = useState(0);
    const [name, setName] = useState('');

    // ❌ 每次渲染都创建新函数
    const handleClick = () => {
        setCount(count + 1);
    };

    // ✅ 使用 useCallback 缓存函数
    const handleClickOptimized = useCallback(() => {
        setCount(prev => prev + 1);
    }, []); // 空依赖数组，函数永远不变

    const handleNameChange = useCallback((newName) => {
        setName(newName);
    }, []); // 使用函数式更新，不依赖外部变量

    return (
        <div>
            <p>Count: {count}</p>
            <p>Name: {name}</p>
            <Child onClick={handleClickOptimized} onNameChange={handleNameChange} />
        </div>
    );
}

const Child = React.memo(({ onClick, onNameChange }) => {
    console.log('Child 重新渲染'); // 使用 useCallback 后不会频繁打印

    return (
        <div>
            <button onClick={onClick}>增加计数</button>
            <button onClick={() => onNameChange('新名字')}>改变名字</button>
        </div>
    );
});
```

## useContext
处理上下文管理。
```jsx
const ThemeContext = React.createContext();

function App() {
    return (
        <ThemeContext.Provider value="dark">
            <Header />
        </ThemeContext.Provider>
    );
}

function Header() {
    const theme = useContext(ThemeContext);
    return <div className={`header-${theme}`}>标题</div>;
}
```

## useMemo

用于缓存计算结果，避免在每次渲染时重复执行昂贵的计算。

```jsx
import React, { useState, useMemo } from 'react';

function ExpensiveComponent({ items, filter }) {
    const [count, setCount] = useState(0);

    // ❌ 每次渲染都重新计算
    const expensiveValue = items
        .filter(item => item.category === filter)
        .reduce((sum, item) => sum + item.price, 0);

    // ✅ 使用 useMemo 缓存计算结果
    const memoizedExpensiveValue = useMemo(() => {
        console.log('执行昂贵计算'); // 只有依赖变化时才会打印
        return items
            .filter(item => item.category === filter)
            .reduce((sum, item) => sum + item.price, 0);
    }, [items, filter]); // 只有 items 或 filter 变化时才重新计算

    return (
        <div>
            <p>总价: {memoizedExpensiveValue}</p>
            <p>计数: {count}</p>
            <button onClick={() => setCount(count + 1)}>
                增加计数 {/* 这个操作不会触发昂贵计算 */}
            </button>
        </div>
    );
}
```

# 原理
React Hooks 的核心原理基于链表结构和闭包。

## Fiber 节点与 Hook 链表

```jsx
// 简化的 Fiber 节点结构
function FiberNode() {
    this.memoizedState = null; // 指向第一个 Hook
    this.updateQueue = null;
    // ... 其他属性
}
// Hook 对象结构
function Hook() {
    this.memoizedState = null; // 当前状态值
    this.baseState = null;     // 基础状态
    this.baseQueue = null;     // 基础更新队列
    this.queue = null;         // 更新队列
    this.next = null;          // 指向下一个 Hook
}
```

## useState 的实现原理

```jsx
// 简化的 useState 实现
let currentFiber = null;
let currentHookIndex = 0;

function useState(initialState) {
    // 获取当前 Hook 或创建新的
    const hook = getCurrentHook();

    if (hook.memoizedState === null) {
        // 首次渲染，初始化状态
        hook.memoizedState = typeof initialState === 'function'
            ? initialState()
            : initialState;
    }

    const setState = (newState) => {
        // 创建更新对象
        const update = {
            action: newState,
            next: null
        };

        // 将更新加入队列
        if (hook.queue === null) {
            hook.queue = update;
            update.next = update; // 环形链表
        } else {
            update.next = hook.queue.next;
            hook.queue.next = update;
        }

        // 触发重新渲染
        scheduleWork(currentFiber);
    };

    return [hook.memoizedState, setState];
}

function getCurrentHook() {
    const fiber = currentFiber;
    const index = currentHookIndex++;

    if (fiber.memoizedState === null) {
        // 首次渲染，创建 Hook 链表
        const hook = new Hook();
        fiber.memoizedState = hook;
        return hook;
    } else {
        // 后续渲染，遍历 Hook 链表
        let hook = fiber.memoizedState;
        for (let i = 0; i < index; i++) {
            hook = hook.next;
        }
        return hook;
    }
}
```

## useEffect 的实现原理

```jsx
// 简化的 useEffect 实现
function useEffect(callback, deps) {
    const hook = getCurrentHook();

    const hasChanged = deps === undefined ||
        (hook.memoizedState === null ||
         !areDepEqual(deps, hook.memoizedState.deps));

    if (hasChanged) {
        // 依赖发生变化，需要执行副作用
        const effect = {
            callback,
            deps,
            cleanup: null
        };

        // 将副作用加入队列，在提交阶段执行
        currentFiber.effectList = currentFiber.effectList || [];
        currentFiber.effectList.push(effect);

        hook.memoizedState = effect;
    }
}

function areDepEqual(prevDeps, nextDeps) {
    if (prevDeps.length !== nextDeps.length) return false;

    for (let i = 0; i < prevDeps.length; i++) {
        if (Object.is(prevDeps[i], nextDeps[i]) === false) {
            return false;
        }
    }
    return true;
}
```

## useCallback实现原理

```jsx
// React 内部的简化实现
function useCallback(callback, deps) {
    const hook = getCurrentHook();

    // 首次渲染
    if (hook.memoizedState === null) {
        hook.memoizedState = {
            callback,
            deps
        };
        return callback;
    }

    const prevDeps = hook.memoizedState.deps;
    const prevCallback = hook.memoizedState.callback;

    // 检查依赖是否发生变化
    if (areHookInputsEqual(deps, prevDeps)) {
        // 依赖未变化，返回缓存的函数
        return prevCallback;
    }

    // 依赖发生变化，更新缓存
    hook.memoizedState = {
        callback,
        deps
    };

    return callback;
}

function areHookInputsEqual(nextDeps, prevDeps) {
    if (prevDeps === null) return false;
    if (nextDeps.length !== prevDeps.length) return false;

    for (let i = 0; i < prevDeps.length; i++) {
        if (Object.is(prevDeps[i], nextDeps[i]) === false) {
            return false;
        }
    }
    return true;
}
```

## useMemo实现原理

```jsx
// React 内部的简化实现
function useMemo(create, deps) {
    const hook = getCurrentHook();

    // 首次渲染
    if (hook.memoizedState === null) {
        const value = create(); // 执行计算函数
        hook.memoizedState = {
            value,
            deps
        };
        return value;
    }

    const prevDeps = hook.memoizedState.deps;
    const prevValue = hook.memoizedState.value;

    // 检查依赖是否发生变化
    if (areHookInputsEqual(deps, prevDeps)) {
        // 依赖未变化，返回缓存的值
        return prevValue;
    }

    // 依赖发生变化，重新计算
    const value = create();
    hook.memoizedState = {
        value,
        deps
    };

    return value;
}

function areHookInputsEqual(nextDeps, prevDeps) {
    if (prevDeps === null) return false;
    if (nextDeps.length !== prevDeps.length) return false;

    // 使用 Object.is 进行浅比较
    for (let i = 0; i < prevDeps.length; i++) {
        if (Object.is(prevDeps[i], nextDeps[i]) === false) {
            return false;
        }
    }
    return true;
}
```


# 为什么Hooks有使用限制

Hooks 的规则限制主要是为了保证Hook 链表的稳定性。

## 规则

- 只能在函数组件顶层调用
- 不能在循环、条件语句或嵌套函数中调用

# 自定义Hook？

自定义 Hook 就是以 use 开头的函数，内部使用其他 Hooks。

```jsx
function useLocalStorage(key, initialValue) {
    // 从 localStorage 读取初始值
    const [storedValue, setStoredValue] = useState(() => {
        try {
            const item = window.localStorage.getItem(key);
            return item ? JSON.parse(item) : initialValue;
        } catch (error) {
            console.error('读取 localStorage 失败:', error);
            return initialValue;
        }
    });

    // 封装的 setter 函数
    const setValue = useCallback((value) => {
        try {
            // 支持函数式更新
            const valueToStore = value instanceof Function
                ? value(storedValue)
                : value;

            setStoredValue(valueToStore);
            window.localStorage.setItem(key, JSON.stringify(valueToStore));
        } catch (error) {
            console.error('写入 localStorage 失败:', error);
        }
    }, [key, storedValue]);

    return [storedValue, setValue];
}

// 使用示例
function Settings() {
    const [theme, setTheme] = useLocalStorage('theme', 'light');
    const [language, setLanguage] = useLocalStorage('language', 'zh');

    return (
        <div>
            <select value={theme} onChange={e => setTheme(e.target.value)}>
                <option value="light">浅色</option>
                <option value="dark">深色</option>
            </select>

            <select value={language} onChange={e => setLanguage(e.target.value)}>
                <option value="zh">中文</option>
                <option value="en">English</option>
            </select>
        </div>
    );
}
```

## useAsync Hook

```jsx
function useAsync(asyncFunction, dependencies = []) {
    const [state, setState] = useState({
        data: null,
        loading: true,
        error: null
    });

    const execute = useCallback(async () => {
        setState(prev => ({ ...prev, loading: true, error: null }));

        try {
            const data = await asyncFunction();
            setState({ data, loading: false, error: null });
        } catch (error) {
            setState({ data: null, loading: false, error });
        }
    }, dependencies);

    useEffect(() => {
        execute();
    }, [execute]);

    return { ...state, retry: execute };
}

// 使用示例
function UserList() {
    const { data: users, loading, error, retry } = useAsync(
        () => fetch('/api/users').then(res => res.json()),
        []
    );

    if (loading) return <div>加载中...</div>;
    if (error) return (
        <div>
            错误: {error.message}
            <button onClick={retry}>重试</button>
        </div>
    );

    return (
        <ul>
            {users?.map(user => (
                <li key={user.id}>{user.name}</li>
            ))}
        </ul>
    );
}
```

# Class和Hook？

## 代码复用更简单

```jsx
// Class 组件需要 HOC 或 Render Props
class UserProfile extends Component {
    // 复杂的生命周期逻辑
    componentDidMount() { /* 获取用户数据 */ }
    componentDidUpdate(prevProps) { /* 处理 props 变化 */ }
    componentWillUnmount() { /* 清理 */ }
}

// Hooks 组件更简洁
function UserProfile({ userId }) {
    const user = useUser(userId); // 自定义 Hook 封装所有逻辑
    return <div>{user?.name}</div>;
}
```

## 逻辑关注点分离

```jsx
// Class 组件：相关逻辑分散在不同生命周期
class ChatRoom extends Component {
    componentDidMount() {
        this.subscribeToChat();
        this.updateTitle();
    }

    componentDidUpdate() {
        this.updateTitle();
    }

    componentWillUnmount() {
        this.unsubscribeFromChat();
    }
}

// Hooks 组件：相关逻辑聚合在一起
function ChatRoom({ roomId }) {
    // 聊天订阅逻辑
    useEffect(() => {
        const unsubscribe = subscribeToChat(roomId);
        return unsubscribe;
    }, [roomId]);

    // 标题更新逻辑
    useEffect(() => {
        document.title = `聊天室 ${roomId}`;
    }, [roomId]);
}
```

## TypeScript支持更好

```jsx
// Hooks 的类型推导更自然
function useCounter(initialValue: number = 0) {
    const [count, setCount] = useState(initialValue);
    // TypeScript 自动推导类型

    const increment = useCallback(() => {
        setCount(prev => prev + 1);
    }, []);

    return { count, increment };
}
```

## 更小的包体积

- 没有 Class 的继承开销
- 更好的 Tree Shaking
- 编译后代码更简洁
