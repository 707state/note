---
title: 干了这杯Wine再谈正事
author: jask
tags:
    - Windows
    - Linux
date: 2026-03-25
---

# Wine is not Emulator

众所周知Wine不是一个模拟器，而是模拟Windows的dll调用，那么重点就在于它是如何模拟哪些动态链接库调用的呢？

## 项目结构

Wine项目的结构并不复杂，dlls存放的是编译为PE格式的dll文件；fonts文件夹放的是字体/符号文件；programs放的是Windows自带的经典软件，比如说notepad；server位置放的是wine server，负责管理所有的Wine进程的共享状态和内核对象，某种意义上这是一个简易的Windows内核；tools放置了Wine内部使用的工具，比如说server通信用的协议就是一个DSL，通过tools/make\_requests（这是一个Perl脚本，简洁归简洁，我是一点看不懂）生成。

## 流程分析

Wine的入口在loader/main.c，这个入口通过dlopen + /proc/self/exe获取到wine的二进制路径后拼接出来dlls的路径，通过运行期加载ntdll.so获取到其中的\_\_wine\_main，这个才是真正的入口。这一函数在dlls/ntdll/unix/loader.c中，主要是初始化虚拟内存、环境变量等等，重点是启动主线程。

```c++
static void start_main_thread(void)
{
    TEB *teb = virtual_alloc_first_teb(); // 分配第一个TEB
    signal_init_threading();
    dbg_init();
    startup_info_size = server_init_process(); // 连接winserver
    virtual_map_user_shared_data();
    init_cpu_info();
    init_files();
    init_startup_info();
    *(ULONG_PTR *)&peb->CloudFileFlags = get_image_address();
    set_load_order_app_name( main_wargv[0] );
    init_thread_stack( teb, 0, 0, 0 );
    NtCreateKeyedEvent( &keyed_event, GENERIC_READ | GENERIC_WRITE, NULL, 0 );
    load_ntdll(); // 加载PE格式的ntdll.dll，这一步很关键
    load_wow64_ntdll( main_image_info.Machine );
    load_apiset_dll();
    server_init_process_done(); //通知winserver初始化完成
}
```

下一步就是load\_ntdll，这个函数整体上只是一个桥接，真正重要的地方时load\_ntdll\_functions。

```c
static void load_ntdll_functions( HMODULE module )
{
    void **p__wine_syscall_dispatcher;
    void **p__wine_unix_call_dispatcher;
    void **p__wine_unix_call_dispatcher_arm64ec = NULL;
    unixlib_handle_t *p__wine_unixlib_handle;
    const IMAGE_EXPORT_DIRECTORY *exports;

    exports = get_module_data_dir( module, IMAGE_DIRECTORY_ENTRY_EXPORT, NULL );
    assert( exports );

#define GET_FUNC(name) \
    if (!(p##name = (void *)find_named_export( module, exports, #name ))) \
        ERR( "%s not found\n", #name )

    GET_FUNC( DbgUiRemoteBreakin );
    GET_FUNC( KiRaiseUserExceptionDispatcher );
    GET_FUNC( KiUserExceptionDispatcher );
    GET_FUNC( KiUserApcDispatcher );
    GET_FUNC( KiUserCallbackDispatcher );
    GET_FUNC( LdrInitializeThunk );
    GET_FUNC( LdrSystemDllInitBlock );
    GET_FUNC( RtlUserThreadStart );
    GET_FUNC( __wine_ctrl_routine );
	// 从导出表中获取到__wine_syscall_dispatcher的地址
    GET_FUNC( __wine_syscall_dispatcher );
    GET_FUNC( __wine_unix_call_dispatcher );
    GET_FUNC( __wine_unixlib_handle );
    if (is_arm64ec())
    {
        GET_FUNC( __wine_unix_call_dispatcher_arm64ec );
        GET_FUNC( KiUserEmulationDispatcher );
    }
	// 把Unix端的dispatcher函数地址写入PE模块，这个赋值并不好理解
    *p__wine_syscall_dispatcher = __wine_syscall_dispatcher;
    *p__wine_unixlib_handle = (UINT_PTR)unix_call_funcs;
    if (p__wine_unix_call_dispatcher_arm64ec)
    {
        /* redirect __wine_unix_call_dispatcher to __wine_unix_call_dispatcher_arm64ec */
        *p__wine_unix_call_dispatcher = *p__wine_unix_call_dispatcher_arm64ec;
        *p__wine_unix_call_dispatcher_arm64ec = __wine_unix_call_dispatcher;
    }
    else *p__wine_unix_call_dispatcher = __wine_unix_call_dispatcher;
#undef GET_FUNC
}
```

