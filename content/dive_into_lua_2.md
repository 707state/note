---
title: 深入理解Lua（与虚拟机）2
author: jask
tags:
  - Lua
  - ProgrammingLanguages
date: 2026-03-28
series: 深入理解lua
---

# Rust the Lua

Lua本身采用GNU Make构建，但是非常容易使用meson或者其他构建系统重构，那么这里就用Cargo + cc crate，并把Rust集成进来。

```meson
rust_ffi_lib = static_library(
  'rust_ffi_core',
  sources: 'rust_ffi/src/lib.rs',
  rust_abi: 'c',
)

rust_ffi_module = shared_module(
  'rust_ffi',
  sources: 'rust_ffi/module.c',
  include_directories: include_directories('src/'),
  link_with: [lua_lib, rust_ffi_lib],
  name_prefix: '',
)

lua_exe = executable(
  'lua',
  sources: ['src/lua.c'],
  link_with: [lua_lib, rust_ffi_lib],
  dependencies: [raylib_dep],
)
```

```rust
use std::env;
use std::path::PathBuf;

fn main() {
    let out_dir = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR is not set"));

    let common_sources = [
        "src/lapi.c",
        "src/lauxlib.c",
        "src/lbaselib.c",
        "src/lcode.c",
        "src/lcorolib.c",
        "src/lctype.c",
        "src/ldblib.c",
        "src/ldebug.c",
        "src/ldo.c",
        "src/ldump.c",
        "src/lfunc.c",
        "src/lgc.c",
        "src/linit.c",
        "src/liolib.c",
        "src/llex.c",
        "src/lmem.c",
        "src/loadlib.c",
        "src/lobject.c",
        "src/lopcodes.c",
        "src/loslib.c",
        "src/lparser.c",
        "src/lstate.c",
        "src/lstring.c",
        "src/lstrlib.c",
        "src/ltable.c",
        "src/ltablib.c",
        "src/ltm.c",
        "src/lundump.c",
        "src/lutf8lib.c",
        "src/lvm.c",
        "src/lzio.c",
    ];

    let mut lua_core = cc::Build::new();
    lua_core
        .include("src")
        .warnings(true)
        .define("LUA_USE_DLOPEN", None)
        .define("LUA_USE_POSIX", None);

    for source in common_sources {
        lua_core.file(source);
    }
    lua_core.compile("lua_core");

    build_cli("src/lua.c", "lua_cli_main", "lua_cli");
    build_cli("src/luac.c", "luac_cli_main", "luac_cli");

    println!("cargo:rustc-link-search=native={}", out_dir.display());
    println!("cargo:rustc-link-lib=static=lua_core");

    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    match target_os.as_str() {
        "linux" | "freebsd" | "dragonfly" | "netbsd" | "openbsd" => {
            println!("cargo:rustc-link-lib=dylib=dl");
            println!("cargo:rustc-link-lib=dylib=m");
        }
        "android" => {
            println!("cargo:rustc-link-lib=dylib=dl");
            println!("cargo:rustc-link-lib=dylib=m");
            println!("cargo:rustc-link-lib=dylib=log");
        }
        "macos" => {
            println!("cargo:rustc-link-lib=framework=CoreFoundation");
            println!("cargo:rustc-link-lib=framework=Cocoa");
            println!("cargo:rustc-link-lib=dylib=m");
        }
        _ => {}
    }
}

fn build_cli(source: &str, renamed_main: &str, output: &str) {
    let mut build = cc::Build::new();
    build
        .include("src")
        .warnings(true)
        .define("LUA_USE_DLOPEN", None)
        .define("LUA_USE_POSIX", None)
        .define("main", Some(renamed_main))
        .file(source)
        .compile(output);
}
```

等到meson 1.11之后还可以直接用Cargo集成。

先把Lua中的math模块切换为Rust并把lua源码的动态链接库加载逻辑魔改一下吧。

## 动态链接库

Lua的动态链接库加载逻辑非常难绷，package.cpath惯常做法是：

```lua
package.cpath="path/to/dylib/?.so;"..package.cpath
```

这非常扯，为什么一个跨平台的语言需要在加载动态链接库时指定后缀？那其实完全可以改改loadlib.c，在编译的时候根据操作系统自动设定后缀，就可以做到这样的效果：

