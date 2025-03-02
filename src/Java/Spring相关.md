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

# Spring中@Autowired和@Resource注解有什么区别？

1. 来源不同：Autowired是Spring提供的注解，Resource是Java EE的JSR-250规范的一部分，由Java本身提供。
2. 注入方式：Autowired是通过类型进行注入，如果容器中存在个相同类型的实力，还可以与Qualifier注解一起使用，通过指定bean的id来注入特定的实例。Resource默认是通过名称(ByName)进行注入。如果未指定名称，则会尝试通过类型进行匹配。
3. 属性：Autowired可以不指定任何属性，仅通过类型自动装配。Resource可以指定一个名为name的属性，该属性表示要注入的bean的名称。
4. 依赖性：使用Autowired时，通常需要依赖Spring的框架，使用Resource时，即使不再Spring框架下，也可以和任何符合Java EE的规范的环境中工作。
5. 使用场景：如果需要更细粒度的控制注入过程，或者需要支持Spring框架之外的Java EE环境时，Resource注解可能是一个更好的选择；如果完全在Spring的环境下工作，并且希望通过类型自动装配，Autowired是更常见的选择。
