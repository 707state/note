
<!--toc:start-->
- [心血来潮，玩玩xv6](#心血来潮玩玩xv6)
<!--toc:end-->

# 心血来潮，玩玩xv6

xv6已经给了一个非常完整的qemu命令，就是make qemu-gdb，然后把这一段：

vscode

```json
{
  "type": "lldb",
  "request": "launch",
  "name": "Riscv",
  "program": "${workspaceFolder}/kernel/kernel",
  "cwd": "${workspaceFolder}",
  "args": [],
  "processCreateCommands": ["gdb-remote 127.0.0.1:25501"]
}
```

写到配置文件里面，就可以开始debug了！
