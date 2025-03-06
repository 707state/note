# LDKV

最近一直在学Redis, 我觉得就是学Redis不能光背书，还的是看看实现细节的。

那就自己实现一个类似于Redis的键值对存储吧。

# 数据结构

先实现类似于Redis的集中数据结构（封装folly库），先不考虑复杂的超时策略，只考虑惰性删除。

## Listpack

## TTLHash

## TTLSet

## Bloomfilter