```lua
package.cpath = "./build/?;" .. package.cpath
local rust = require("rust_ffi")
local simd = rust.simd
```

直接用路径+require就能够去加载对应的dll/so/dylib，可不必代码中写方便多了吗？

其实很好实现：

```c
// loadlib.c:517
static const char *searchpath (lua_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep,
											 // 新增一个suffix表示后缀
                                             const char *suffix) {
...
  while ((filename = getnextfilename(&pathname, endpathname)) != NULL) {
    const char *cand = filename;
	// 如果后缀不空而且filename没写后缀
    if (suffix != NULL && !hasextension(filename))
      cand = lua_pushfstring(L, "%s%s", filename, suffix);
    if (readable(cand))  /* does file exist and is readable? */
      return ((cand == filename) ? lua_pushstring(L, filename) : cand);
    if (cand != filename)
      lua_pop(L, 1);  /* pop temporary suffixed candidate */
  }
...
}
// 这个是直接暴露给LUA的，是package.searchpath参数
static int ll_searchpath (lua_State *L) {
  const char *f = searchpath(L, luaL_checkstring(L, 1),
                                luaL_checkstring(L, 2),
                                luaL_optstring(L, 3, "."),
                                luaL_optstring(L, 4, LUA_DIRSEP),
								// 相比原版多一个参数
                                luaL_optstring(L, 5, NULL));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    luaL_pushfail(L);
    lua_insert(L, -2);
    return 2;  /* return fail + error message */
  }
}
static const char *findfile (lua_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  const char *suffix = NULL;
  lua_getfield(L, lua_upvalueindex(1), pname);
  path = lua_tostring(L, -1);
  if (l_unlikely(path == NULL))
    luaL_error(L, "'package.%s' must be a string", pname);
  // 条件编译会设置LUA_CMOD_SUFFIX，传递给searchpath
  if (strcmp(pname, "cpath") == 0)
    suffix = LUA_CMOD_SUFFIX;
  return searchpath(L, name, path, ".", dirsep, suffix);
}

```

package.loadlib的入口行为：ll\_loadlib中先获取路径最后一个组建（主要是Windows的\\），如果basename没有'.'的话就认为没写拓展名，就拼接为path + LUA\_CMOD\_SUFFIX然后交给lookforfunc走正常流程继续。

require：这是修改了C模块的路径解析，遍历package.cpath的j候选路径，如果没有拓展名且是在查cpath就拼接filename + LUA\_CMOD\_SUFFIX，验证readable检查然后返回真实可加载的文件名。

## Rust math lib

解决了一个我看不顺眼的地方之后，就可以用完全不需要处理跨平台逻辑的Lua了，开始用Rust去重写Lua的模块。

首先第一步是重构构建系统！前面说了用Cargo取代了原本的构建逻辑之后，现在我可以使用cargo build直接编译。接下来我要一步步干掉原本的C module,比如说math、io、os、string、utf8这些内置模块，全部Rust化。

首先是最独立的math模块。

```rust
#[repr(C)]
pub struct lua_State {
    _private: [u8; 0],
}
...
#[unsafe(no_mangle)]
pub unsafe extern "C" fn luaopen_math(state: *mut lua_State) -> c_int {
    unsafe { create_library(state, &MATHLIB_REGS) };
    unsafe { lua_pushnumber(state, PI) };
    unsafe { lua_setfield(state, -2, FIELD_PI.as_ptr().cast()) };
    unsafe { lua_pushnumber(state, lua_Number::INFINITY) };
    unsafe { lua_setfield(state, -2, FIELD_HUGE.as_ptr().cast()) };
    unsafe { lua_pushinteger(state, LUA_MAXINTEGER) };
    unsafe { lua_setfield(state, -2, FIELD_MAXINTEGER.as_ptr().cast()) };
    unsafe { lua_pushinteger(state, LUA_MININTEGER) };
    unsafe { lua_setfield(state, -2, FIELD_MININTEGER.as_ptr().cast()) };
    unsafe { setrandfunc(state) };
    1
}
```

