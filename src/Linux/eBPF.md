<!--toc:start-->
- [eBPF学习](#ebpf学习)
- [eBPF-go](#ebpf-go)
  - [Loading](#loading)
    - [CollectionSpec](#collectionspec)
    - [NewCollection](#newcollection)
    - [LoadAndAssign](#loadandassign)
  - [Global Variables](#global-variables)
    - [运行期常量](#运行期常量)
  - [加载前使用VariableSpec](#加载前使用variablespec)
  - [Internal/Hidden全局变量](#internalhidden全局变量)
- [代码](#代码)
  - [XDP钩子函数](#xdp钩子函数)
  - [挂载](#挂载)
<!--toc:end-->

# eBPF学习

简单理解，eBPF允许程序在特权上下文环境中运行程序的技术，允许在不修改内核源代码的前提下拓展内核的功能。

eBPF程序是通过一个eBPF虚拟机来执行，通常是解释执行或者JIT Compiled。采用的是事件驱动的思想。

```c
struct bpf_insn {
    __u8    code;       // 操作码
    __u8    dst_reg:4;  // 目标寄存器
    __u8    src_reg:4;  // 源寄存器
    __s16   off;        // 偏移量
    __s32   imm;        // 立即操作数
};
```

这是一个bpf汇编指令的格式。

code：指令操作码，如 mov、add 等。

dst_reg：目标寄存器，用于指定要操作哪个寄存器。

src_reg：源寄存器，用于指定数据来源于哪个寄存器。

off：偏移量，用于指定某个结构体的成员。

imm：立即操作数，当数据是一个常数时，直接在这里指定。

# eBPF-go

## Loading

eBPF的C语言是一门受限的方言，使用LLVM工具链，所以用go来写会更加理想。

ebpf-go项目中自带了一个bpf object(ELF) loader。

一个ELF文件到内核资源的流程：

1. 将ELF文件解析成Go中的类型(CollectionSpec)，包括：MapSpec，Types，ProgramSpec。其中MapSpec，ProgramSpec会被传递到Collection并链接。

### CollectionSpec

代表了从ELF文件中提取出来的eBPF对象，并且可以用LoadCollectionSpec来获得。

如下：

```c
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

// Declare a hash map called 'my_map' with a u32 key and a u64 value.
// The __uint, __type and SEC macros are from libbpf's bpf_helpers.h.
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, __u32);
    __type(value, __u64);
    __uint(max_entries, 1);
} my_map SEC(".maps");

// Declare a dummy socket program called 'my_prog'.
SEC("socket") int my_prog() {
    return 0;
}
```

```go
// Parse an ELF into a CollectionSpec.
// bpf_prog.o is the result of compiling BPF C code.
spec, err := ebpf.LoadCollectionSpec("bpf_prog.o")
if err != nil {
    panic(err)
}

// Look up the MapSpec and ProgramSpec in the CollectionSpec.
m := spec.Maps["my_map"]
p := spec.Programs["my_prog"]
// Note: We've omitted nil checks for brevity, take a look at
// LoadAndAssign for an automated way of checking for maps/programs.

// Inspect the map and program type.
fmt.Println(m.Type, p.Type)

// Print the map's key and value BTF types.
fmt.Println(m.Key, m.Value)

// Print the program's instructions in a human-readable form,
// similar to llvm-objdump -S.
fmt.Println(p.Instructions)
```

### NewCollection

在解析ELF到CollectionSpec之后，就可以通过go的NewCollection加载到kernel中，结果是一个Collection。

```go
spec, err := ebpf.LoadCollectionSpec("bpf_prog.o")
if err != nil {
    panic(err)
}

// Instantiate a Collection from a CollectionSpec.
coll, err := ebpf.NewCollection(spec)
if err != nil {
    panic(err)
}
// Close the Collection before the enclosing function returns.
defer coll.Close()

// Obtain a reference to 'my_map'.
m := coll.Maps["my_map"]

// Set map key '1' to value '2'.
if err := m.Put(uint32(1), uint64(2)); err != nil {
    panic(err)
}
```

### LoadAndAssign

用来取代NewCollection的API，主要好处：
- 自动的拉取Maps和Programs而不需要手动的进行。
- 选择性的加载Maps和Programs。

```go
spec, err := ebpf.LoadCollectionSpec("bpf_prog.o")
if err != nil {
    panic(err)
}

// Insert only the resources specified in 'obj' into the kernel and assign
// them to their respective fields. If any requested resources are not found
// in the ELF, this will fail. Any errors encountered while loading Maps or
// Programs will also be returned here.
var objs myObjs
if err := spec.LoadAndAssign(&objs, nil); err != nil {
    panic(err)
}
defer objs.Close()

// Interact with MyMap through the custom struct.
if err := objs.MyMap.Put(uint32(1), uint64(2)); err != nil {
    panic(err)
}
```

## Global Variables

ebpf-go能够通过VariableSpec来与全局变量进行交互。

### 运行期常量

常量通常用于程序的配置，C编译器不允许修改这些变量。

如果一个条件恒真/假，就会对删除无用的代码。

```c
volatile __u16 global_u16;

SEC("socket") int global_example() {
    global_u16++;
    return global_u16;
}

```

## 加载前使用VariableSpec

在加载BPF程序到Kernel之前可以用VariableSpec与全局变量交互。

```go
set := uint16(9000)
if err := spec.Variables["global_u16"].Set(set); err != nil {
    panicf("setting variable: %s", err)
}
```

```go
for range 3 {
    ret, _, err := coll.Programs["global_example"].Test(make([]byte, 15))
    if err != nil {
        panicf("running BPF program: %s", err)
    }
    fmt.Println("BPF program returned", ret)
}

// Output:
// Running program with global_u16 set to 9000
// BPF program returned 9000
// BPF program returned 9001
// BPF program returned 9002
```

## Internal/Hidden全局变量

如果不希望用户空间能够与某一个变量交互，可以使用__hidden属性来声明变量。

```c
__hidden __u64 hidden_var;

SEC("socket") int hidden_example() {
    hidden_var++;
    return hidden_var;
}
```

这就会让hidden_var的VariableSpec不会出现在CollectionSpec中。

# 代码

```c
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, __u32);
    __type(value, __u64);
    __uint(max_entries, 1);
} pkt_count SEC(".maps");
// count_packets atomically increases a packet counter on every invocation.
SEC("xdp")
int count_packets() {
    __u32 key    = 0;
    __u64 *count = bpf_map_lookup_elem(&pkt_count, &key);
    if (count) {
        __sync_fetch_and_add(count, 1);
    }
    return XDP_PASS;
}
```

这段代码中的pkt_count的作用是定义一个数组类型的eBPF映射，从u32的key映射到u64的value，max\_entries是说数组只有一个元素。

## XDP钩子函数

SEC("xdp") 声明这个函数是 XDP 程序的一部分，会在 网络包到达网卡时触发。

bpf_map_lookup_elem(&pkt_count, &key) 查找 pkt_count 这个 eBPF map 中 key=0 对应的值，即数据包计数器。

如果找到了 count，就使用 __sync_fetch_and_add(count, 1); 原子性地增加计数，防止并发访问时的竞争条件（race condition）。

return XDP_PASS; 让数据包正常通过，不会被丢弃或修改。

## 挂载

eBPF的程序是需要挂载点的，也就是说，
