---
title: kmod是怎么运行的
author: jask
series: Linux自我修养
tags:
  - Linux
date: 2026-07-05
done: 0
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