如果只是单纯的用Rust写lua module，有很多现成的crate可用，但是既然这里是想要魔改，就用一些比较复杂的手段。首先就是需要在Rust测重复写大量的struct去和C兼容了，这一步纯纯是体力活，所以AI给我做；然后是需要很多C的符号暴露给Rust，就像这样：

```rust
unsafe extern "C" {
    fn luaL_checkversion_(state: *mut lua_State, version: lua_Number, sizes: usize);
    fn luaL_checknumber(state: *mut lua_State, arg: c_int) -> lua_Number;
    fn luaL_optnumber(state: *mut lua_State, arg: c_int, def: lua_Number) -> lua_Number;
...
}
```

同样也是力气活。

有趣的地方在哪里呢？请看这里：

```rust
unsafe extern "C" fn rust_add(state: *mut lua_State) -> c_int {
    let lhs = unsafe { luaL_checknumber(state, 1) };
    let rhs = unsafe { luaL_checknumber(state, 2) };
    unsafe { lua_pushnumber(state, lhs + rhs) };
    1
}
```

这是Rust版本的add实现，这里面全部都是当作f64处理的，可以看到这本质上还是在操作虚拟机的栈，从两个寄存器读出来参数然后压栈罢了，和C实现没有什么区别。这里完全看不出来Rust的优势，全部都是unsafe代码，但是重点在于可以充分利用上Rust了。有趣之处在于，仅通过C abi是完全无法让我获得理想的binding的，所以这里必须使用更深程度的修改，以便于我编写代码。

## Rust the vm

Lua VM是一个寄存器虚拟机，主要代码都在lvm.c中，这部分代码的逻辑比较复杂。

### bridge

为了用Rust取代原本的luac.c，需要添加一些代码，让Rust与lua vm进行交互。

```rust
const Proto *rust_luavm_top_proto(lua_State *L) {
  return getproto(s2v(L->top.p - 1));
}
int rust_luavm_getfuncline(const Proto *f, int pc) {
  return luaG_getfuncline(f, pc);
}
const TString *rust_luavm_eventname(lua_State *L, int idx) {
  return G(L)->tmname[idx];
}
```

通过这三个方法暴露给Rust，就可以在Rust中实现print\_code这样的方法，这就可以用Rust把luac中最难实现的-l（打印Lua Bytecode）的方法实现出来。

