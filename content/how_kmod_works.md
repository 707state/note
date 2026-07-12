---
title: kmod是怎么运行的
author: jask、DeepSeek V4 Flash
series: Linux自我修养
tags:
  - Linux
date: 2026-07-05
done: 1
LLM: 1
---

# 只看重点

insmod工具的重点是`do_insmod`函数，调用`libkmod`的函数:

```c
 err = kmod_module_new_from_path(ctx, argv[i], &mod);   // 通过路径解析 .ko 文件                                       
 err = kmod_module_insert_module(mod, flags, options);   // 核心加载函数
```

核心的插入流程在`libkmod-module.c`中的`kmod_module_insert_module`：

```c
	err = do_finit_module(mod, flags, args);
	if (err == -ENOSYS)			       ```
		err = do_init_module(mod, flags, args);
		
// do_finit_module
   // 检查内核是否支持模块压缩                                                                                                                                                              
   compression = kmod_file_get_compression(mod->file);                                                                                                                                      
   kernel_compression = kmod_get_kernel_compression(mod->ctx);                                                                                                                              
   if (compression != NONE && compression != kernel_compression)                                                                                                                            
       return -ENOSYS;  // 内核不支持此压缩格式 → 回退                                                                                                                                      
                                                                                                                                                                                            
   // 对应标志位:                                                                                                                                                                           
   // MODULE_INIT_IGNORE_MODVERSIONS = 1                                                                                                                                                    
   // MODULE_INIT_IGNORE_VERMAGIC    = 2                                                                                                                                                    
   // MODULE_INIT_COMPRESSED_FILE    = 4                                                                                                                                                    
   err = finit_module(mod->file->fd, args, kernel_flags);                                                                                                                                   
```
这里面调用一个`finit_module`系统调用，没有然后了。

rmmod工具也是类似的逻辑，在`delete_module.c`有完整的`delete_module` mock实现:

```c
long delete_module(const char *modname, _maybe_unused_ unsigned int flags)
{                                                                         
        DECLARE_STRBUF_WITH_STACK(buf, PATH_MAX);                         
        struct mod *mod;                                                  
        int ret = 0;                                                      
                                                                          
        init_retcodes();                                                  
        mod = find_module(modules, modname);                              
        if (mod == NULL)                                                  
                return 0;                                                 
                                                                          
        if (!strbuf_pushchars(&buf, "/sys/module/") ||                    
            !strbuf_pushchars(&buf, modname)) {                           
                errno = ENOMEM;                                           
                return -1;                                                
        }                                                                 
                                                                          
        ret = remove_directory(strbuf_str(&buf));                         
        if (ret != 0)                                                     
                return ret;                                               
                                                                          
        errno = mod->errcode;                                             
                                                                          
        return mod->ret;                                                  
}                                                                         
```

真实的`delete_module`是通过libc调用的，libc封装了`delete_module`系统调用。

# 设计取舍

## 1. 模块压缩：魔法字节分发 + 条件编译后端

`libkmod/libkmod-file.c` 从 `.ko` 文件开头读 7 字节，与 ZSTD/XZ/gzip 的魔数匹配后选择对应的 `load` 函数指针：

```c
static const struct comp_type {
       size_t magic_size;
       enum kmod_file_compression_type compression;
       const char *magic_bytes;
       int (*load)(struct kmod_file *file);
} comp_types[] = {
       { sizeof(magic_zstd), KMOD_FILE_COMPRESSION_ZSTD, magic_zstd, kmod_file_load_zstd },
       { sizeof(magic_xz),   KMOD_FILE_COMPRESSION_XZ,   magic_xz,   kmod_file_load_xz },
       { sizeof(magic_zlib), KMOD_FILE_COMPRESSION_ZLIB, magic_zlib, kmod_file_load_zlib },
       { 0, KMOD_FILE_COMPRESSION_NONE, NULL, load_reg },
};
```

每个后端在单独文件中实现（`libkmod-file-xz.c` / `libkmod-file-zlib.c` / `libkmod-file-zstd.c`），通过 `#if ENABLE_XZ` 等编译开关控制。
`libkmod-internal-file.h` 中，被禁用的后端编译为内联 `return -ENOSYS`。

