<!--toc:start-->
- [Jamvm——一个完整的java 1.5 版本的JVM实现](#jamvm一个完整的java-15-版本的jvm实现)
  - [主体部分](#主体部分)
- [JVM模块](#jvm模块)
  - [前置知识](#前置知识)
    - [JVM Byte Code格式](#jvm-byte-code格式)
  - [启动与关闭部分](#启动与关闭部分)
    - [findClassFromClassLoader部分](#findclassfromclassloader部分)
    - [lookupMethod部分](#lookupmethod部分)
    - [启动部分](#启动部分)
      - [方法调用栈](#方法调用栈)
      - [在栈帧中的操作](#在栈帧中的操作)
    - [结束部分](#结束部分)
  - [类加载器](#类加载器)
    - [方法查询](#方法查询)
    - [方法执行](#方法执行)
  - [类初始化阶段](#类初始化阶段)
<!--toc:end-->

# Jamvm——一个完整的java 1.5 版本的JVM实现

JamVM是一个不再更新的完整的Java 1.5 JVM的实现。这里学习一下JamVM的实现，了解一个完整的JVM实现。

## 主体部分

先分析一下main函数中的整体实现：

```c
int main(int argc, char *argv[]) {
    Class *array_class, *main_class;
    Object *system_loader, *array;
    MethodBlock *mb;
    InitArgs args;
    int class_arg;
    char *cpntr;
    int status;
    int i;

    setDefaultInitArgs(&args);
    class_arg = parseCommandLine(argc, argv, &args);

    args.main_stack_base = &array_class;
    ...
}
```

这里是整个JVM需要用到的资源，包括所有的class文件、main class文件、class loader、方法块(Method Block)等。

下一步就需要初始化Java虚拟机。

```cpp
// jam.c
int main(int argc,char* argv[]){
...
initVM(&args);
...
}
//init.c
void initVM(InitArgs *args) {
    /* Perform platform dependent initialisation */
    initialisePlatform();

    /* Initialise the VM modules -- ordering is important! */
    initialiseHooks(args);
    initialiseProperties(args);
    initialiseAlloc(args);
    initialiseDll(args);
    initialiseUtf8();
    initialiseThreadStage1(args);
    initialiseSymbol();
    initialiseClass(args);
    initialiseMonitor();
    initialiseString();
    initialiseException();
    initialiseNatives();
    initialiseJNI();
    initialiseInterpreter(args);
    initialiseThreadStage2(args);
    initialiseGC(args);
    VM_initing = FALSE;
}
```

这里的重点方法就是initVM(&args)。

首先就是通过initialisePlatform函数来处理各种不同平台上的初始化操作，然后用虚拟化hooks用来处理信号处理、回调函数等场景，Properties表示的是初始化JVM相关参数，包括内存、系统变量等，Alloc负责管理内存分配子系统，用于处理虚拟机的内存分配和释放。

- ThreadStage1：执行线程子系统的第一阶段初始化，可能涉及基本的数据结构设置和主线程的创建。
- Symbol：初始化符号表，用于存储和查找类名、方法名等符号信息。
- Class：初始化类加载器和类相关的数据结构，负责加载、解析和链接 Java 类。
- Monitor：初始化监视器（同步机制），支持 Java 的同步块和方法，确保线程安全。
- String：初始化字符串处理模块，管理 Java 字符串的创建和操作。
- Exception：初始化异常处理机制，支持 Java 的异常抛出和捕获功能。
- Native：注册和初始化本地方法，允许 Java 调用底层的本地代码。
- JNI：初始化 Java 本地接口（JNI），提供 Java 与本地代码交互的接口。
- Interpreter：初始化字节码解释器，负责执行 Java 字节码指令。
- ThreadStage2：执行线程子系统的第二阶段初始化，可能涉及线程调度和同步机制的设置。
- GC：初始化垃圾回收器，管理内存回收，防止内存泄漏。

执行完InitVM(init.c:72)之后，通过getSystemClassLoader(class.c:1549)函数来获取system_loader。

然后在mainThreadSetContextClassLoader(thread.c:1178)函数中，将主线程的上下文类加载器设置为系统类加载器。

将命令行中的主类名中的'.'号替换为'/'以符合内部类命名法。

用findClassFromClassLoader(class.c:1539)去找到主类，非空则使用initClass(class.c:1188)来把主类初始化。

MethodBlock使用lookupMethod(resolve.c:56)函数来进行初始化，这里需要找到main方法并且需要保证main方法是静态的。

最后一个阶段：创建一个 String 类型的数组，用于存储命令行参数（不包括类名本身）。然后，将剩余的命令行参数转换为 Java 字符串并填充到该数组中。最后，调用(executeStaticMethod, jam.h:880，并不是一个函数，而是对executeMethodArgs的封装)主类的 main 方法，并传入该参数数组。

执行结束时的操作：

等待main方法执行完成mainThreadWaitToExitVM(thread.c:1163)，等到所有的线程(除了daemon线程)终止。然后调用exitVM(thread.c:1144)结束JVM。

# JVM模块

## 前置知识

### JVM Byte Code格式

```c
ClassFile {
    u4 magic;
    u2 minor_version;
    u2 major_version;
    u2 constant_pool_count;
    cp_info constant_pool[constant_pool_count-1];
    u2 access_flags;
    u2 this_class;
    u2 super_class;
    u2 interfaces_count;
    u2 interfaces[interfaces_count];
    u2 fields_count;
    field_info fields[fields_count];
    u2 methods_count;
    method_info methods[methods_count];
    u2 attributes_count;
    attribute_info attributes[attributes_count];
}
```

在JVM规范中u1、u2、u4分别表示的是1、2、4个字节的无符号数，可使用java.io.DataInputStream类中的对应方法：readUnsignedByte、readUnsignedShort、readInt方法读取。除此之外，表结构(table)由任意数量的可变长度的项组成，用于表示class中的复杂结构，如上述的：cp\_info、field\_info、method\_info、attribute\_info。

- 其中magic表示的是一个固定的值，表示为0xCAFE_BABE。JVM加载class文件时会先读取4字节（u4 magic;）的魔数信息校验是否是一个class文件。

- class文件的版本号由两个u2组成（u2 minor\_version; u2 major\_version;），分别表示的是minor\_version（副版本号）、major\_version （主版本号）。

- constant\_pool\_count：常量池计数器。

- constant\_pool：常量池，cp_info表示的是常量池对象。

```c
cp_info {
   u1 tag;
   u1 info[];
}
```

tag表示的是常量池中的存储类型。

- access\_flags：表示的是某个类或者接口的访问权限及属性。
- this_class：当前类的名称。
- super_class：父类的名称。
- interfaces_count：当前类实现的接口数。
- interfaces：表示所有的接口。
- fields_count：表示成员变量数。
- fields：成员变量数组。

```c
field_info {
   u2 access_flags;
   u2 name_index;
   u2 descriptor_index;
   u2 attributes_count;
   attribute_info attributes[attributes_count];
}
```


    u2 access_flags;表示的是成员变量的修饰符；
    u2 name_index;表示的是成员变量的名称；
    u2 descriptor_index;表示的是成员变量的描述符；
    u2 attributes_count;表示的是成员变量的属性数量；
    attribute_info attributes[attributes_count];表示的是成员变量的属性信息；

- methods_count：方法数。
- methods：method_info methods[methods_count];表示的是当前class中的所有成员方法，method_info表示的是成员方法对象。

```c
method_info {
   u2 access_flags;
   u2 name_index;
   u2 descriptor_index;
   u2 attributes_count;
   attribute_info attributes[attributes_count];
}
```

- attributes_count：当前class文件属性表的成员个数。
- attributes：当前class文件所有属性。

## 启动与关闭部分

首先，jvm想要运行一个class文件就必须先找到main这个入口函数，这是所有JVM通用的。

结合主体部分，可以知道，执行main入口包含两部分：

1. 找到main class，这一过程由findClassFromClassLoader；找到main block，这一步由lookupMethod进行。
2. 找到静态的main方法，这一过程由解析ArrayClass(jam.h:827)和allocArray(alloc.c:2042)完成，将整个命令行参数解析为String[]之后，在最后一个class文件的位置执行main\_class的静态main方法。

### findClassFromClassLoader部分

函数如下：

```c
Class *findClassFromClassLoader(char *classname, Object *loader) {
    if (*classname == '[') {
        log_message("LOG", "Entering function: findArrayClassFromClassloader!\n");
        return findArrayClassFromClassLoader(classname, loader);
    }

    if (loader != NULL) {
        log_message("LOG", "Entering function: findNonArrayClassFromClassloader!\n
        return findNonArrayClassFromClassLoader(classname, loader);
    }
    log_message("LOG", "Entering function: findSystemClass0!\n");
    return findSystemClass0(classname);
}
```

传入的参数classname是一个字符串的形式，其中的调用格式如下：

```gdb
findClassFromClassLoader (classname=0x5555555834f5 "java/lang/Object", loader=0x0)
```

如果classname的第一个字母为[就说明，请求的是一个数组类型，比如'[Ljava/util/HashTable$HashEntry;'就说明是一个HashTable[]，这时候就会调用findArrayClassFromClassLoader来查找。

如果loader非空，就说明有一个用户自定的类发生了查找(这之后，loader都不会为空)。

函数入参中的loader只有在classname不是数组或系统类的时候才不会为空。

从findArrayClassFromClassLoader的函数中，有一个createArrayClass(class.c:571)的函数，其中展示了数组类的父类——Object。

并且在其中可以看到其需要实现Cloneable和Serializable这两个接口。

所有的Class文件在被加载之后，都需要通过addClassToHash函数(class.c:111)被添加到boot\_class或者是class\_loader之中。

### lookupMethod部分

这一部分需要做的是，从main\_class中寻找到main方法，这里有一个SYMBOL宏。

结果以mb(MethodBlock)的形式显示。

首先从findMethod(resolve.c:28)寻找mb，也就是当前class寻找，如果找不到就说明方法不在当前类，就需要向父类查找。

```c
MethodBlock *lookupMethod(Class *class, char *methodname, char *type) {
    MethodBlock *mb;
    if((mb = findMethod(class, methodname, type)))
       return mb;
    if(CLASS_CB(class)->super)
        return lookupMethod(CLASS_CB(class)->super, methodname, type);
    return NULL;
}
```

自此获得了main method以及main class。

### 启动部分

整个JVM开始执行main\_class和main method的位置在executeStaticMethod(jam.h:880)。这是一个宏，是对于executeMethodArgs(execute.c:95)的封装。

```c
void *executeMethodArgs(Object *ob, Class *class, MethodBlock *mb, ...) {
    va_list jargs;
    void *ret;
    va_start(jargs, mb);
    ret = executeMethodVaList(ob, class, mb, jargs);
    va_end(jargs);
    return ret;
}
```

这里的代码并不复杂，使用C中的va\_list机制，解析出来所有的参数，然后把用参数放入到jargs，使用executeMathodVaList(execute.c:106)来真正的执行。

```c
void *executeMethodVaList(Object *ob, Class *class, MethodBlock *mb,
                          va_list jargs) {
    ExecEnv *ee = getExecEnv();
    char *sig = mb->type;
    uintptr_t *sp;
    void *ret;
    CREATE_TOP_FRAME(ee, class, mb, sp, ret);
    /* copy args onto stack */
    if(ob)
        *sp++ = (uintptr_t) ob; /* push receiver first */
    SCAN_SIG(sig, VA_DOUBLE(jargs, sp), VA_SINGLE(jargs, sp))
    if(mb->access_flags & ACC_SYNCHRONIZED)
        objectLock(ob ? ob : mb->class);
    if(mb->access_flags & ACC_NATIVE)
        (*mb->native_invoker)(class, mb, ret);
    else
        executeJava();
    if(mb->access_flags & ACC_SYNCHRONIZED)
        objectUnlock(ob ? ob : mb->class);
    POP_TOP_FRAME(ee);
    return ADJUST_RET_ADDR(ret, *sig);
}
```

首先获取当前的执行环境ee，然后获得方法的签名，sp可以理解为栈指针，ret就是方法的返回值。

#### 方法调用栈

CREATE_TOP_FRAME这个宏来创建方法栈帧(frame.h:26)。

```c
#define CREATE_TOP_FRAME(ee, class, mb, sp, ret)                \
{                                                               \
    // 获取当前线程的栈顶，表示调用方法之前的栈帧。
    Frame *last = ee->last_frame;                               \
    // dummy紧接着last->ostack来分配，作为新的栈帧的占位符
    Frame *dummy = (Frame *)(last->ostack+last->mb->max_stack); \
    Frame *new_frame;                                           \
    uintptr_t *new_ostack;                                      \
                                                                \
    // sp指向的是存放局部数据的位置的起始地址，同时将其转换为void*指针从而能够保存返回值的位置
    ret = (void*) (sp = (uintptr_t*)(dummy+1));                 \
    // new_frame表示在sp开始的位置分配max_locals大小，指向的是新的栈帧的元数据结构
    new_frame = (Frame *)(sp + mb->max_locals);                 \
    //new_ostack表示新的操作符栈
    new_ostack = ALIGN_OSTACK(new_frame + 1);                   \
                                                                \
    if((char*)(new_ostack + mb->max_stack) > ee->stack_end) {   \
        // 栈溢出了
        if(ee->overflow++) {                                    \
            /* Overflow when we're already throwing stack       \
               overflow.  Stack extension should be enough      \
               to throw exception, so something's seriously     \
               gone wrong - abort the VM! */                    \
            printf("Fatal stack overflow!  Aborting VM.\n");    \
            exitVM(1);                                          \
        }                                               \
        // 扩大栈的大小
        ee->stack_end += STACK_RED_ZONE_SIZE;                   \
        signalException(java_lang_StackOverflowError, NULL);    \
        return NULL;                                            \
    }                                                           \
                                                                \
    // dummy是一个过渡帧，他没有真实的方法
    dummy->mb = NULL;                                           \
    // ostack指向局部变量区
    dummy->ostack = sp;                                         \
    // prev指向的是调用这个方法的栈帧
    dummy->prev = last;                                         \
                                                                \
    // new_frame才是要得到的栈帧
    new_frame->mb = mb;                                         \
    new_frame->lvars = sp;                                      \
    new_frame->ostack = new_ostack;                             \
                                                                \
    // 链接dummy，保证完整性
    new_frame->prev = dummy;                                    \
    // 更新ee->last_frame到当前方法的栈帧
    ee->last_frame = new_frame;                                 \
}
```

JVM每次调用一个方法的时候，都会创建一个栈帧，存储着方法的局部变量(lvars)、操作数栈(ostack)、返回地址以及上一层调用帧(prev)。

```c
#define POP_TOP_FRAME(ee)                                       \
    ee->last_frame = ee->last_frame->prev->prev;
```

这里的POP\_TOP\_FRAME就很清晰了，就是说出了一个方法之后，将方法栈帧的顶部pop掉。

#### 在栈帧中的操作

经过CREATE\_TOP\_FRAME之后，就已经进入了新的方法栈帧中。

如果ob部位空指针，就把对象引用ob压入栈中。这里的作用就类似于Java中的this。

```c
#define SCAN_SIG(p, D, S)           \
   p++;               /* skip start ( */    \
   while(*p != ')') {                \
       if((*p == 'J') || (*p == 'D')) {        \
          D;                    \
          p++;                  \
      } else {                  \
          S;                    \
          if(*p == '[')              \
              for(p++; *p == '['; p++);      \
          if(*p == 'L')              \
              while(*p++ != ';');        \
          else                  \
              p++;              \
      }                     \
   }                        \
   p++;               /* skip end ) */
```

这个SCAN\_SIG宏能够解析方法签名。VA\_DOUBLE和VA\_SINGLE就是用来处理类型的。

接下来就是处理同步方法，如果是同步方法，就给对象上一个锁。

```c
// execute.c:123
if(mb->access_flags & ACC_SYNCHRONIZED)
    objectLock(ob ? ob : mb->class);
```

objectLock方法在lock.c的340行实现。

```c
if(mb->access_flags & ACC_NATIVE)
    (*mb->native_invoker)(class, mb, ret);
else
    executeJava();
```

这一段代码区分是Java方法还是Native方法，如果是Native方法就需要通过native\_invoker以函数指针的形式来执行。

```c
if(mb->access_flags & ACC_SYNCHRONIZED)
    objectUnlock(ob ? ob : mb->class);
```

执行完上面那一步之后，就已经把整个方法执行完了，也就是说，在executeJava和native_invoker中的是执行代码的关键部分。如果是synchronized方法执行完了，就需要给对象解锁。

执行完之后进行一个POP\_TOP\_FRAME的操作，然后用ADJUST\_RET\_ADDR来获取返回信息。

### 结束部分

在JVM所有方法执行完之后，需要关闭JVM，这里调用的是mainThreadWaitToExitVM方法，这个方法之后会调用threadSelf方法获得thread，然后对self禁用suspend，修改self thread的状态，等待条件变量。最后通过exitVM方法就可以退出，原理是通过调用JVM的System Class Library中的exit方法。JVM本身的推出则是通过jamvm\_exit方法的调用。

## 类加载器

首先来看GetSystemClassLoader(class.c:1551)这个函数。

```c
Object *getSystemClassLoader() {
    Class *class_loader = findSystemClass(SYMBOL(java_lang_ClassLoader));
    if(!exceptionOccurred()) {
        MethodBlock *mb;
        if((mb = findMethod(class_loader, SYMBOL(getSystemClassLoader),
                                          SYMBOL(___java_lang_ClassLoader))) != NULL) {
            Object *system_loader = *(Object**)executeStaticMethod(class_loader, mb);
            if(!exceptionOccurred())
                return system_loader;
        }
    }
    return NULL;
}
```

Java的Class Loader并不是JVM来提供的，但是JVM要负责处理类加载的流程。

在这里就可以看到，getSystemClassLoader函数需要先找到Java的class loader，然后找到class loader的getSystemClassLoader方法，这时候就获得了system\_loader。

也就是说，class loader并不由JVM提供，而是由Java提供，但是具体的执行由JVM来进行。

### 方法查询

findMethod方法提供了从class\_loader查询某一个方法的能力。

```c
MethodBlock *findMethod(Class *class, char *methodname, char *type) {
   ClassBlock *cb = CLASS_CB(class);
   MethodBlock *mb = cb->methods;
   int i;
   for(i = 0; i < cb->methods_count; i++,mb++)
       if(mb->name == methodname && mb->type == type)
          return mb;
   return NULL;
}
```

这里面有一个点就是，Class的struct是一个非常tricky的实现，本质上Class就是一个单链表。

### 方法执行

在system class loader成功找出来方法之后，就需要对这个class loader中的方法块中的静态方法执行，也就是说Java中的静态方法是在加载的时候就执行的。

这里的executeStaticMethod并不是函数，而是一个宏定义，封装的是executeMethodArgs。

## 类初始化阶段

在类被加载之后(解析、验证工作都已经完成)，就需要被初始化。

```c
Class *initClass(Class *class) {
   ClassBlock *cb = CLASS_CB(class);
   ConstantPool *cp = &cb->constant_pool;
   FieldBlock *fb = cb->fields;
   MethodBlock *mb;
   Object *excep;
   int state, i;
   if(cb->state >= CLASS_INITED)
      return class;
   linkClass(class);
   objectLock(class);
   while(cb->state == CLASS_INITING)
      if(cb->initing_tid == threadSelf()->id) {
         objectUnlock(class);
         return class;
      } else {
          /* FALSE means this wait is non-interruptible.
             An interrupt will appear as if the initialiser
             failed (below), and clearing will lose the
             interrupt status */
          objectWait0(class, 0, 0, FALSE);
      }
   if(cb->state >= CLASS_INITED) {
      objectUnlock(class);
      return class;
   }
   if(cb->state == CLASS_BAD) {
       objectUnlock(class);
       signalException(java_lang_NoClassDefFoundError, cb->name);
       return NULL;
   }
   cb->state = CLASS_INITING;
   cb->initing_tid = threadSelf()->id;
   objectUnlock(class);
   if(!(cb->access_flags & ACC_INTERFACE) && cb->super
              && (CLASS_CB(cb->super)->state != CLASS_INITED)) {
      initClass(cb->super);
      if(exceptionOccurred()) {
          state = CLASS_BAD;
          goto set_state_and_notify;
      }
   }
   /* Never used to bother with this as only static finals use it and
      the constant value's copied at compile time.  However, separate
      compilation can result in a getstatic to a (now) constant field,
      and the VM didn't initialise it... */
   for(i = 0; i < cb->fields_count; i++,fb++)
      if((fb->access_flags & ACC_STATIC) && fb->constant) {
         if((*fb->type == 'J') || (*fb->type == 'D'))
            fb->u.static_value.l = *(u8*)&(CP_INFO(cp, fb->constant));
         else
            fb->u.static_value.u = resolveSingleConstant(class, fb->constant);
      }
   if((mb = findMethod(class, SYMBOL(class_init), SYMBOL(___V))) != NULL)
      executeStaticMethod(class, mb);
   if((excep = exceptionOccurred())) {
       Class *error, *eiie;
       clearException();
       /* Don't wrap exceptions of type java.lang.Error... */
       if((error = findSystemClass0(SYMBOL(java_lang_Error)))
                 && !isInstanceOf(error, excep->class)
                 && (eiie = findSystemClass(SYMBOL(java_lang_ExceptionInInitializerError)))
                 && (mb = findMethod(eiie, SYMBOL(object_init), SYMBOL(_java_lang_Throwable__V)))) {
           Object *ob = allocObject(eiie);
           if(ob != NULL) {

               executeMethod(ob, mb, excep);
               setException(ob);
           }
       } else
           setException(excep);
       state = CLASS_BAD;
   } else
       state = CLASS_INITED;
set_state_and_notify:
   objectLock(class);
   cb->state = state;

   objectNotifyAll(class);
   objectUnlock(class);

   return state == CLASS_BAD ? NULL : class;
}
```

这里的第一步是确保类不会被重复初始化，接着需要对类进行链接(linkClass,class.c:844)，确保类的字段、方法等结构已经解析完成。

# 垃圾回收部分