```rust
unsafe fn print_code(state: *mut lua_State, proto: *const Proto) {
    let sizecode = unsafe { (*proto).sizecode };
    for pc in 0..sizecode {
        let instruction = unsafe { *(*proto).code.add(pc as usize) };
        let opcode = get_opcode(instruction);
        let a = getarg_a(instruction);
        let b = getarg_b(instruction);
        let c = getarg_c(instruction);
        let ax = getarg_ax(instruction);
        let bx = getarg_bx(instruction);
        let sb = getarg_sb(instruction);
        let sc = getarg_sc(instruction);
        let vb = getarg_vb(instruction);
        let vc = getarg_vc(instruction);
        let sbx = getarg_sbx(instruction);
        let isk = getarg_k(instruction);
        let line = unsafe { rust_luavm_getfuncline(proto, pc) };
        print!("\t{}\t", pc + 1);
        if line > 0 {
            print!("[{}]\t", line);
        } else {
            print!("[-]\t");
        }
        print!("{:<9}\t", OPNAMES[opcode as usize]);
        match opcode {
            0 => print!("{a} {b}"),
            1 | 2 => print!("{a} {sbx}"),
            3 => {
                print!("{a} {bx}");
                print!("{COMMENT}");
                print_constant(proto, bx);
            }
            4 => {
                print!("{a}");
                print!("{COMMENT}");
                print_constant(proto, extraarg(proto, pc));
            }
            5..=7 => print!("{a}"),
            8 => {
                print!("{a} {b}");
                print!("{COMMENT}{} out", b + 1);
            }
            9 | 10 => {
                print!("{a} {b}");
                print!("{COMMENT}{}", upval_name(proto, b));
            }
            11 => {
                print!("{a} {b} {c}");
                print!("{COMMENT}{}", upval_name(proto, b));
                print!(" ");
                print_constant(proto, c);
            }
            12 | 13 => print!("{a} {b} {c}"),
            14 => {
                print!("{a} {b} {c}");
                print!("{COMMENT}");
                print_constant(proto, c);
            }
            15 => {
                print!("{a} {b} {c}{}", if isk != 0 { "k" } else { "" });
                print!("{COMMENT}{}", upval_name(proto, a));
                print!(" ");
                print_constant(proto, b);
                if isk != 0 {
                    print!(" ");
                    print_constant(proto, c);
                }
            }
            16 | 17 => {
                print!("{a} {b} {c}{}", if isk != 0 { "k" } else { "" });
                if isk != 0 {
                    print!("{COMMENT}");
                    print_constant(proto, c);
                }
            }
            18 => {
                print!("{a} {b} {c}{}", if isk != 0 { "k" } else { "" });
                print!("{COMMENT}");
                print_constant(proto, b);
                if isk != 0 {
                    print!(" ");
                    print_constant(proto, c);
                }
            }
            19 => {
                print!("{a} {vb} {vc}{}", if isk != 0 { "k" } else { "" });
                print!("{COMMENT}{}", vc + extraargc(proto, pc));
            }
            20 => {
                print!("{a} {b} {c}{}", if isk != 0 { "k" } else { "" });
                if isk != 0 {
                    print!("{COMMENT}");
                    print_constant(proto, c);
                }
            }
            21 | 32 | 33 => print!("{a} {b} {sc}"),
            22..=31 => {
                print!("{a} {b} {c}");
                print!("{COMMENT}");
                print_constant(proto, c);
            }
            34..=45 => print!("{a} {b} {c}"),
            46 => {
                print!("{a} {b} {c}");
                print!("{COMMENT}{}", event_name(state, c));
            }
            47 => {
                print!("{a} {sb} {c} {isk}");
                print!("{COMMENT}{}", event_name(state, c));
                if isk != 0 {
                    print!(" flip");
                }
            }
            48 => {
                print!("{a} {b} {c} {isk}");
                print!("{COMMENT}{} ", event_name(state, c));
                print_constant(proto, b);
                if isk != 0 {
                    print!(" flip");
                }
            }
            49..=53 => print!("{a} {b}"),
            54 | 55 => print!("{a}"),
            56 => {
                let sj = getarg_sj(instruction);
                print!("{sj}");
                print!("{COMMENT}to {}", sj + pc + 2);
            }
            57..=59 => print!("{a} {b} {isk}"),
            60 => {
                print!("{a} {b} {isk}");
                print!("{COMMENT}");
                print_constant(proto, b);
            }
            61..=65 => print!("{a} {sb} {isk}"),
            66 => print!("{a} {isk}"),
            67 => print!("{a} {b} {isk}"),
            68 => {
                print!("{a} {b} {c}");
                print!("{COMMENT}");
                if b == 0 {
                    print!("all in ");
                } else {
                    print!("{} in ", b - 1);
                }
                if c == 0 {
                    print!("all out");
                } else {
                    print!("{} out", c - 1);
                }
            }
            69 => {
                print!("{a} {b} {c}{}", if isk != 0 { "k" } else { "" });
                print!("{COMMENT}{} in", b - 1);
            }
            70 => {
                print!("{a} {b} {c}{}", if isk != 0 { "k" } else { "" });
                print!("{COMMENT}");
                if b == 0 {
                    print!("all out");
                } else {
                    print!("{} out", b - 1);
                }
            }
            71 => {}
            72 => print!("{a}"),
            73 | 77 => {
                print!("{a} {bx}");
                print!("{COMMENT}to {}", pc - bx + 2);
            }
            74 => {
                print!("{a} {bx}");
                print!("{COMMENT}exit to {}", pc + bx + 3);
            }
            75 => {
                print!("{a} {bx}");
                print!("{COMMENT}to {}", pc + bx + 2);
            }
            76 => print!("{a} {c}"),
            78 => {
                print!("{a} {vb} {vc}{}", if isk != 0 { "k" } else { "" });
                if isk != 0 {
                    print!("{COMMENT}{}", c + extraargc(proto, pc));
                }
            }
            79 => {
                print!("{a} {bx}");
                print!("{COMMENT}{:p}", unsafe { *(*proto).p.add(bx as usize) });
            }
            80 => {
                print!("{a} {b} {c}{}", if isk != 0 { "k" } else { "" });
                print!("{COMMENT}");
                if c == 0 {
                    print!("all out");
                } else {
                    print!("{} out", c - 1);
                }
            }
            81 => print!("{a} {b} {c}"),
            82 => {
                print!("{a} {bx}");
                print!("{COMMENT}");
                if bx == 0 {
                    print!("?");
                } else {
                    print_constant(proto, bx - 1);
                }
            }
            83 => print!("{a}"),
            84 => print!("{ax}"),
            _ => {}
        }
        println!();
    }
}
```

