---
title: LuaJIT与JIT Compiler
author: jask
tags:
  - Lua
  - ProgrammingLanguages
date: 2026-05-19
series: 深入理解lua
---

# LuaJIT

久闻大名，但是我从来没看过LuaJIT的实现，也不知道LuaJIT的JIT实现是怎么样的，那现在就来看看吧。

## 构建流程

LuaJIT是依赖Lua来bootstrap的，在没有Lua的设备上，luajit构建前会先编译`src/host/minilua.c`来生成一个lua 5.1的解释器用于codegen，主要用于生成`luajit.h`、`buildvm_arch.h`这两个文件，这两个文件分别从`src/host/genversion.lua`和`dynasm/dynasm.lua`这两个文件结合头文件生成出来。

构建流程可以参考如下meson代码：

`luajit.h`

```meson
luajit_h = custom_target('luajit.h',
  input : ['src/luajit_rolling.h', relver_txt],
  output : 'luajit.h',
  command : [minilua, files('src/host/genversion.lua'), '@INPUT0@', '@INPUT1@', '@OUTPUT@'],
  install : true,
  install_dir : get_option('includedir') / ('luajit-' + mmversion))
```

`buildvm_arch.h`

```meson
buildvm_arch_h = custom_target('buildvm_arch.h',
  input : 'src/vm_' + dasm_arch + '.dasc',
  output : 'buildvm_arch.h',
  command : [minilua, files('dynasm/dynasm.lua'), dasm_flags, '-o', '@OUTPUT@', '@INPUT@'])
```

这两个文件生成之后要构建的是`buildvm`，这是用来生成`lj_vm.S`和`lj_bcdef.h`等头文件的工具，以及`vmdef.lua`。

meson代码如下：

```meson
ljlib_c = [
  'src/lib_base.c',
  'src/lib_math.c',
  'src/lib_bit.c',
  ...
]

lj_vm_s = custom_target('lj_vm.S',
  output : 'lj_vm.S',
  command : [buildvm, '-m', ljvm_mode, '-o', '@OUTPUT@'])


vmdef_lua = custom_target('vmdef.lua',
  input : ljlib_c,
  output : 'vmdef.lua',
  command : [buildvm, '-m', 'vmdef', '-o', '@OUTPUT@', '@INPUT@'],
  install : true,
  install_dir : ljlibdir / 'jit')

```

注意，这里生成的`vmdef.lua`是运行时与luajit交互的，因此会和加载方式有关。

有了`minilua`和`buildvm`生成的文件之后，就可以用来构建`luajit`了。

## 和lua的区别

`luajit`在特性和字节码上和lua5.1兼容，但是实现上的差别还是很大。

`luajit`首先是一个tracing jit compiler，而不是一个函数/方法级的jit compiler。流程上大致是：源码->parser生成字节码->assembly interpreter运行->hot loop/call触发trace recording->recorder生成IR->optimizer重写IR->汇编器生成机器码->字节码被patch到编译后的代码中。

## 重点代码

Lua的执行前面几篇已经看得非常详细了，像是register vm、协程上下文切换的实现都已经分析过了，LuaJIT的重点将会是codegen。

Lua虚拟机本身执行字节码，而LuaJIT会把运行过程中的热点字节码编译为native代码，举一个简单的代码例子：

```lua
local a = 1
while a <= 1000 do
    a = a + 1
end
```

这段代码首先会编译成字节码，使用`luajit -bl`可以查看：

```asm
0007  JMP      1 => 0002
0002  KSHORT   1 1000 -- 常量放入栈槽1
0003  ISGT     0   1
0004  JMP      1 => 0008
0005  LOOP     1 => 0008 -- 热点计数
```

运行到LOOP字节码处会有如下代码：

```asm
 case BC_LOOP:
    |  // RA = base, RC = target (loop extent)
    |  // Note: RA/RC is only used by trace recorder to determine scope/extent
    |  // This opcode does NOT jump, it's only purpose is to detect a hot loop.
    |.if JIT
    |  hotloop
    |.endif
    |  // Fall through. Assumes BC_ILOOP follows.
    break;

|.macro hotloop
|  hotcheck HOTCOUNT_LOOP
|  blo ->vm_hotloop
|.endmacro
```

当`hotcount`归零时，会跳转到`vm_hotloop`：

```asm
  |->vm_hotloop:
  |  ldr LFUNC:CARG3, [BASE, FRAME_FUNC]   // 获取当前函数对象
  |   add CARG1, GL, #GG_G2DISP+GG_DISP2J  // CARG1 = &J (jit_State*)
  |   str PC, SAVE_PC                       // 保存当前PC
  |  ldr CARG3, LFUNC:CARG3->pc            // 获取 proto
  |   mov CARG2, PC                         // CARG2 = pc (注意：PC指向LOOP)
  |   str L, [GL, #GL_J(L)]                 // J->L = L
  |  ldrb CARG3w, [CARG3, #PC2PROTO(framesize)]  // 取 framesize
  |   str BASE, L->base                     // 保存当前栈基址
  |  add CARG3, BASE, CARG3, lsl #3        // 计算 top = base + framesize
  |  str CARG3, L->top                      // 保存 top
  |  bl extern lj_trace_hot                 // ★ 调用 JIT 入口
  |  b <3                                   // 返回解释器循环
```

然后会跳转到`lj_trace_hot`处：

