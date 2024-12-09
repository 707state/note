# 虽然是RxJava, 但是Spring也可以很Reactive, 对吧

## JpaRepository和CrudRepository之间的区别

JpaRepository拓展了PagingAndSortingRepository, 而PagingAndSortingRepository又拓展了CrudRepository。

CrudRepository 主要提供 CRUD （增删改查）功能；
PagingAndSortingRepository 提供了执行分页和排序数据的方法；
JpaRepository 则提供了一些与 JPA 相关的方法，比如刷新持久性上下文和删除批处理中的记录；

Spring Data 核心库提供了两个基本接口：

    CrudRepository - CRUD （增删改查）方法；
    PagingAndSortingRepository - 扩展 CrudRepository，主要是分页和排序方法；