| 取舍 | 选择 | 代价 |
|------|------|------|
| 加载时机 | 延迟加载：`kmod_file_get_contents()` 首次调用时才解压 | 第一次访问有延迟毛刺 |
| 后端策略 | 条件编译 + 函数指针表，而非统一的 dlopen | 新增压缩格式需要改三处文件 |
| 解压方式 | XZ/Zlib 用流式 API + malloc，Zstd 用 mmap + 一次性解压 | 内存模型不一致，Zstd 路径对大模块瞬时内存翻倍 |

内核通过 `finit_module` 的 `MODULE_INIT_COMPRESSED_FILE` 标志接收已压缩的模块文件，libkmod 会检查自己的压缩格式是否与内核匹配（`kmod_file_get_compression` vs `kmod_get_kernel_compression`），不匹配则回退到 `init_module` 路径（先解压再传入）。

## 2. Patricia Trie 索引：双实现（FILE vs mmap）

`libkmod-index.c` 中的索引文件格式是 **Patricia trie（路径压缩字典树）**。磁盘格式用一个三元组标记节点属性（`INDEX_NODE_PREFIX` / `INDEX_NODE_VALUES` / `INDEX_NODE_CHILDS`）：

```c
enum node_offset {
       INDEX_NODE_FLAGS = 0xF0000000,
       INDEX_NODE_PREFIX = 0x80000000,
       INDEX_NODE_VALUES = 0x40000000,
       INDEX_NODE_CHILDS = 0x20000000,
       INDEX_NODE_MASK   = 0x0FFFFFFF,
};
```

同一个文件中包含两套完全独立的搜索实现：
- `index_file_*` — 基于 `FILE*` + `getc_unlocked`，顺序读取节点
- `index_mm_*` — 基于 `mmap`，直接指针访问

| 取舍 | FILE 实现 | mmap 实现 |
|------|-----------|-----------|
| 内存占用 | 只保留当前路径，无长期开销 | 整个索引文件映射到虚拟地址空间 |
| 查找延迟 | 每次查找需要 read syscall + 内存拷贝 | 指针解引用即可，无系统调用 |
| 使用场景 | `kmod_dump_index`（一次性导出） | `index_search` / `index_searchwild`（频繁查找） |
| 实现量 | 同一种算法写了两遍，~600行重复逻辑 | |

选择维护两套实现而非统一抽象的原因是：mmap 路径在多次查找时节省大量系统调用，对 modprobe 这种频繁索引查找的场景至关重要；FILE 路径则在"只读一次"的场景下无需分配虚拟地址空间。

## 3. 自定义哈希表：每桶内排序 + bsearch

`shared/hash.c` 实现了一个开放寻址哈希表，但每个桶内是 **排序数组**：

```c
// 插入时二分查找位置，memmove 腾位
for (; entry < entry_end; entry++) {
       int c = strcmp(key, entry->key);
       if (c == 0) { /* replace */ ... }
       else if (c < 0) {
               memmove(entry + 1, entry, ...);
               break;
       }
}
// 查找时 bsearch
entry = bsearch(&se, bucket->entries, bucket->used, ..., hash_entry_cmp);
```

哈希函数是自行实现的 `hash_superfast`，而非标准 djb2 / FNV。

| 取舍 | 选择 | 代价 |
|------|------|------|
| 查找路径 | 排序 + bsearch → O(log n) 每桶 | 插入 O(n) 因 memmove |
| 写操作 | 不适合写多读少场景 | modprobe 场景以读为主，可接受 |
| 哈希函数 | SuperFastHash，质量较高 | 比 FNV 慢，但冲突更少 |
| 标准库 | 不依赖 GLib / uthash | 自己维护、有限功能集 |

## 4. 自定义 strbuf：栈初始缓冲区

`shared/strbuf.h` 提供了 `DECLARE_STRBUF_WITH_STACK(buf, PATH_MAX)`，在栈上分配初始存储，只有超出栈容量时才 fallback 到 malloc：