系统调用表的构建并非C代码实现，而是由tools/make\_specfiles codegen出来的，会存放在dlls/ntdll/ntsyscalls.h中。

构建系统调用表的位置在dlls/ntdll/unix/syscall.c中：
```c
// 函数指针数组
static void * const syscalls[] =
{
#define SYSCALL_ENTRY(id,name,args) name,
    ALL_SYSCALLS
#undef SYSCALL_ENTRY
};
// 参数大小数组
static BYTE syscall_args[ARRAY_SIZE(syscalls)] =
{
#define SYSCALL_ENTRY(id,name,args) args,
    ALL_SYSCALLS
#undef SYSCALL_ENTRY
};
// Windows风格的服务描述符表
SYSTEM_SERVICE_TABLE KeServiceDescriptorTable[4] =
{
    { (ULONG_PTR *)syscalls, NULL, ARRAY_SIZE(syscalls), syscall_args }
};

```

在线程初始化时这个表被设置到TEB中：

```c
static void start_thread( TEB *teb )
{
    struct ntdll_thread_data *thread_data = (struct ntdll_thread_data *)&teb->GdiTebBatch;
    BOOL suspend;

    thread_data->syscall_table = KeServiceDescriptorTable;
    thread_data->syscall_trace = TRACE_ON(syscall);
    thread_data->pthread_id = pthread_self();
    pthread_setspecific( teb_key, teb );
    server_init_thread( thread_data->start, &suspend );
    signal_start_thread( thread_data->start, thread_data->param, suspend, teb );
}
```



### 系统调用翻译

每个Nt开头的函数都有一个Codegen出来的汇编存根，实际实现在dlls/ntdll/ntdll.spec中定义。

Unix端主要是一大堆的汇编，定义在dlls/ntdll/unix/signal\_x86\_64.c中，可以看到:

```c
__ASM_GLOBAL_FUNC( __wine_syscall_dispatcher,
                   __ASM_LOCAL_LABEL("__wine_syscall_dispatcher_gs_load") ":\n\t"
                   "movq %gs:0x378,%rcx\n\t"       /* thread_data->syscall_frame */
                   "popq 0x70(%rcx)\n\t"           /* frame->rip */
                   __ASM_CFI(".cfi_adjust_cfa_offset -8\n\t")
                   __ASM_CFI_REG_IS_AT2(rip, rcx, 0xf0,0x00)
                   "pushfq\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset 8\n\t")
                   "popq 0x80(%rcx)\n\t"
                   __ASM_CFI(".cfi_adjust_cfa_offset -8\n\t")
                   "movl $0,0xb4(%rcx)\n\t"        /* frame->restore_flags */
                   __ASM_LOCAL_LABEL("__wine_syscall_dispatcher_prolog_end") ":\n\t"
                   "movq %rbx,0x08(%rcx)\n\t"
				   ....
```

我看不懂，不熟悉X86的Calling Convention还有Windows PE格式，可以先当作一个黑盒理解。

举个例子，以CreateFileW为例，大致流程就变成了：

1. 一个exe文件调用了CreateFileW("text.txt",...)；
2. kernel32.dll含有这个函数，处理了参数转换之后就调用NtCreateFile；
3. ntdll.dll通过系统调用存根，进行jmp把系统调用分派到Unix端；
4. \_\_wine\_syscall\_dispatcher保存所有寄存器到syscall\_frame，切换到内核栈，从KeServiceDescriptorTable获取函数指针，然后就调用Unix侧的NtCreateFile实现。
5. dlls/ntdll/unix/file.c NtCreateFile，这就是Unix侧的实现，转换为了一个open调用。
6. 如果要和wine server通信的话，还涉及进行请求。Wine server处理请求，创建文件对象，分配句柄并返回，最后通过\_\_wine\_syscall\_dispatcher\_return返回，这里涉及到切换回用户栈、恢复寄存器等等操作。
7. ntdll.dll返回，返回值会放在RAX寄存器。
8. kernel32.dll转换返回值，把NTSTATUS转换为HANDLE。

### 系统调用存根

这是一套解析.spec文件构建工具，在tools/winebuild/下，负责把.spec文件转换为汇编文件和目标文件。