这只是第一步，现在只是通过暴露三个方法使得Rust可以直接和Lua VM进行交互，但还不算是Rust化Lua VM实现，下一步就是想办法把Lua VM本身变得锈迹斑斑。

### Lua VM internal

在魔改VM部分的实现之前，需要搞一下lzio/ldump/lundump这些基本的功用，要不然就很难利用上Rust的安全性了。这部分完全交给AI处理，比如说：

```rust
#[repr(C)]
pub struct ZIO {
    pub n: usize,
    pub p: *const c_char,
    pub reader: LuaReader,
    pub data: *mut c_void,
    pub l: *mut lua_State,
}
#[unsafe(no_mangle)]
pub unsafe extern "C" fn luaZ_read(z: *mut ZIO, buffer: *mut c_void, mut n: usize) -> usize {
    let z = ZIO::as_mut(z);
    let mut out = buffer.cast::<u8>();
    while n != 0 {
        if !checkbuffer(z) {
            return n;
        }
        let chunk_len = z.n.min(n);
        let src = unsafe { z.readable_bytes(chunk_len) };
        let dst = unsafe { slice::from_raw_parts_mut(out, chunk_len) };
        dst.copy_from_slice(src);
        unsafe { z.advance(chunk_len) };
        out = unsafe { out.add(chunk_len) };
        n -= chunk_len;
    }
    0
}
```

这种完全可以用Rust重写而不必担心，dump/undump本身只是导出/加载字节码的工具，并不涉及到Lua VM最复杂的能力。

经过一番折腾，现在已经收敛到gc/vm/parser/lexer/code/api/string/table这几个地方无法使用Rust了。

实际上，真正复杂的部分是gc、code、vm以及parser。这三个部分耦合且复杂，非常难以直接重写。


#### 为什么难重写

按理来讲lexer/parser不该这么难替换的，因为现代语言往往采用的是：Lexer->Parser->Codegen这样的路径，前一个执行完了才往后继续。但是在上一期我讲过，Lua的Parser/Lexer完全不解耦合，而是Lexer解析第一个Token之后，提给Parser，然后parsing期间继续读token。这是种早期编译器设计，但是这对我就造成了非常大的困扰。Lexer即便迁移到Rust，也需要和Parser进行互操作，而且Parser还和Lua VM之间有很多依赖，因为Lua支持eval。

好吧，还是先分析代码。

```c
LUAI_FUNC lu_byte luaY_nvarstack (FuncState *fs);
LUAI_FUNC void luaY_checklimit (FuncState *fs, int v, int l,
                                const char *what);
LUAI_FUNC LClosure *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                                 Dyndata *dyd, const char *name, int firstchar);
```

这是Parser向外暴露的三个符号，而这三个符号是需要被ldo.c和lcode.c进行调用的，比如说ldo.c：

```c
/*
** Free register 'reg', if it is neither a constant index nor
** a local variable.
)
*/
static void freereg (FuncState *fs, int reg) {
  if (reg >= luaY_nvarstack(fs)) {
    fs->freereg--;
    lua_assert(reg == fs->freereg);
  }
}
```

Parser需要维护给定函数中的寄存器栈中变量数量，单这一点就非常难重写。

_那么能不能换一种思路呢？_

既然一比一迁移非常困难，那就不要重写，而是重新实现一份。

#### 整体结构

目前需要实现的部分是：

1. Parser
2. ldo.c部分，主要实现Lua VM中的栈、调用、错误处理以及协程切换，可以理解成汇编中对于入栈/出栈的模拟。
3. gc。Lua实现的垃圾回收算法。
4. lcode.c。实现了Codegen。

