<!--toc:start-->
- [虽然是RxJava, 但是Spring也可以很Reactive, 对吧](#虽然是rxjava-但是spring也可以很reactive-对吧)
  - [JpaRepository和CrudRepository之间的区别](#jparepository和crudrepository之间的区别)
- [Bean的生命周期](#bean的生命周期)
- [Spring中@Autowired和@Resource注解有什么区别？](#spring中autowired和resource注解有什么区别)
- [Spring的IOC介绍一下](#spring的ioc介绍一下)
- [为什么依赖注入不适合用字段注入？](#为什么依赖注入不适合用字段注入)
- [Spring中的AOP讲一下](#spring中的aop讲一下)
- [Spring事务，使用this调用是否生效？](#spring事务使用this调用是否生效)
- [Spring如何解决循环以来问题？](#spring如何解决循环以来问题)
- [Spring IOC实现机制](#spring-ioc实现机制)
- [Spring AOP实现机制](#spring-aop实现机制)
<!--toc:end-->

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

# Spring的IOC介绍一下

IOC：Inversion Of Control即控制反转，是一种设计思想。在传统的Java SE程序设计中，是直接在对象内部new的方式来创建对象，是程序主动创建依赖对象。

在Spring中，IOC是有专门的容器去控制对象。

控制就是对象的创建、初始化、销毁。

IOC解决了繁琐的对象生命周期，解耦了我们的代码。

反转：反转了控制权，由Spring来控制对象。

# 为什么依赖注入不适合用字段注入？

字段注入可能带来的问题：

- 对象的外部可见性。
- 可能导致循环依赖。
- 无法设置注入的对象为final，也无法注入静态变量。

# Spring中的AOP讲一下

用于实现面向切片编程。

在面向切片中，功能分为两种。

- 核心功能：登陆、注册、增删改查都是核心业务。
- 周边业务：日志、事务管理这些次要的是周边业务。

在面向切面编程中，核心业务功能和周边功能是分别独立进行开发，两者不是耦合的，然后把切面功能和核心业务功能 "编织" 在一起，这就叫AOP。

AOP能够将与业务无关，却为业务模块所共同调用的逻辑或责任（例如事务处理、日志管理、权限控制等）封装起来，便于减少系统的重复代码，降低模块间的耦合度，并且有利于未来的可拓展性和可维护性。

几个概念：

- AspectJ：切面，只是一个概念，没有具体的接口或类与之对应，是Join point，Advice和Pointcut的一个统称。
- Join point：连接点，指程序执行过程中的一个点，例如方法调用、异常处理等。在Spring AOP中，仅支持方法级别的连接点。
- Advice：通知，即我们定义的一个切面中的横切逻辑，有"around"，"before"和"after"三种类型。Adviceg通常作为一个拦截器，也可以包含许多个拦截器作为一条链路围绕这Join point进行处理。
- Pointcut：切点，用于匹配连接点，一个AspectJ中包含哪些Join point需要由Pointcut进行筛选。
- Introduction：引介，让一个切面可以声明被通知的对象实现任何他们没有真正实现的额外的接口。例如可以让一个代理对象代理两个目标类。
- Weaving：织入，在有了连接点、切点、通知以及切面，如何将它们应用到程序中呢？没错，就是织入，在切点的引导下，将通知逻辑插入到目标方法上，使得我们的通知逻辑在方法调用时得以执行。
- AOP Proxy：AOP 代理，指在 AOP 实现框架中实现切面协议的对象。在 Spring AOP 中有两种代理，分别是 JDK 动态代理和 CGLIB 动态代理。
- Target：目标对象，就是被代理的对象。

Spring AOP是基于JDK动态代理和Cglib提升实习的，两种代理方式都属于运行时的一个方式，所以他没有编译时的一个处理，那么因此Spring是通过Java代码实现的。

# Spring事务，使用this调用是否生效？

不能生效，因为Spring事务是通过代理对象来控制的，只有通过代理对象的方法调用才会应用事务管理的相关规则。当使用this直接调用时，是绕过了Spring的代理机制，因此不会应用事务设置。

# Spring如何解决循环以来问题？

循环依赖指的是两个类中的属性相互依赖对方：例如 A 类中有 B 属性，B 类中有 A属性，从而形成了一个依赖闭环。

Spring主要有三种情况：

1. 通过构造方法进行依赖注入时产生的循环依赖问题。
2. 通过setter方法进行依赖注入且是在多例（原型）模式下产生的循环依赖问题。
3. 通过setter方法进行依赖注入且是在单例模式下产生的循环依赖问题。

只有第三种被解决了，1&2都会抛出异常。

Spring 解决单例模式下的setter循环依赖问题的主要方式是通过三级缓存解决循环依赖。三级缓存指的是 Spring 在创建 Bean 的过程中，通过三级缓存来缓存正在创建的 Bean，以及已经创建完成的 Bean 实例。具体步骤如下：

1. 实例化Bean：Spring 在实例化 Bean 时，会先创建一个空的 Bean 对象，并将其放入一级缓存中。
2. 属性赋值：Spring 开始对 Bean 进行属性赋值，如果发现循环依赖，会将当前 Bean 对象提前暴露给后续需要依赖的 Bean（通过提前暴露的方式解决循环依赖）。
3. 初始化Bean：完成属性赋值后，Spring 将 Bean 进行初始化，并将其放入二级缓存中。
4. 注入依赖：Spring对Bean进行依赖注入，如果发现循环依赖，会从二级缓存中获取已经完成初始化的Bean实例。

通过三级缓存的机制，Spring能够在处理循环依赖时，确保及时暴露正在创建的Bean对象，并能够正确地注入已经初始化的Bean实例，从而解决循环依赖问题，保证应用程序的正常运行。

# Spring IOC实现机制

- 反射：Spring IOC容器利用Java的反射机制动态地加载类、创建对象实例以及调用对象方法，反射允许在运行时检查类、方法、属性等信息，从而实现灵活的对象实例化和管理。
- 依赖注入：OC的核心概念是依赖注入，即容器负责管理应用程序组件之间的依赖关系。Spring通过构造函数注入、属性注入或方法注入，将组件之间的依赖关系描述在配置文件中或使用注解。
- 设计模式-工厂模式：Spring IOC容器通常采用工厂模式来管理对象的创建和生命周期。容器作为工厂负责实例化Bean并管理他们的生命周期，将Bean的实例化过程交给容器来管理。
- 容器实现：Spring IOC容器是实现IOC的核心，通常使用BeanFactory或ApplicationContext来管理Bean。BeanFactory是IOC容器的基本形式，提供基本的IOC功能；ApplicationContext是BeanFactory的拓展，并提供更多的企业级功能。

# Spring AOP实现机制

Spring AOP的实现依赖于动态代理技术，动态代理实在运行时动态生成代理对象，而不是在编译时。他允许开发者在运行时指定要代理的接口和行为，从而在不改动源代码的情况下增强方法的功能。
Spring AOP支持两种动态代理：
1. JDK动态代理：使用java.lang.reflect.Proxy类和java.lang.reflect.InvocationHandler接口实现。这种方式需要代理的类实现一个或多个接口。
2. Cglib的动态代理：当被代理的类没有实现接口时，Spring会使用Cglib库生成一个被代理类的子类作为代理。Cglib是一个第三方代码生成库，通过继承方式实现代理。
