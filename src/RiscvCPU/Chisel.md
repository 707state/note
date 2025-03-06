# Chisel
本次实验我要使用Chisel3 3.6.1版本，采用chiseltest和scalatest做测试。

Scala Version: 2.12.13。

采用firtool来生成Verilog代码。

一些基本Chisel代码的内容需要学习。

## Module

可以参考Verilog中的module, 在语义上是一致的。

## Bundle/Vec

## Mux
这是用来选择输入用的，有几个内置函数。

### Mux
二路选择

### MuxCase
n路选择器，根据给出的条件选择对应的结果。

### MuxLookup

n路索引选择。
```scala
MuxLookup(idx, default)(Seq(0.U -> a, 1.U -> b, ...))
```

### Mux1H
在同一时间只有一个选择信号是激活的，其他作用于MuxLookup相同。

## Reg
寄存器类型

### RegEnable
绝大多数时间，使能信号enable都是高电平的（true），此时寄存器输出会以一个时钟周期延时跟随输入变化。而在第4个时钟周期，使能信号置低电平，此时寄存器输出会在D处的上升沿保持值5。

```scala
val enableReg = Reg(UInt(4.W))
when (enable) {
    enbaleReg := inVal
}
```
为了简化，Chisel提供了RegEnable
```scala
val enableReg2 = RegEnable(inVal, enable)
```
对于有复位信号的：

```scala
val resetEnableReg = RegInit(0.U(4.W))

when (enable) {
    resetEnableReg := inVal
}
```
可以简化为：
```scala
val resetEnableReg2 = RegEnable(inVal, 0.U(4.W), enable)
```