很显然这四个没有一个是省油的灯，先看ldo.c吧。

#### ldo.c

在当前的实现中，ldo.c主要承担了luaD\_call、lua\_resume等功能，对于调用分派、结束调用、错误处理、保护执行、栈指针、协程恢复/yield边界都是在这里实现的。

```c

/*
** Reallocate the stack to a new size, correcting all pointers into it.
** In case of allocation error, raise an error or return false according
** to 'raiseerror'.
*/
int luaD_reallocstack (lua_State *L, int newsize, int raiseerror) {
  int oldsize = stacksize(L);
  int i;
  StkId newstack;
  StkId oldstack = L->stack.p;
  lu_byte oldgcstop = G(L)->gcstopem;
  lua_assert(newsize <= MAXSTACK || newsize == ERRORSTACKSIZE);
  relstack(L);  /* change pointers to offsets */
  G(L)->gcstopem = 1;  /* stop emergency collection */
  newstack = luaM_reallocvector(L, oldstack, oldsize + EXTRA_STACK,
                                   newsize + EXTRA_STACK, StackValue);
  G(L)->gcstopem = oldgcstop;  /* restore emergency collection */
  if (l_unlikely(newstack == NULL)) {  /* reallocation failed? */
    correctstack(L, oldstack);  /* change offsets back to pointers */
    if (raiseerror)
      luaM_error(L);
    else return 0;  /* do not raise an error */
  }
  L->stack.p = newstack;
  correctstack(L, oldstack);  /* change offsets back to pointers */
  L->stack_last.p = L->stack.p + newsize;
  for (i = oldsize + EXTRA_STACK; i < newsize + EXTRA_STACK; i++)
    setnilvalue(s2v(newstack + i)); /* erase new segment */
  return 1;
}
```

以这个重新分配栈大小的函数为例，将其迁移到Rust中。

```rust
pub unsafe extern "C" fn luaD_reallocstack(
    L: *mut lua_State,
    newsize: c_int,
    raiseerror: c_int,
) -> c_int {
	    let oldsize = unsafe { stacksize(L) };
    let oldstack = unsafe { (*L).stack.p };
    let oldgcstop = unsafe { (*G(L)).gcstopem };
    unsafe { api_check(newsize <= MAXSTACK || newsize == ERRORSTACKSIZE, "invalid stack size") };
    unsafe { relstack(L) };
    unsafe { (*G(L)).gcstopem = 1 };
    let newstack = unsafe {
        luaM_realloc_(
            L,
            oldstack.cast(),
            (oldsize + EXTRA_STACK) as usize * size_of::<StackValue>(),
            (newsize + EXTRA_STACK) as usize * size_of::<StackValue>(),
        )
        .cast::<StackValue>()
    };
    unsafe { (*G(L)).gcstopem = oldgcstop };
    if newstack.is_null() {
        unsafe { correctstack(L, oldstack) };
        if raiseerror != 0 {
            unsafe { luaD_throw(L, LUA_ERRMEM) };
        }
        return 0;
    }
    unsafe {
        (*L).stack.p = newstack;
        correctstack(L, oldstack);
        (*L).stack_last.p = (*L).stack.p.add(newsize as usize);
    }
    for i in (oldsize + EXTRA_STACK)..(newsize + EXTRA_STACK) {
        unsafe { setnilvalue(s2v(newstack.add(i as usize))) };
    }
    1
}
```