```c
/* A hotcount triggered. Start recording a root trace. */
void LJ_FASTCALL lj_trace_hot(jit_State *J, const BCIns *pc)
{
  /* Note: pc is the interpreter bytecode PC here. It's offset by 1. */
  ERRNO_SAVE
  /* Reset hotcount. */
  hotcount_set(J2GG(J), pc, J->param[JIT_P_hotloop]*HOTCOUNT_LOOP);
  /* Only start a new trace if not recording or inside __gc call or vmevent. */
  if (J->state == LJ_TRACE_IDLE &&
      !(J2G(J)->hookmask & (HOOK_GC|HOOK_VMEVENT))) {
    J->parent = 0;  /* Root trace. */
    J->exitno = 0;
    J->state = LJ_TRACE_START;
    lj_trace_ins(J, pc-1);
  }
  ERRNO_RESTORE
}
```

接下来会去录制每条字节码对应的IR：

```c
/* A bytecode instruction is about to be executed. Record it. */
void lj_trace_ins(jit_State *J, const BCIns *pc)
{
  /* Note: J->L must already be set. pc is the true bytecode PC here. */
  J->pc = pc;
  J->fn = curr_func(J->L);
  J->pt = isluafunc(J->fn) ? funcproto(J->fn) : NULL;
  while (lj_vm_cpcall(J->L, NULL, (void *)J, trace_state) != 0) // 这里trace_state是回调函数
    J->state = LJ_TRACE_ERR;
}
```

在`trace_state`中，会进行`trace_start`、`lj_records_setup`等，然后进入录制循环，逐条调用`lj_record_ins`来记录指令。

JIT Compiler开始录制虚拟机指令后，会把字节码转换为对应的IR形式，这一部分工作在`lj_record_ins`完成。

```c
/* Get TRef from slot. Load slot and specialize if not done already. */		  
#define getslot(J, s)	(J->base[(s)] ? J->base[(s)] : sload(J, (int32_t)(s)))

void lj_record_ins(jit_State *J){
...
  lbase = J->L->base;       // 解释器的真实栈
  ins = *pc;                 // 当前字节码指令
  op = bc_op(ins);           // 操作码
  ra = bc_a(ins);            // 操作数 A

  switch (bcmode_a(op)) {    // 操作数 A 的模式
  case BCMvar:               // A 是变量（从栈槽）
    copyTV(J->L, rav, &lbase[ra]);        // 保存运行时值（用于特化判断）
    ix.val = ra = getslot(J, ra);         // ★ 从 J->base[] 取 TRef
    break;
  }
```

生成IR的部分：

```c
  switch (op) {

  /* -- Comparison ops ---------------------------------------------------- */

  case BC_ISLT: case BC_ISGE: case BC_ISLE: case BC_ISGT:
#if LJ_HASFFI
    if (tref_iscdata(ra) || tref_iscdata(rc)) {
      rec_mm_comp_cdata(J, &ix, op, ((int)op & 2) ? MM_le : MM_lt);
      break;
    }
#endif
    /* Emit nothing for two numeric or string consts. */
    if (!(tref_isk2(ra,rc) && tref_isnumber_str(ra) && tref_isnumber_str(rc))) {
      IRType ta = tref_isinteger(ra) ? IRT_INT : tref_type(ra);
      IRType tc = tref_isinteger(rc) ? IRT_INT : tref_type(rc);
      int irop;
      if (ta != tc) {
	/* Widen mixed number/int comparisons to number/number comparison. */
	if (ta == IRT_INT && tc == IRT_NUM) {
	  ra = emitir(IRTN(IR_CONV), ra, IRCONV_NUM_INT);
	  ta = IRT_NUM;
	} else if (ta == IRT_NUM && tc == IRT_INT) {
	  rc = emitir(IRTN(IR_CONV), rc, IRCONV_NUM_INT);
	} else if (LJ_52) {
	  ta = IRT_NIL;  /* Force metamethod for different types. */
	} else if (!((ta == IRT_FALSE || ta == IRT_TRUE) &&
		     (tc == IRT_FALSE || tc == IRT_TRUE))) {
	  break;  /* Interpreter will throw for two different types. */
	}
      }
      rec_comp_prep(J);
      irop = (int)op - (int)BC_ISLT + (int)IR_LT;
      if (ta == IRT_NUM) {
	if ((irop & 1)) irop ^= 4;  /* ISGE/ISGT are unordered. */
	if (!lj_ir_numcmp(numberVnum(rav), numberVnum(rcv), (IROp)irop))
	  irop ^= 5;
      } else if (ta == IRT_INT) {
	if (!lj_ir_numcmp(numberVnum(rav), numberVnum(rcv), (IROp)irop))
	  irop ^= 1;
      } else if (ta == IRT_STR) {
	if (!lj_ir_strcmp(strV(rav), strV(rcv), (IROp)irop)) irop ^= 1;
	ra = lj_ir_call(J, IRCALL_lj_str_cmp, ra, rc);
	rc = lj_ir_kint(J, 0);
	ta = IRT_INT;
      } else {
	rec_mm_comp(J, &ix, (int)op);
	break;
      }
      emitir(IRTG(irop, ta), ra, rc);
      rec_comp_fixup(J, J->pc, ((int)op ^ irop) & 1);
    }
    break;
...
}
```

`emitir`会通过`lj_ir_set` 把操作码和操作数放入 `J->fold.ins`，并且通过`lj_opt_fold` 尝试常量折叠、代数简化、CSE 等优化方式。

当循环再次回到LOOP字节码时，`rec_loop_interp`会被调用。

从IR编译成Native机器码是在`lj_asm_trace`进行的，每条IR通过`asm_ir`分发给架构相关的handler。