```c
struct strbuf {
       char *bytes;
       size_t size;
       size_t used;
       bool heap; // true → bytes 指向 malloc 区
};

#define DECLARE_STRBUF_WITH_STACK(name__, sz__) \
       char name__##_storage__[sz__];           \
       _cleanup_strbuf_ struct strbuf name__ = { \
               .bytes = name__##_storage__,     \
               .size = sz__,                    \
       }
```

使用 `_cleanup_` 属性让 `strbuf_release` 在出作用域时自动调用。

| 取舍 | 选择 | 代价 |
|------|------|------|
| 小字符串 | 零 malloc | 字符串指针可能指向栈，函数返回后失效 |
| 所有权 | `strbuf_str()` 返回的指针寿命与 buffer 绑定 | 调用者不能缓存返回的指针 |
| 类型安全 | 无，pushchars/pushmem 接受 `const char*` | 简单直接 |

## 5. 手写 ELF 解析：零外部依赖

`libkmod-elf.c` 没有使用 `libelf`，而是直接根据 ELF 规范手动解析。核心函数 `elf_get_uint` 处理字节序：

```c
static uint64_t elf_get_uint(const struct kmod_elf *elf,
                             uint64_t offset, uint16_t size)
{
       if (elf->msb) {
               memcpy((char *)&ret + sizeof(ret) - size, p, size);
               ret = be64toh(ret);
       } else {
               memcpy(&ret, p, size);
               ret = le64toh(ret);
       }
}
```

同时支持 32/64 位 ELF，通过 `elf_identify` 检测 `EI_CLASS` 和 `EI_DATA`，然后用 `x32` / `msb` 标志发散到不同代码路径。

| 取舍 | 选择 | 代价 |
|------|------|------|
| ELF 库 | 无外部依赖，节省 ~200KB | 解析器增加 ~1300 行，处理 ELF 边界情况更脆弱 |
| 32/64 位 | 共用一套 `elf_get_uint` + READV 宏 | 宏内部带着类型转换的复杂度 |
| 内存安全 | 所有偏移都经过 `elf_range_valid` + `uadd64_overflow` | 每次访问两条边界检查 |
| 覆盖率 | 只解析模块 ELF 需要的 section | 不支持 `.dynamic` 等通用 section |

## 6. 模块对象：单次分配 + 池化去重

`libkmod-module.c` 中，`kmod_module_new` 用一个 `malloc` 分配 `struct kmod_module` + 名字 + 别名 + hashkey：

```c
// name\alias\hashkey 在同一块内存中依次排列
m = malloc(sizeof(*m) + (alias == NULL ? 1 : 2) * (keylen + 1));
m->name = (char *)m + sizeof(*m);
m->hashkey = m->name + keylen + 1;
```

模块对象被加入全局哈希池 `kmod_pool_add_module`，同名的重复调用返回同一对象的引用（refcount 增加）：

```c
m = kmod_pool_get_module(ctx, key);
if (m != NULL) {
       *mod = kmod_module_ref(m);
       return 0;
}
```

初始化用位字段跟踪，避免重复解析：

```c
struct {
       bool dep : 1;
       bool options : 1;
       bool install_commands : 1;
       bool remove_commands : 1;
} init;
```

| 取舍 | 选择 | 代价 |
|------|------|------|
| 内存布局 | 一次 malloc 减少碎片 | 别名场景下内存计算复杂（三块字符串挤在一起） |
| 对象去重 | 池化保证同一模块只解析一次 | 所有模块活到 ctx 销毁，不适合长期运行 |
| 初始化 | 按需懒加载（lazy init bits） | 若访问所有字段，反而是多余的分支 |

## 7. depmod 的索引写入：与 libkmod 共享格式但独立实现

`tools/depmod.c` 自己实现了 Patricia trie 的构建和序列化，而读取端在 `libkmod/libkmod-index.c`。关键常量在两处重复定义：

```c
/* tools/depmod.c */
#define INDEX_MAGIC 0xB007F457
#define INDEX_VERSION_MAJOR 0x0002
```

```c
/* libkmod/libkmod-index.c */
#define INDEX_MAGIC 0xB007F457
#define INDEX_VERSION_MAJOR 0x0002
```