工作流程就是：winebuild -m64 --syscall-table -o ntdll.spec.o ntdll.spec

对于系统调用就不生成存根，因为那些是在Unix端实现的，通过dispatcher调用。

PE端的导出表有NtClose等等符号，但是这些没有具体实现，Unix端的syscalls[]数组包含了NtClose的实现，这两个是运行期动态绑定的，也就是说工作流程就是：应用程序查找导出表，跳转到对应地址，这个地址实际上是dispatcher，然后dispatcher通过EAX寄存器肚脐系统调用号，通过syscalls查表去调用Unix端的实现。

#### Windows知识

在我上了大学之后，大多数时候我都在用Linux，Windows了解的还是比较少，现在马上毕业了才发觉Windows的好来。MS Windows不完美，有各种问题，但是Windows作为操作系统本身的设计非常优秀，比如说完全异步的IO操作（IOCP）、稳定的API（ntdll.dll/kernel32.dll，强迫用户动态链接，内核可以随意修改但是暴露出去的符号永远稳定）、细粒度的ACL（比起Linux的POSIX ACL、SELinux等方案更早更统一）。

1. TEB

Thread Environment Block是Win32 (x86) 存储当前正在运行的线程信息的数据结构，可以通过TEB在不调用Win32 API的情况下获得Import Tables、启动参数或者文件名。

2. PE格式

就像很多别的二进制可执行文件格式一样，PE格式需要一个指导dynamic linker加载的Layout、用于记录程序运行时需要链接的函数的导入表以及ASLR用于随机化地址空间布局。调试上有一些区别，主要是PE格式有一个额外的PDB文件，而ELF格式则是通过DWARF。

### 加载PE

主要代码在dlls/ntdll/unix/virtual.c中，重点逻辑在于：

