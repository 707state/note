-   [InfluxDB: Why Time
    Series](#influxdb-why-time-series)
    -   [什么是时序数据？](#什么是时序数据)
    -   [时序数据的使用场景](#时序数据的使用场景)
        -   [DevOps监控](#devops监控)
        -   [实时分析](#实时分析)
        -   [IoT](#iot)
-   [InfluxDB数据模型](#influxdb数据模型)

# InfluxDB: Why Time Series

不懂，学学

## 什么是时序数据？

时序数据与一般的数据之间的区别在于，时序数据永远是关于时间的问题。

最简单的判断方法就是判断自变量是不是时间。

常见的有股市数据等。

常见的时序数据有两种形式，标准/非标准。

标准的数据包括从软硬件中定期采集的数据；非标准的常常是事件驱动的数据。

## 时序数据的使用场景

### DevOps监控

流量监控、数据存储等

### 实时分析

OLAP类业务

### IoT

物联网需求。

# InfluxDB数据模型

InfluxDB发送数据的协议如下：

`<measurement>`{=html}, `<tag set>`{=html} `<field set>`{=html}
`<timestamp>`{=html}

measurement是一个字符串，tag set是一组键值对，所有的值都是字符串，field
set是彝族键值对，值是int64, float64, bool或者字符串。

measurement和tag set都保存在倒排索引中。

在硬盘上，数据是按照列式格式，其中时间、测量值、标签集和字段集按照连续的块进行存储。标签和字段的数量没有限制。其他时间序列解决方案不支持多个字段，这会导致在传输共享标签集的数据时，其网络协议变得臃肿。此外，大多数其他时间序列解决方案仅支持
float64 类型的值，这意味着用户无法将额外的元数据与时间序列数据一同编码。