| 取舍 | 选择 | 代价 |
|------|------|------|
| 关注点分离 | depmod（写入）与 libkmod（读取）互不依赖 | 格式变更必须在两处同步，可能不同步 |
| 代码规模 | depmod.c 约 3100 行，索引构建占大半 | 功能内聚但体积大 |
| 搜索能力 | 索引支持别名通配符搜索（modules.alias 的场景） | 通配符搜索实现复杂（四级递归函数） |

## 8. 溢出安全：编译期内建 + 运行期后备

`shared/util.h` 提供了一套 `uadd*_overflow` / `umul*_overflow` 函数，优先使用编译器内建：

```c
static inline bool uadd32_overflow(uint32_t a, uint32_t b, uint32_t *res) {
#if HAVE___BUILTIN_UADD_OVERFLOW && __SIZEOF_INT__ == 4
       return __builtin_uadd_overflow(a, b, res);
#else
       *res = a + b;
       return UINT32_MAX - a < b;
#endif
}
```

并用 `_Generic` 宏聚合不同宽度：

```c
#define uaddsz_overflow(a, b, res) \
       _Generic((res), \
               uint32_t *: uadd32_overflow, \
               uint64_t *: uadd64_overflow, \
               default: simple_uaddsz_overflow)(a, b, res)
```

| 取舍 | 选择 | 代价 |
|------|------|------|
| 性能 | 有内建就用，零开销 | 后备路径在 C 标准下是 UB 行为 |
| 可移植 | 用 `_Generic` 做类型分发 | C11 特性，老编译器需回退 |

## 9. 动态符号加载 vs 直接链接

`shared/util.h` 通过 `DLSYM_LOCALLY_ENABLED` 开关在两种符号解析策略间切换：

```c
#if defined(DLSYM_LOCALLY_ENABLED) && DLSYM_LOCALLY_ENABLED
#define DECLARE_SYM(sym__) DECLARE_DLSYM(sym__);  // 运行时 dlopen
#else
#define DECLARE_SYM(sym__) DECLARE_PTRSYM(sym__); // 直接链接
#endif
```

`DECLARE_DLSYM` 声明一个函数指针初始化器，运行时通过 `dlsym_many()` 从 `.so` 中加载；
`DECLARE_PTRSYM` 直接初始化为符号地址。这在压缩后端中用于可选加载 liblzma / libz / libzstd。

| 取舍 | 选择 | 代价 |
|------|------|------|
| 启动速度 | 直接链接 → 加载器处理 | dlopen → 运行时按需解析 |
| 可用性 | dlopen → 库不存在也能启动，只是压缩不可用 | 额外的符号表维护工作 |

## 10. 测试基础设施：LD_PRELOAD 沙箱

`testsuite/testsuite.c` 使用 fork + exec + LD_PRELOAD 来拦截系统调用，模拟内核行为：

```c
static const struct {
       const char *key;
       const char *ldpreload;
} env_config[_TC_LAST] = {
       [TC_UNAME_R]    = { S_TC_UNAME_R,    OVERRIDE_LIBDIR "uname.so" },
       [TC_ROOTFS]     = { S_TC_ROOTFS,     OVERRIDE_LIBDIR "path.so" },
       [TC_INIT_MODULE_RETCODES]  = { S_TC_INIT_MODULE_RETCODES,
                                      OVERRIDE_LIBDIR "init_module.so" },
       [TC_DELETE_MODULE_RETCODES] = { S_TC_DELETE_MODULE_RETCODES,
                                       OVERRIDE_LIBDIR "delete_module.so" },
};
```

通过 epoll 并行收集子进程的标准输出和标准错误，与预期文件逐字节对比。

| 取舍 | 选择 | 代价 |
|------|------|------|
| 模拟内核 | LD_PRELOAD 拦截 syscall wrapper，无需 root | 无法测试真实内核行为 |
| 文件系统 | 预构建 rootfs tarball，测试时校验时间戳 | rootfs 变更需手动重建 |
| I/O 验证 | epoll 驱动，按行/按块对比预期输出 | 预期文件与实际输出任何差异都导致测试失败，包括空白符 |
| 结果报告 | ANSI 彩色输出 + 跳过/失败/通过计数 | 无结构化日志，难以 CI 集成 |