```c
/***********************************************************************
 *           map_image_into_view
 *
 * Map an executable (PE format) image into an existing view.
 * virtual_mutex must be held by caller.
 */
static NTSTATUS map_image_into_view( struct file_view *view, const UNICODE_STRING *nt_name, int fd,
                                     struct pe_image_info *image_info, USHORT machine,
                                     int shared_fd, BOOL removable )
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sections = NULL, *sec;
    IMAGE_DATA_DIRECTORY *imports, *dir;
    NTSTATUS status = STATUS_CONFLICTING_ADDRESSES;
    int i;
    off_t pos;
    struct stat st;
    char *header_end;
    char *ptr = view->base;
    SIZE_T header_size, header_map_size, total_size = view->size;
    SIZE_T align_mask = max( image_info->alignment - 1, page_mask );
    INT_PTR delta;
    /* 映射PE头部到内存 */
    fstat( fd, &st );
    header_size = min( image_info->header_size, st.st_size );
    header_map_size = min( image_info->header_map_size, ROUND_SIZE( 0, st.st_size, host_page_mask ));
    if ((status = map_pe_header( view->base, header_size, header_map_size, fd, &removable )))
        return status;
    status = STATUS_INVALID_IMAGE_FORMAT;  /* generic error */
	// 读取DOS头和NT头
    dos = (IMAGE_DOS_HEADER *)ptr;
    nt = (IMAGE_NT_HEADERS *)(ptr + dos->e_lfanew);
    header_end = ptr + ROUND_SIZE( 0, header_size, align_mask );
    memset( ptr + header_size, 0, header_end - (ptr + header_size) );
    if ((char *)(nt + 1) > header_end) return status;
    sec = IMAGE_FIRST_SECTION( nt );
    if ((char *)(sec + nt->FileHeader.NumberOfSections) > header_end) return status;
    if ((char *)(sec + nt->FileHeader.NumberOfSections) > ptr + image_info->header_map_size)
    {
        /* copy section data since it will get overwritten by a section mapping */
        if (!(sections = malloc( sizeof(*sections) * nt->FileHeader.NumberOfSections )))
            return STATUS_NO_MEMORY;
        memcpy( sections, sec, sizeof(*sections) * nt->FileHeader.NumberOfSections );
        sec = sections;
    }
	// 获取导入表目录
    imports = get_data_dir( nt, total_size, IMAGE_DIRECTORY_ENTRY_IMPORT );
    /* check for non page-aligned binary */
    if (image_info->image_flags & IMAGE_FLAGS_ImageMappedFlat)
    {
        /* unaligned sections, this happens for native subsystem binaries */
        /* in that case Windows simply maps in the whole file */

        total_size = min( total_size, ROUND_SIZE( 0, st.st_size, page_mask ));
        if (map_file_into_view( view, fd, 0, total_size, 0, VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY,
                                removable ) != STATUS_SUCCESS) goto done;
        /* check that all sections are loaded at the right offset */
        if (nt->OptionalHeader.FileAlignment != nt->OptionalHeader.SectionAlignment) goto done;
        for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
        {
            if (sec[i].VirtualAddress != sec[i].PointerToRawData)
                goto done;  /* Windows refuses to load in that case too */
        }
        /* set the image protections */
        set_vprot( view, ptr, total_size, VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY | VPROT_EXEC );
        /* no relocations are performed on non page-aligned binaries */
        status = STATUS_SUCCESS;
        goto done;
    }
    /* map all the sections */
    for (i = pos = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
	...
	}
#ifdef __aarch64__
    if ((dir = get_data_dir( nt, total_size, IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG )))
    {
        if (image_info->machine == IMAGE_FILE_MACHINE_ARM64 &&
            (machine == IMAGE_FILE_MACHINE_AMD64 ||
             (!machine && main_image_info.Machine == IMAGE_FILE_MACHINE_AMD64)))
        {
            update_arm64x_mapping( view, nt, dir, sec );
            /* reload changed machine from NT header */
            image_info->machine = nt->FileHeader.Machine;
        }
        if (image_info->machine == IMAGE_FILE_MACHINE_AMD64)
            update_arm64ec_ranges( view, nt, dir, &image_info->entry_point );
    }
#endif
    if (machine && machine != nt->FileHeader.Machine)
    {
        status = STATUS_NOT_SUPPORTED;
        goto done;
    }
    /* relocate to dynamic base */
    if (image_info->map_addr && (delta = image_info->map_addr - image_info->base))
    {
        if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            ((IMAGE_NT_HEADERS64 *)nt)->OptionalHeader.ImageBase = image_info->map_addr;
        else
            ((IMAGE_NT_HEADERS32 *)nt)->OptionalHeader.ImageBase = image_info->map_addr;
        if ((dir = get_data_dir( nt, total_size, IMAGE_DIRECTORY_ENTRY_BASERELOC )))
        {
            IMAGE_BASE_RELOCATION *rel = (IMAGE_BASE_RELOCATION *)(ptr + dir->VirtualAddress);
            IMAGE_BASE_RELOCATION *end = (IMAGE_BASE_RELOCATION *)((char *)rel + dir->Size);
            while (rel && rel < end - 1 && rel->SizeOfBlock && rel->VirtualAddress < total_size)
                rel = process_relocation_block( ptr + rel->VirtualAddress, rel, delta );
        }
    }
    /* set the image protections */
    set_vprot( view, ptr, ROUND_SIZE( 0, header_size, align_mask ), VPROT_COMMITTED | VPROT_READ );
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        SIZE_T size;
        BYTE vprot = VPROT_COMMITTED;
        if (sec[i].Misc.VirtualSize)
            size = ROUND_SIZE( sec[i].VirtualAddress, sec[i].Misc.VirtualSize, align_mask );
        else
            size = ROUND_SIZE( sec[i].VirtualAddress, sec[i].SizeOfRawData, align_mask );
        if (sec[i].Characteristics & IMAGE_SCN_MEM_READ)    vprot |= VPROT_READ;
        if (sec[i].Characteristics & IMAGE_SCN_MEM_WRITE)   vprot |= VPROT_WRITECOPY;
        if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) vprot |= VPROT_EXEC;
        if (!set_vprot( view, ptr + sec[i].VirtualAddress, size, vprot ) && (vprot & VPROT_EXEC))
            ERR( "failed to set %08x protection on %s section %.8s, noexec filesystem?\n",
                 sec[i].Characteristics, debugstr_us(nt_name), sec[i].Name );
    }
#ifdef VALGRIND_LOAD_PDB_DEBUGINFO
    VALGRIND_LOAD_PDB_DEBUGINFO(fd, ptr, total_size, ptr - (char *)wine_server_get_ptr( image_info->base ));
#endif
    status = STATUS_SUCCESS;
done:
    free( sections );
    return status;
}
```

for循环中处理的是所有PE节区，

### Wine Server

wineserver与客户端进程之间通过Unix Domain Socket进行交互，重点代码在dlls/ntdll/unix/server.c。

