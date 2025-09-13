
<!--toc:start-->
- [四大组件](#四大组件)
  - [Activity](#activity)
    - [启动模式](#启动模式)
    - [Intent Flags](#intent-flags)
  - [Service](#service)
    - [Service 类型](#service-类型)
    - [启动方式](#启动方式)
    - [IntentService](#intentservice)
  - [BroadcastReceiver](#broadcastreceiver)
    - [注册方式](#注册方式)
    - [广播类型](#广播类型)
    - [广播接收器的实现](#广播接收器的实现)
  - [ContentProvider](#contentprovider)
    - [基本实现](#基本实现)
    - [访问ContentProvider](#访问contentprovider)
    - [ContentProvider 特点](#contentprovider-特点)
    - [UriMatcher 使用](#urimatcher-使用)
  - [总结](#总结)
- [Handler机制](#handler机制)
  - [核心组件](#核心组件)
  - [Looper、MessageQueue、Handler 之间的关系](#loopermessagequeuehandler-之间的关系)
    - [工作流程](#工作流程)
    - [源码](#源码)
  - [主线程与子线程通信](#主线程与子线程通信)
    - [从子线程更新 UI](#从子线程更新-ui)
    - [从主线程发送消息到子线程](#从主线程发送消息到子线程)
    - [HandlerThread的使用](#handlerthread的使用)
  - [Handler内存泄露内存泄漏原因](#handler内存泄露内存泄漏原因)
  - [Handler 机制的实际应用](#handler-机制的实际应用)
- [事件分发机制](#事件分发机制)
  - [事件分发的三个核心方法](#事件分发的三个核心方法)
  - [事件传递顺序](#事件传递顺序)
    - [Activity事件分发](#activity事件分发)
    - [Window事件分发](#window事件分发)
    - [ViewGroup 事件分发](#viewgroup-事件分发)
    - [View 事件分发](#view-事件分发)
  - [关键方法讲解](#关键方法讲解)
  - [总结](#总结)
- [重要组件](#重要组件)
  - [RecyclerView](#recyclerview)
    - [核心组件](#核心组件)
    - [工作原理](#工作原理)
      - [视图回收与复用机制](#视图回收与复用机制)
      - [布局过程](#布局过程)
      - [滚动机制](#滚动机制)
    - [预加载机制](#预加载机制)
    - [总结](#总结)
- [系统服务](#系统服务)
  - [系统进程级核心服务（System Services）](#系统进程级核心服务system-services)
  - [HAL / Native Daemon 层](#hal-native-daemon-层)
  - [运行时支持服务](#运行时支持服务)
  - [应用可见的系统服务接](#应用可见的系统服务接)
<!--toc:end-->

#  四大组件

Android 的四大组件（Activity、Service、BroadcastReceiver、ContentProvider）是构建 Android 应用的基础。每个组件都有特定的生命周期和用途。

## Activity

Activity 是用户交互的入口点，代表应用中的一个屏幕。

完整生命周期：
- onCreate(): Activity 创建时调用，用于初始化，如设置布局
- onStart(): Activity 变为可见时调用
- onResume(): Activity 开始与用户交互时调用
- onPause(): 当前 Activity 被部分遮挡时调用，常用于保存数据
- onStop(): Activity 完全不可见时调用
- onDestroy(): Activity 被销毁前调用
- onRestart(): Activity 从停止状态重新启动时调用

生命周期配对：

- onCreate() 与 onDestroy() 配对：创建和销毁
- onStart() 与 onStop() 配对：可见性
- onResume() 与 onPause() 配对：前台状态

### 启动模式

1. 标准模式

默认模式，每次启动都创建新实例
可能导致同一 Activity 多个实例

```java
Intent intent = new Intent(this, SecondActivity.class);
startActivity(intent);
```

2. singleTop（栈顶复用模式）

如果目标 Activity 已在栈顶，则复用，调用 onNewIntent()；否则创建新实例。

```xml
<activity android:name=".SecondActivity"
          android:launchMode="singleTop"/>
```

3. singleTask（栈内复用模式）

在当前任务栈中查找，存在则将其上方所有 Activity 出栈并复用；不存在则创建新实例。

4. singleInstance（单例模式）

独占一个任务栈，全局唯一；适用于特殊界面，如来电界面。

### Intent Flags

除了在 AndroidManifest 中设置启动模式，还可以通过 Intent flags 动态控制：

```java
Intent intent = new Intent(this, SecondActivity.class);
intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
startActivity(intent);
```

常用 flags：

- FLAG_ACTIVITY_NEW_TASK
- FLAG_ACTIVITY_CLEAR_TOP
- FLAG_ACTIVITY_SINGLE_TOP
- FLAG_ACTIVITY_CLEAR_TASK

## Service

Service 是一个可以在后台执行长时间运行操作的应用组件，不提供用户界面。

### Service 类型

1. 按运行地点分类
- 本地服务（Local Service）：运行在应用进程内
- 远程服务（Remote Service）：运行在独立进程，通过 AIDL 通信

2. 按运行方式分类
- 前台服务：有通知栏显示，不易被系统回收
- 后台服务：无通知栏显示，内存不足时可能被回收

### 启动方式

1. startService 方式

```java
Intent intent = new Intent(this, MyService.class);
startService(intent);
// 停止服务
stopService(intent);
```

特点：
- Service 与调用者无关联，调用者退出后 Service 仍运行
- Service 需自行调用 stopSelf() 或外部调用 stopService() 停止
- 生命周期：onCreate() → onStartCommand() → onDestroy()

2. bindService 方式

```java
Intent intent = new Intent(this, MyService.class);
bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE);
// 解绑服务
unbindService(serviceConnection);
```

特点：
- Service 与调用者绑定，调用者退出则 Service 销毁
- 可获取 Service 实例，实现交互
- 生命周期：onCreate() → onBind() → onUnbind() → onDestroy()

### IntentService

IntentService 是 Service 的子类，用于处理异步请求：

```java
public class MyIntentService extends IntentService {
    public MyIntentService() {
        super("MyIntentService");
    }

    @Override
    protected void onHandleIntent(Intent intent) {
        // 在工作线程执行，完成后自动停止
    }
}
```

特点：
- 内部创建工作线程处理请求
- 请求处理完毕自动停止
- 请求队列化处理

## BroadcastReceiver

BroadcastReceiver 是用于接收广播消息并做出响应的组件。

### 注册方式

1. 静态注册

在 AndroidManifest.xml 中注册：

```xml
<receiver android:name=".MyReceiver">
    <intent-filter>
        <action android:name="android.intent.action.BOOT_COMPLETED"/>
    </intent-filter>
</receiver>
```

特点：
- 应用未启动也能接收广播
- 适合接收系统广播
- Android 8.0 后，大多数隐式广播无法通过静态注册接收

2. 动态注册

在代码中注册：

```java
IntentFilter filter = new IntentFilter();
filter.addAction("com.example.MY_ACTION");
registerReceiver(myReceiver, filter);
// 注意在适当时机解注册
unregisterReceiver(myReceiver);
```

特点：
- 跟随注册的组件生命周期
- 需手动解注册避免内存泄漏
- 可以接收所有类型广播

### 广播类型

1. 普通广播

```java
Intent intent = new Intent("com.example.MY_ACTION");
sendBroadcast(intent);
```

特点：完全异步，所有接收者同时接收

2. 异步广播

```java
Intent intent = new Intent("com.example.MY_ACTION");
sendOrderedBroadcast(intent, null);
```

特点：

- 按优先级顺序接收
- 可以中断广播传递
- 可以修改广播内容

3. 本地广播

```java
LocalBroadcastManager.getInstance(this).sendBroadcast(intent);
```

特点：
- 只在应用内传播
- 更高效安全
- Android X 中已被弃用，推荐使用 LiveData 等替代

### 广播接收器的实现

```java
public class MyReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        // 处理接收到的广播
        // 注意：不要执行耗时操作
        if ("com.example.MY_ACTION".equals(intent.getAction())) {
            // 处理逻辑
        }
    }
}
```

## ContentProvider

ContentProvider 用于管理数据访问，实现不同应用间数据共享。

### 基本实现

创建 ContentProvider：

```java
public class MyProvider extends ContentProvider {
    private static final String AUTHORITY = "com.example.provider";
    private static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/items");
    private MyDatabaseHelper dbHelper;

    @Override
    public boolean onCreate() {
        dbHelper = new MyDatabaseHelper(getContext());
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
                        String[] selectionArgs, String sortOrder) {
        SQLiteDatabase db = dbHelper.getReadableDatabase();
        return db.query("items", projection, selection, selectionArgs,
                       null, null, sortOrder);
    }

    // 实现其他必要方法: insert, update, delete, getType
}
```

同时需要在XML中注册：

```xml
<provider
    android:name=".MyProvider"
    android:authorities="com.example.provider"
    android:exported="true"
    android:permission="com.example.READ_WRITE_PERMISSION"/>
```

### 访问ContentProvider

```java
// 查询操作
Uri uri = Uri.parse("content://com.example.provider/items");
Cursor cursor = getContentResolver().query(uri, null, null, null, null);

// 插入操作
ContentValues values = new ContentValues();
values.put("name", "New Item");
Uri newUri = getContentResolver().insert(uri, values);

// 更新操作
int count = getContentResolver().update(uri, values, "id=?", new String[]{"1"});

// 删除操作
int deletedRows = getContentResolver().delete(uri, "id=?", new String[]{"1"});
```

### ContentProvider 特点

数据封装：隐藏数据源实现细节
标准接口：提供统一的 CRUD 操作
安全性：可设置权限控制访问
数据变化通知：通过 ContentObserver 监听数据变化

### UriMatcher 使用

```java
private static final UriMatcher sUriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
private static final int ITEMS = 1;
private static final int ITEM_ID = 2;

static {
    sUriMatcher.addURI("com.example.provider", "items", ITEMS);
    sUriMatcher.addURI("com.example.provider", "items/#", ITEM_ID);
}

@Override
public Cursor query(Uri uri, String[] projection, String selection,
                    String[] selectionArgs, String sortOrder) {
    SQLiteDatabase db = dbHelper.getReadableDatabase();
    Cursor cursor;

    switch (sUriMatcher.match(uri)) {
        case ITEMS:
            cursor = db.query("items", projection, selection, selectionArgs,
                             null, null, sortOrder);
            break;
        case ITEM_ID:
            String id = uri.getLastPathSegment();
            cursor = db.query("items", projection, "_id=?", new String[]{id},
                             null, null, sortOrder);
            break;
        default:
            throw new IllegalArgumentException("Unknown URI: " + uri);
    }

    cursor.setNotificationUri(getContext().getContentResolver(), uri);
    return cursor;
}
```

## 总结

1. 共同点：
- 均在 AndroidManifest.xml 中注册（动态注册的 BroadcastReceiver 除外）
- 均有自己的生命周期
- 均可通过 Intent 激活（ContentProvider 通过 ContentResolver）
- 均为 Android 应用的基础构建块

2. 区别：
- Activity: 提供可视化用户界面，处理用户交互
- Service: 执行后台任务，无用户界面
- BroadcastReceiver: 响应系统或应用事件，通常短暂执行
- ContentProvider: 管理共享数据，提供数据访问统一接口

# Handler机制

Handler 机制是 Android 中用于线程间通信的核心机制，特别是在处理 UI 更新时尤为重要。由于 Android UI 操作不是线程安全的，所有 UI 操作必须在主线程（UI 线程）中执行，而耗时操作则需要在工作线程中进行。Handler 机制提供了一种在不同线程间安全通信的方式。

## 核心组件

Handler 机制由四个核心组件组成：

- Message：需要传递的消息，可以包含数据
- MessageQueue：消息队列，存储待处理的消息
- Looper：消息循环器，不断从 MessageQueue 中取出消息并分发
- Handler：消息处理器，负责发送和处理消息

## Looper、MessageQueue、Handler 之间的关系

这三者之间的关系可以概括为：

1. 一个线程只有一个 Looper
2. 一个 Looper 只有一个 MessageQueue
3. 一个线程可以有多个 Handler
4. 多个 Handler 可以共享同一个 Looper 和 MessageQueue

### 工作流程

Handler 机制的工作流程如下：

1. 初始化阶段：
- Looper 通过 Looper.prepare() 在当前线程创建（主线程自动创建）
- Looper 创建 MessageQueue
- Handler 关联到当前线程的 Looper
2. 消息发送阶段：
- Handler 调用 sendMessage() 或 post() 方法
- Message 被加入到 MessageQueue 中
3. 消息循环阶段：
- Looper 通过 Looper.loop() 开启循环
- Looper 不断从 MessageQueue 中取出 Message
- 将 Message 分发给对应的 Handler 处理
4. 消息处理阶段：
- Handler 的 handleMessage() 方法被调用处理消息

### 源码

1. Looper创建

```java
public static void prepare() {
    prepare(true);
}

private static void prepare(boolean quitAllowed) {
    if (sThreadLocal.get() != null) {
        throw new RuntimeException("Only one Looper may be created per thread");
    }
    sThreadLocal.set(new Looper(quitAllowed));
}

private Looper(boolean quitAllowed) {
    mQueue = new MessageQueue(quitAllowed);  // 创建 MessageQueue
    mThread = Thread.currentThread();  // 记录当前线程
}
```

2. Looper 循环

```java
public static void loop() {
    final Looper me = myLooper();  // 获取当前线程的 Looper
    if (me == null) {
        throw new RuntimeException("No Looper; Looper.prepare() wasn't called on this thread.");
    }
    final MessageQueue queue = me.mQueue;  // 获取 MessageQueue

    for (;;) {  // 无限循环
        Message msg = queue.next();  // 获取下一条消息
        if (msg == null) {  // 没有消息，退出循环
            return;
        }

        try {
            msg.target.dispatchMessage(msg);  // 分发消息给对应的 Handler
        } finally {
            // ...
        }

        msg.recycleUnchecked();  // 回收消息
    }
}
```

3. Handler 发送消息

```java
public final boolean sendMessage(Message msg) {
    return sendMessageDelayed(msg, 0);
}

public final boolean sendMessageDelayed(Message msg, long delayMillis) {
    if (delayMillis < 0) {
        delayMillis = 0;
    }
    return sendMessageAtTime(msg, SystemClock.uptimeMillis() + delayMillis);
}

public boolean sendMessageAtTime(Message msg, long uptimeMillis) {
    MessageQueue queue = mQueue;
    if (queue == null) {
        return false;
    }
    return enqueueMessage(queue, msg, uptimeMillis);
}

private boolean enqueueMessage(MessageQueue queue, Message msg, long uptimeMillis) {
    msg.target = this;  // 设置消息的目标 Handler
    return queue.enqueueMessage(msg, uptimeMillis);  // 将消息加入队列
}
```

3. Handler 处理消息

```java
public void dispatchMessage(Message msg) {
    if (msg.callback != null) {
        handleCallback(msg);  // 处理 post(Runnable) 方式发送的消息
    } else {
        if (mCallback != null) {
            if (mCallback.handleMessage(msg)) {
                return;
            }
        }
        handleMessage(msg);  // 调用子类重写的 handleMessage 方法
    }
}
```

## 主线程与子线程通信

### 从子线程更新 UI

```java
// 创建与主线程关联的 Handler
private Handler mainHandler = new Handler(Looper.getMainLooper()) {
    @Override
    public void handleMessage(Message msg) {
        // 在主线程中处理 UI 更新
        switch (msg.what) {
            case MSG_UPDATE_UI:
                textView.setText((String) msg.obj);
                break;
        }
    }
};

// 在子线程中执行耗时操作
new Thread(new Runnable() {
    @Override
    public void run() {
        // 执行耗时操作
        String result = performLongOperation();

        // 通过 Handler 发送消息到主线程
        Message message = Message.obtain();
        message.what = MSG_UPDATE_UI;
        message.obj = result;
        mainHandler.sendMessage(message);

        // 或者使用 post 方式
        mainHandler.post(new Runnable() {
            @Override
            public void run() {
                textView.setText(result);
            }
        });
    }
}).start();
```

### 从主线程发送消息到子线程

需要在子线程中创建 Looper:

```java
class WorkerThread extends Thread {
    private Handler workerHandler;

    @Override
    public void run() {
        // 为当前线程创建 Looper
        Looper.prepare();

        // 创建 Handler
        workerHandler = new Handler() {
            @Override
            public void handleMessage(Message msg) {
                // 在工作线程中处理消息
                processMessage(msg);
            }
        };

        // 通知主线程，Handler 已准备好
        synchronized (this) {
            notifyAll();
        }

        // 开始消息循环
        Looper.loop();
    }

    public Handler getHandler() {
        return workerHandler;
    }
}

// 使用方式
WorkerThread workerThread = new WorkerThread();
workerThread.start();

// 等待 Handler 创建完成
synchronized (workerThread) {
    try {
        workerThread.wait();
    } catch (InterruptedException e) {
        e.printStackTrace();
    }
}

// 从主线程发送消息到工作线程
Handler workerHandler = workerThread.getHandler();
Message msg = Message.obtain();
msg.what = MSG_DO_WORK;
msg.obj = data;
workerHandler.sendMessage(msg);
```

### HandlerThread的使用

Android 提供了 HandlerThread 类，简化了带 Looper 的线程创建：

```java
// 创建 HandlerThread
HandlerThread handlerThread = new HandlerThread("WorkerThread");
handlerThread.start();

// 创建关联到 HandlerThread 的 Handler
Handler workerHandler = new Handler(handlerThread.getLooper()) {
    @Override
    public void handleMessage(Message msg) {
        // 在 HandlerThread 中处理消息
    }
};

// 发送消息
workerHandler.sendMessage(msg);

// 使用完毕后记得退出
handlerThread.quitSafely();
```

## Handler内存泄露内存泄漏原因

Handler 内存泄漏主要发生在以下情况：

- 非静态内部类 Handler：非静态内部类隐式持有外部类引用
- 延迟消息：当 Activity 销毁时，延迟消息仍在队列中等待处理
- 消息持有大对象：消息中包含大对象引用，导致无法及时释放

## Handler 机制的实际应用

1. 延迟执行

```java
handler.postDelayed(new Runnable() {
    @Override
    public void run() {
        // 延迟执行的代码
    }
}, 1000);  // 1000ms 后执行
```

2. 定时任务

```java
private final Runnable timerRunnable = new Runnable() {
    @Override
    public void run() {
        // 执行定时任务
        updateTimer();
        // 再次发送延迟消息，形成循环
        handler.postDelayed(this, 1000);
    }
};

// 开始定时任务
handler.post(timerRunnable);

// 停止定时任务
handler.removeCallbacks(timerRunnable);
```

3. 异步加载数据

```java
private void loadDataAsync() {
    new Thread(new Runnable() {
        @Override
        public void run() {
            // 显示加载中
            handler.post(() -> showLoading());

            // 执行耗时操作
            final Data data = loadDataFromNetwork();

            // 更新 UI
            handler.post(() -> {
                hideLoading();
                showData(data);
            });
        }
    }).start();
}
```

4. 消息优先级处理

```java
// 发送高优先级消息
Message highPriorityMsg = Message.obtain();
highPriorityMsg.what = HIGH_PRIORITY_MSG;
handler.sendMessageAtFrontOfQueue(highPriorityMsg);
```

# 事件分发机制

Android 事件分发机制是处理用户交互的核心机制，它决定了触摸事件如何在视图层次结构中传递和处理。

Android 中的触摸事件由 MotionEvent 对象表示，主要包括以下类型：
- ACTION_DOWN：手指按下屏幕
- ACTION_MOVE：手指在屏幕上移动
- ACTION_UP：手指离开屏幕
- ACTION_CANCEL：事件取消（如被父视图拦截）
- ACTION_POINTER_DOWN/UP：多点触控时其他手指按下/抬起

## 事件分发的三个核心方法

事件分发机制主要涉及三个关键方法：

1. dispatchTouchEvent(MotionEvent ev)：分发事件
2. onInterceptTouchEvent(MotionEvent ev)：拦截事件（仅 ViewGroup 有此方法）
3. onTouchEvent(MotionEvent ev)：处理事件

## 事件传递顺序

Activity → Window → DecorView → ViewGroup → ... → View

具体流程如下：

1. 用户触摸屏幕，系统将触摸事件传递给当前 Activity
2. Activity 的 dispatchTouchEvent() 将事件传递给 Window
3. Window 将事件传递给 DecorView（根视图）
4. 事件从 DecorView 开始，自上而下传递给视图层次中的 ViewGroup 和 View

### Activity事件分发

```java
public boolean dispatchTouchEvent(MotionEvent ev) {
    if (ev.getAction() == MotionEvent.ACTION_DOWN) {
        onUserInteraction();
    }
    if (getWindow().superDispatchTouchEvent(ev)) {
        return true;  // 事件被 Window 或其子视图消费
    }
    return onTouchEvent(ev);  // 调用 Activity 的 onTouchEvent
}
```

### Window事件分发

Window 的实现类 PhoneWindow 将事件传递给 DecorView：

```java
public boolean superDispatchTouchEvent(MotionEvent event) {
    return mDecor.superDispatchTouchEvent(event);
}
```

### ViewGroup 事件分发

ViewGroup 是事件分发机制中最复杂的部分，因为它既要处理自己的触摸事件，又要协调子视图的事件分发：

```java
// 简化版 ViewGroup 的 dispatchTouchEvent 逻辑
public boolean dispatchTouchEvent(MotionEvent ev) {
    boolean handled = false;

    // 1. 是否拦截
    final boolean intercepted;
    if (ev.getAction() == MotionEvent.ACTION_DOWN || mFirstTouchTarget != null) {
        final boolean disallowIntercept = (mGroupFlags & FLAG_DISALLOW_INTERCEPT) != 0;
        if (!disallowIntercept) {
            intercepted = onInterceptTouchEvent(ev);
        } else {
            intercepted = false;
        }
    } else {
        intercepted = true;
    }

    // 2. 如果不拦截，则分发给子 View
    if (!intercepted) {
        for (int i = childrenCount - 1; i >= 0; i--) {
            final View child = getChildAt(i);
            if (child.canReceivePointerEvents() && isTransformedTouchPointInView()) {
                if (dispatchTransformedTouchEvent(ev, child)) {
                    // 子 View 消费了事件
                    handled = true;
                    break;
                }
            }
        }
    }

    // 3. 如果没有子 View 处理或者拦截了事件，则自己处理
    if (!handled) {
        handled = dispatchTransformedTouchEvent(ev, null);  // 调用自身的 onTouchEvent
    }

    return handled;
}
```

### View 事件分发

View 没有子视图，因此没有 onInterceptTouchEvent() 方法，其事件分发相对简单：

```java
public boolean dispatchTouchEvent(MotionEvent event) {
    boolean result = false;

    // 1. 如果有 OnTouchListener 且 View 可用，则先调用 OnTouchListener
    if (mOnTouchListener != null && (mViewFlags & ENABLED_MASK) == ENABLED
            && mOnTouchListener.onTouch(this, event)) {
        result = true;
    }

    // 2. 如果 OnTouchListener 没有消费事件，则调用 onTouchEvent
    if (!result && onTouchEvent(event)) {
        result = true;
    }

    return result;
}
```

## 关键方法讲解

1. dispatchTouchEvent()
作用：分发事件，是事件分发的入口
返回值：
true：事件被消费，不再向上传递
false：事件未被消费，向上传递给父视图的 onTouchEvent()
2. onInterceptTouchEvent()
作用：判断是否拦截事件（只存在于 ViewGroup 中）
返回值：
true：拦截事件，事件交给自己的 onTouchEvent() 处理
false：不拦截，事件继续传递给子视图
3. onTouchEvent()
作用：处理触摸事件
返回值：
true：事件已被消费
false：事件未被消费，向上传递给父视图的 onTouchEvent()


## 总结

1. 事件分发核心要点
- 事件传递顺序：Activity → Window → ViewGroup → View
- 核心方法：dispatchTouchEvent → onInterceptTouchEvent → onTouchEvent
- 事件序列：一个完整的事件序列从 DOWN 开始，到 UP 结束
- 责任链模式：事件分发机制采用责任链模式，事件沿着视图层次结构传递
2. 滑动冲突解决方案
- 外部拦截法：父容器决定是否拦截事件
- 内部拦截法：子 View 决定是否让父容器拦截事件
- 嵌套滑动机制：使用 NestedScrollingChild 和 NestedScrollingParent 接口
- 现代化控件：使用 CoordinatorLayout、NestedScrollView 等控件


# 重要组件

## RecyclerView

RecyclerView 是 Android 中用于展示大量数据集的高级视图组件，它在 Android 5.0 (Lollipop) 中被引入，作为 ListView 和 GridView 的更灵活、高效的替代品。RecyclerView 的核心理念是通过复用视图来提高性能，尤其是在处理大量数据时。

1. RecyclerView 的主要优势
- 视图复用机制：高效的视图回收与复用系统
- 布局灵活性：支持线性、网格、瀑布流等多种布局方式
- 动画支持：内置丰富的项目动画效果
- 装饰器：可添加分割线等装饰
- 可定制性：几乎所有组件都可自定义

### 核心组件

1. Adapter

负责创建 ViewHolder 并将数据绑定到视图上：

```java
public abstract class RecyclerView.Adapter<VH extends RecyclerView.ViewHolder> {
    // 创建新的 ViewHolder
    public abstract VH onCreateViewHolder(ViewGroup parent, int viewType);

    // 将数据绑定到 ViewHolder
    public abstract void onBindViewHolder(VH holder, int position);

    // 返回数据集大小
    public abstract int getItemCount();

    // 获取项目类型（用于不同类型的视图）
    public int getItemViewType(int position) {
        return 0;
    }
}
```

2. LayoutManager（布局管理器）:

负责测量和定位项目视图，以及确定何时复用不再可见的项目视图：

```java
public abstract class RecyclerView.LayoutManager {
    // 测量和布局子视图
    public void onLayoutChildren(RecyclerView.Recycler recycler, RecyclerView.State state) { ... }

    // 计算滚动量
    public int scrollVerticallyBy(int dy, RecyclerView.Recycler recycler, RecyclerView.State state) { ... }
    public int scrollHorizontallyBy(int dx, RecyclerView.Recycler recycler, RecyclerView.State state) { ... }

    // 查找视图位置
    public View findViewByPosition(int position) { ... }
}
```

常用的 LayoutManager 实现：

- LinearLayoutManager：线性布局（垂直或水平）
- GridLayoutManager：网格布局
- StaggeredGridLayoutManager：瀑布流布局

3. ViewHolder（视图持有者）

持有视图引用，减少 findViewById 调用：

```java
public abstract static class RecyclerView.ViewHolder {
    public final View itemView;
    int mPosition;
    int mItemViewType;

    public ViewHolder(View itemView) {
        this.itemView = itemView;
    }

    public final int getPosition() { ... }
    public final int getItemViewType() { ... }
}
```

4. Recycler（回收器）

管理废弃视图的缓存和复用：

```java
public final class RecyclerView.Recycler {
    // 获取指定位置的视图
    public View getViewForPosition(int position) { ... }

    // 回收视图
    public void recycleView(View view) { ... }

    // 清除缓存
    public void clear() { ... }
}
```

5. ItemAnimator（项目动画器）

处理项目添加、移除、移动和更改时的动画效果：

```java
public abstract class RecyclerView.ItemAnimator {
    // 动画持续时间
    public abstract long getAddDuration();
    public abstract long getRemoveDuration();
    public abstract long getMoveDuration();
    public abstract long getChangeDuration();

    // 执行动画
    public abstract boolean animateAdd(RecyclerView.ViewHolder holder);
    public abstract boolean animateRemove(RecyclerView.ViewHolder holder);
    public abstract boolean animateMove(RecyclerView.ViewHolder holder, int fromX, int fromY, int toX, int toY);
    public abstract boolean animateChange(RecyclerView.ViewHolder oldHolder, RecyclerView.ViewHolder newHolder, int fromX, int fromY, int toX, int toY);
}
```

### 工作原理

#### 视图回收与复用机制
RecyclerView 的核心是其高效的视图回收与复用机制，主要通过以下几个缓存池实现：

1. 四级缓存结构
- Scrap Heap（第一级缓存）
临时存放正在屏幕上显示的视图
当布局改变时（如滚动），这些视图会被临时移除，然后快速复用
不需要重新绑定数据，直接复用
- Cache（第二级缓存）
存放刚刚移出屏幕的视图（默认大小为 2）
按照位置索引存储，可以快速复用
需要重新绑定数据
- ViewCacheExtension（第三级缓存）
开发者自定义的缓存逻辑
在 Cache 未命中时尝试
- RecycledViewPool（第四级缓存）
存放所有类型的废弃视图（默认每种类型最多 5 个）
可以在多个 RecyclerView 之间共享
需要完全重新绑定数据
2. 缓存查找流程

当 RecyclerView 需要显示一个位置的视图时，查找顺序如下：
- 首先检查 Scrap Heap
- 如果未找到，检查 Cache
- 如果仍未找到，检查 ViewCacheExtension
- 如果仍未找到，检查 RecycledViewPool
- 如果所有缓存都未命中，创建新的 ViewHolder

简化逻辑
```java
// Recycler 中获取视图的简化逻辑
public View getViewForPosition(int position) {
    // 1. 检查 Scrap Heap
    ViewHolder holder = getScrapViewForPosition(position);
    if (holder != null) return holder.itemView;

    // 2. 检查 Cache
    holder = getFromCache(position);
    if (holder != null) {
        // 需要重新绑定数据
        mAdapter.bindViewHolder(holder, position);
        return holder.itemView;
    }

    // 3. 检查自定义缓存扩展
    View view = mViewCacheExtension.getViewForPositionAndType(position, type);
    if (view != null) {
        holder = getChildViewHolder(view);
        // 处理视图...
        return holder.itemView;
    }

    // 4. 检查 RecycledViewPool
    holder = getRecycledViewPool().getRecycledView(type);
    if (holder != null) {
        // 需要完全重新绑定
        holder.resetInternal();
        mAdapter.bindViewHolder(holder, position);
        return holder.itemView;
    }

    // 5. 创建新的 ViewHolder
    holder = mAdapter.createViewHolder(RecyclerView.this, type);
    mAdapter.bindViewHolder(holder, position);
    return holder.itemView;
}
```

#### 布局过程

RecyclerView 的布局过程主要由 LayoutManager 负责：

1. 测量阶段：
RecyclerView 的 onMeasure 方法调用 LayoutManager 的 onMeasure
LayoutManager 确定 RecyclerView 的尺寸
2. 布局阶段：
RecyclerView 的 onLayout 方法调用 LayoutManager 的 onLayoutChildren
LayoutManager 向 Recycler 请求视图并布局
超出屏幕的视图被回收到缓存中
3. 绘制阶段：
RecyclerView 的 draw 方法绘制所有子视图
装饰器（ItemDecoration）在此阶段绘制

布局简化逻辑
```java
// RecyclerView 布局过程简化代码
@Override
protected void onLayout(boolean changed, int l, int t, int r, int b) {
    dispatchLayout();
}

void dispatchLayout() {
    mState.mIsMeasuring = false;
    if (mState.mLayoutStep == State.STEP_START) {
        dispatchLayoutStep1();
        mState.mLayoutStep = State.STEP_LAYOUT;
    }
    dispatchLayoutStep2();
    mState.mLayoutStep = State.STEP_ANIMATIONS;
    dispatchLayoutStep3();
}

// 第一步：保存视图信息，准备动画
private void dispatchLayoutStep1() {
    mState.mItemCount = mAdapter.getItemCount();
    // 保存视图状态，为动画做准备
    mItemAnimator.recordPreLayoutInformation(...);
}

// 第二步：实际布局过程
private void dispatchLayoutStep2() {
    mLayoutManager.onLayoutChildren(mRecycler, mState);
}

// 第三步：执行动画
private void dispatchLayoutStep3() {
    mItemAnimator.runPendingAnimations();
}
```


#### 滚动机制

RecyclerView 的滚动过程：

1. 用户触发滚动事件
2. RecyclerView 计算滚动距离
3. 调用 LayoutManager 的 scrollHorizontallyBy 或 scrollVerticallyBy 方法
4. LayoutManager 移动现有视图并请求新视图
5. 超出屏幕的视图被回收

```java
// RecyclerView 滚动过程简化代码
@Override
public boolean onTouchEvent(MotionEvent e) {
    // 处理触摸事件...
    switch (action) {
        case MotionEvent.ACTION_MOVE:
            // 计算滚动距离
            int dx = x - mLastX;
            int dy = y - mLastY;

            // 执行滚动
            if (mScrollState != SCROLL_STATE_DRAGGING) {
                startScrollIfNeeded();
            }
            if (scrollByInternal(dx, dy, e)) {
                getParent().requestDisallowInterceptTouchEvent(true);
            }
            break;
    }
}

boolean scrollByInternal(int x, int y, MotionEvent ev) {
    int unconsumedX = 0, unconsumedY = 0;
    int consumedX = 0, consumedY = 0;

    // 调用 LayoutManager 执行滚动
    if (mAdapter != null) {
        if (y != 0) {
            consumedY = mLayoutManager.scrollVerticallyBy(y, mRecycler, mState);
            unconsumedY = y - consumedY;
        }
        if (x != 0) {
            consumedX = mLayoutManager.scrollHorizontallyBy(x, mRecycler, mState);
            unconsumedX = x - consumedX;
        }
    }

    // 分发滚动回调
    dispatchOnScrolled(consumedX, consumedY);
    return consumedX != 0 || consumedY != 0;
}
```

### 预加载机制

RecyclerView 支持预加载，即在用户滚动到数据末尾之前提前加载更多数据：

```java
recyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
    @Override
    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
        LinearLayoutManager layoutManager = (LinearLayoutManager) recyclerView.getLayoutManager();
        int visibleItemCount = layoutManager.getChildCount();
        int totalItemCount = layoutManager.getItemCount();
        int firstVisibleItemPosition = layoutManager.findFirstVisibleItemPosition();

        // 当剩余不到 5 个项目时预加载
        if (!isLoading && !isLastPage) {
            if ((visibleItemCount + firstVisibleItemPosition) >= totalItemCount
                    && firstVisibleItemPosition >= 0
                    && totalItemCount >= PAGE_SIZE) {
                loadMoreItems();
            }
        }
    }
});
```

RecyclerView 常与 Paging 库结合实现高效分页：

```java
// 定义 PagingSource
class UserPagingSource extends PagingSource<Int, User> {
    private val userApi: UserApi

    override suspend fun load(params: LoadParams<Int>): LoadResult<Int, User> {
        try {
            val page = params.key ?: 1
            val pageSize = params.loadSize
            val response = userApi.getUsers(page, pageSize)

            return LoadResult.Page(
                data = response.users,
                prevKey = if (page == 1) null else page - 1,
                nextKey = if (response.users.isEmpty()) null else page + 1
            )
        } catch (e: Exception) {
            return LoadResult.Error(e)
        }
    }
}

// 创建 Pager
val flow = Pager(
    config = PagingConfig(
        pageSize = 20,
        enablePlaceholders = false,
        prefetchDistance = 5
    ),
    pagingSourceFactory = { UserPagingSource(userApi) }
).flow

// 在 ViewModel 中收集并转换为 LiveData
val users = flow.cachedIn(viewModelScope).asLiveData()

// 在 Activity/Fragment 中设置
val pagingAdapter = UserPagingAdapter()
recyclerView.adapter = pagingAdapter

viewModel.users.observe(this) { pagingData ->
    pagingAdapter.submitData(lifecycle, pagingData)
}
```

### 总结
RecyclerView 是 Android 中展示大量数据的首选组件，其核心优势在于：

1. 高效的视图回收机制：通过四级缓存结构最大限度复用视图
2. 灵活的布局系统：支持线性、网格、瀑布流等多种布局方式
3. 丰富的动画效果：内置添加、删除、移动等动画
4. 组件化设计：各组件职责明确，易于扩展和定制

# 系统服务

## 系统进程级核心服务（System Services）

由 SystemServer 进程启动的，提供 Android Framework 的基础功能，可以用 adb shell service list 查看。常见的重要服务有：

| 服务名称                             | 作用                  |
| -------------------------------- | ------------------- |
| **ActivityManagerService (AMS)** | 管理四大组件生命周期、进程调度、任务栈 |
| **PackageManagerService (PMS)**  | 管理应用安装、卸载、签名验证      |
| **WindowManagerService (WMS)**   | 管理窗口、界面布局、输入事件      |
| **PowerManagerService**          | 电源管理、休眠/唤醒策略        |
| **AlarmManagerService**          | 定时任务调度              |
| **InputManagerService**          | 触控、键盘等输入事件分发        |
| **SensorService**                | 传感器数据               |
| **LocationManagerService**       | 位置/定位服务             |
| **ConnectivityService**          | 网络连接、Wi-Fi/蜂窝数据管理   |
| **AudioService**                 | 音频路由、音量控制           |
| **SurfaceFlinger**（独立进程）         | 最终合成与显示图像的渲染服务      |


## HAL / Native Daemon 层

这些服务在 native 层实现，与硬件交互，由 init 或 hwservicemanager 启动：
- init：系统启动的第一个进程，解析 init.rc 脚本，启动所有守护进程
- hwservicemanager：HIDL/ AIDL HAL 服务注册与发现
- servicemanager：Binder 的注册中心
- 各类硬件 HAL，如 camera HAL、audio HAL、bluetooth HAL 等

## 运行时支持服务

- Zygote：Java 进程孵化器，用于 fork 应用进程
- Binder：IPC 核心机制，所有 system service 都通过 Binder 通信
- ART/Dalvik：应用运行时虚拟机

## 应用可见的系统服务接

开发者在 App 里通过 Context.getSystemService() 获取的各种服务，其背后就是上述的 framework 服务。
