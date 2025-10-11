<!--toc:start-->
- [如何把汇编的二进制塞入C代码里面并执行？](#如何把汇编的二进制塞入c代码里面并执行)
- [更高级的抽象！](#更高级的抽象)
<!--toc:end-->

准备写一个minilisp的compiler，但是目前处于一个非常初期，还在学习的状态。

# 如何把汇编的二进制塞入C代码里面并执行？

首先先看看怎么在我的macmini上运行起来吧。

首先是汇编文件：

```asm
.global _start
_start:
	mov X0, #42
	ret
```

这段汇编就是我想要存入C文件的代码，问题是怎么以二进制形式嵌入呢？

首先先把汇编编译为object文件(clang -c)，然后用otool -s \_\_TEXT \_\_text xxx.o的方式查看。

注意macos arm64是little endian，也就是说得把得到的二进制颠倒顺序。

```txt
//otool结果
Archive : build/libret.a
build/libret.a(ret.s.o):
(__TEXT,__text) section
0000000000000000 d2800540 d65f03c0
```

对应的C文件：
```c
#include <assert.h>   /* for assert */
#include <stddef.h>   /* for NULL */
#include <string.h>   /* for memcpy */
#include <sys/mman.h> /* for mmap and friends */
// ARM64 汇编代码：
// mov x0, #42    ; 将 42 放入返回值寄存器 x0
// ret            ; 返回
const unsigned char program[] = {
    // mov x0, #42
    0x40,0x05, 0x80, 0xd2,
    // ret
    0xc0,0x03,0x5f,0xd6
};

const int kProgramSize = sizeof(program);
typedef int (*JitFunction)();
int main() {
  // 分配可执行内存
  void *memory = mmap(/*addr=*/NULL, /*length=*/kProgramSize,
                      /*prot=*/PROT_READ | PROT_WRITE,
                      /*flags=*/MAP_ANON | MAP_PRIVATE,  // macOS 使用 MAP_ANON 而不是 MAP_ANONYMOUS
                      /*filedes=*/-1, /*offset=*/0);
  if (memory == MAP_FAILED) {
    assert(0 && "mmap failed");
    return -1;
  }
  memcpy(memory, program, kProgramSize);
  int result = mprotect(memory, kProgramSize, PROT_READ | PROT_EXEC);
  assert(result == 0 && "mprotect failed");
  JitFunction function = (JitFunction)memory;
  int return_code = function();
  assert(return_code == 42 && "the assembly was wrong");
  // 注意munmap
  result = munmap(memory, kProgramSize);
  assert(result == 0 && "munmap failed");

  return return_code;
}
```

这里用到了mmap的方式来把内存执行起来。

# 更高级的抽象！