这是非常糟糕的Rust代码，因为重写到现在这个程度之后，很多C的逻辑与写法完全可以重构到Rust中而不依赖任何C的abi了。另一个问题是Lua的throw功能依赖于longjmp/setjmp，可是Rust不提供这个功能，而且ljsj并不安全。这里面LLM告诉我Rust支持[C-unwind](https://rust-lang.github.io/rfcs/2945-c-unwind-abi.html)这样的调用，可以说是非常牛逼！

现在的问题就是把Parser、垃圾回收器以及lvm.c中的超级大的luaV\_execute函数了。

```rust
#[unsafe(no_mangle)]
pub unsafe extern "C-unwind" fn luaV_execute(L: *mut lua_State, mut ci: *mut CallInfo) {
    let mut trap = 0;
    let mut keep_trap = false;
    'newframe: loop {
        if !keep_trap {
            trap = (*L).hookmask;
        }
        keep_trap = false;
        let cl = ci_func(ci);
        let k = (*(*cl).p).k;
        let mut pc = (*ci).u.l.savedpc;
        if trap != 0 {
            trap = luaG_tracecall(L);
        }
        let mut base = (*ci).func.p.add(1);
        loop {
            if trap != 0 {
                trap = luaG_traceexec(L, pc);
                updatebase(ci, &mut base);
            }
            let i = *pc;
            pc = pc.add(1);
            debug_assert!(base == (*ci).func.p.add(1));
            debug_assert!(base <= (*L).top.p && (*L).top.p <= (*L).stack_last.p);
            debug_assert!(luaP_isIT(i) != 0 || { (*L).top.p = base; true });
            match GET_OPCODE(i) {
                OP_MOVE => {
                    let ra = RA(base, i);
                    setobjs2s(L, ra, RB(base, i));
                }
				...
				}
			}
		}
	}
```

这是非常机械化的重写，完全由AI完成。

#### A Few Minutes Later

现在就得到了一个Rust和C风格混杂的代码库，但是可以在x86_64 linux、MacOS、wasm32 wasip2这几个target上编译并运行（aarch64 linux有c\_char类型问题也是非常离谱的，Rust中的c\_char跟随平台的C char类型，可能是i8也可能是u8，取决于平台）。

在[lua wasm-example](https://707state.github.io/lua)可以尝试一下。

接下来要把代码库变得更Rusty就得彻底替换掉原先的那些C风格的代码，工作量非常大。

## Wasm

目标是在[sitegen](https://github.com/707state/sitegen)中使用Lua来构建一些有趣的功能，但是目前一个非常大的问题是，所有依赖于luaD\_throw的功能在当前的实现下都是不可用的。这个功能我为了避免依赖setjmp/longjmp而采用了Rust的catch\_unwind和AssertUnwindSafe，这两个功能我遇到了非常奇怪的bug，举个例子：


```lua
for k,v in pairs({name="Lua", ver=5.4}) do print(k,v) end
```

我尝试在wasmtime和浏览器中运行时，都会遇到如下的崩溃：

```txt
Lua 5.5.0  Copyright (C) 1994-2025 Lua.org, PUC-Rio
> for k,v in pairs({name="Lua", ver=5.4}) do print(k,v) end
Error: failed to run main module `target/wasm32-wasip2/debug/lua.wasm`

Caused by:
    0: failed to invoke `run` function
    1: error while executing at wasm backtrace:
    0:  0xfb34e - abort
                    at wasisdk://v30.0/build/sysroot/wasi-libc-wasm32-wasip2-build-prefix/src/wasi-libc-wasm32-wasip2-build-build/wasisdk://v30.0/src/wasi-libc/libc-bottom-half/sources/abort.c:5:3
    1:  0xf5af4 - std[66c934bf258a385e]::sys::pal::wasi::abort_internal
                    at /rustc/91021ccc790478a1a89c003e7d32b8d155ae6aae/library/std/src/sys/pal/wasi/mod.rs:28:14
    2:  0xf35dd - std[66c934bf258a385e]::process::abort
                    at /rustc/91021ccc790478a1a89c003e7d32b8d155ae6aae/library/std/src/process.rs:2536:5
    3:  0xf3cb5 - __rustc[e81a526174b51780]::__rust_abort
                    at /rustc/91021ccc790478a1a89c003e7d32b8d155ae6aae/library/std/src/rt.rs:33:5
    4:  0xf0d3e - __rustc[e81a526174b51780]::__rust_start_panic
                    at /rustc/91021ccc790478a1a89c003e7d32b8d155ae6aae/library/panic_abort/src/lib.rs:50:5
    5:  0xf3ae2 - __rustc[e81a526174b51780]::rust_panic
                    at /rustc/91021ccc790478a1a89c003e7d32b8d155ae6aae/library/std/src/panicking.rs:886:25
    6:  0xf2b7d - std[66c934bf258a385e]::panicking::panic_with_hook
                    at /rustc/91021ccc790478a1a89c003e7d32b8d155ae6aae/library/std/src/panicking.rs:850:5
    7:  0xdc576 - std[66c934bf258a385e]::panicking::begin_panic::<lua_rs[d85d8228016bf12d]::runtime::LuaError>::{closure#0}
                    at /Users/jask/.rustup/toolchains/nightly-aarch64-apple-darwin/lib/rustlib/src/rust/library/std/src/panicking.rs:762:9
    8:  0xdc39a - std[66c934bf258a385e]::sys::backtrace::__rust_end_short_backtrace::<std[66c934bf258a385e]::panicking::begin_panic<lua_rs[d85d8228016bf12d]::runtime::LuaError>::{closure#0}, !>
                    at /Users/jask/.rustup/toolchains/nightly-aarch64-apple-darwin/lib/rustlib/src/rust/library/std/src/sys/backtrace.rs:182:18
    9:  0xdc45b - std[66c934bf258a385e]::panicking::begin_panic::<lua_rs[d85d8228016bf12d]::runtime::LuaError>
                    at /Users/jask/.rustup/toolchains/nightly-aarch64-apple-darwin/lib/rustlib/src/rust/library/std/src/panicking.rs:761:5
   10:  0x708b6 - std[66c934bf258a385e]::panic::panic_any::<lua_rs[d85d8228016bf12d]::runtime::LuaError>
                    at /Users/jask/.rustup/toolchains/nightly-aarch64-apple-darwin/lib/rustlib/src/rust/library/std/src/panic.rs:260:5
   11:  0x9d8fa - lua_rs[d85d8228016bf12d]::do_rs::luaD_throw
                    at /Users/jask/codes/lua/src/do.rs:167:9
   12:  0x54790 - lua_rs[d85d8228016bf12d]::lex::luaD_throw
                    at /Users/jask/codes/lua/src/lex.rs:73:14
   13:  0x56239 - lua_rs[d85d8228016bf12d]::lex::lexerror
                    at /Users/jask/codes/lua/src/lex.rs:189:14
   14:  0x59d19 - lua_rs[d85d8228016bf12d]::lex::luaX_syntaxerror
                    at /Users/jask/codes/lua/src/lex.rs:264:14
   15:  0x88087 - lua_rs[d85d8228016bf12d]::parser_rs::primaryexp
                    at /Users/jask/codes/lua/src/parser.rs:1396:18
   16:  0x89b4d - lua_rs[d85d8228016bf12d]::parser_rs::suffixedexp
                    at /Users/jask/codes/lua/src/parser.rs:1404:9
   17:  0x96310 - lua_rs[d85d8228016bf12d]::parser_rs::simpleexp
                    at /Users/jask/codes/lua/src/parser.rs:1479:17
   18:  0x93ed2 - lua_rs[d85d8228016bf12d]::parser_rs::subexpr
                    at /Users/jask/codes/lua/src/parser.rs:1534:13
   19:  0x88241 - lua_rs[d85d8228016bf12d]::parser_rs::expr
                    at /Users/jask/codes/lua/src/parser.rs:1558:9
    2: wasm trap: wasm `unreachable` instruction executed
```

这段代码中我在primaryexp中检查了参数，并没有发现问题，但是在浏览器环境中会清清楚楚地看到unreachable的情况。经过一段时间的搜索，我认为这是[WASM这个平台的问题](https://internals.rust-lang.org/t/wasm32-unknown-unknown-panic-unwind-support-via-native-wasm-exceptions/18665)。

这个时候还得是搜索引擎发挥作用了，我发现了一个[PR](https://github.com/rust-lang/rust/pull/111322)，我认为值得一试。

总结一下，我现阶段目标是在Browser和wasmtime中稳定使用Lua，现在可以使用所有的不依赖于rawrunprotected的方法，而已知emscripten对于try/catch进行了专门的转换，而对于wasip1/wasip2都不存在setjmp/longjmp，所以这个方案就毙掉！那么接下来就是catch\_unwind了，确实存在一些讨论，关于如何修改panic的行为，但是目前我还没搞明白，再等一下吧。
