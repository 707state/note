# 虽然是RxJava, 但是Spring也可以很Reactive, 对吧

## JpaRepository和CrudRepository之间的区别

JpaRepository拓展了PagingAndSortingRepository, 而PagingAndSortingRepository又拓展了CrudRepository。

CrudRepository 主要提供 CRUD （增删改查）功能；
PagingAndSortingRepository 提供了执行分页和排序数据的方法；
JpaRepository 则提供了一些与 JPA 相关的方法，比如刷新持久性上下文和删除批处理中的记录；

Spring Data 核心库提供了两个基本接口：

    CrudRepository - CRUD （增删改查）方法；
    PagingAndSortingRepository - 扩展 CrudRepository，主要是分页和排序方法；


# Bean的生命周期

1. Spring启动，查找并加载需要被Spring管理的bean，进行Bean的实例化
2. Bean实例化后对将Bean的引入和值注入到Bean的属性中
3. 如果Bean实现了BeanNameAware接口的话，Spring将Bean的Id传递给setBeanName()方法
4. 如果Bean实现了BeanFactoryAware接口的话，Spring将调用setBeanFactory()方法，将BeanFactory容器实例传入
5. 如果Bean实现了ApplicationContextAware接口的话，Spring将调用Bean的setApplicationContext()方法，将bean所在应用上下文引用传入进来。
6. 如果Bean实现了BeanPostProcessor接口，Spring就将调用他们的postProcessBeforeInitialization()方法。
7. 如果Bean 实现了InitializingBean接口，Spring将调用他们的afterPropertiesSet()方法。类似的，如果bean使用init-method声明了初始化方法，该方法也会被调用
8. 如果Bean 实现了BeanPostProcessor接口，Spring就将调用他们的postProcessAfterInitialization()方法。
9. 此时，Bean已经准备就绪，可以被应用程序使用了。他们将一直驻留在应用上下文中，直到应用上下文被销毁。
10. 如果bean实现了DisposableBean接口，Spring将调用它的destory()接口方法，同样，如果bean使用了destory-method 声明销毁方法，该方法也会被调用。
