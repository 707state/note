# F-Bounded Polymorphism

用于解决自引用类型和递归类型约束的问题。

## 核心

在泛型类型参数的约束中引用这个泛型类型参数本身。

其表达为：

```rust
F<T> where T extends F<T>
```

## 实际应用

这个东西在Java中有着非常广泛的应用。

Java 的 Enum 类是 F-Bounded Polymorphism 的经典例子：


```java
public abstract class Enum<E extends Enum<E>> implements Comparable<E> {
    // ...
}
```

这确保了枚举常量只能与同一枚举类型的其他常量进行比较。

实现建造者模式也非常有用。

```java
public abstract class Builder<T extends Builder<T>> {
    protected abstract T self();
    
    public T withName(String name) {
        // 设置 name
        return self();
    }
    
    public T withAge(int age) {
        // 设置 age
        return self();
    }
}

public class PersonBuilder extends Builder<PersonBuilder> {
    @Override
    protected PersonBuilder self() {
        return this;
    }
    
    public Person build() {
        // 创建 Person 对象
        return new Person();
    }
}

// 使用
Person person = new PersonBuilder()
    .withName("John")
    .withAge(30)
    .build();
```

## 不同语言中的实现

Java 中的 F-Bounded Polymorphism 通过泛型约束实现：

```java
interface SelfComparable<T extends SelfComparable<T>> {
    int compareTo(T other);
}
```

TypeScript：

```typescript
interface Entity<T extends Entity<T>> {
    equals(other: T): boolean;
    clone(): T;
}

class User implements Entity<User> {
    equals(other: User): boolean {
        // 实现比较逻辑
        return true;
    }
    
    clone(): User {
        // 实现克隆逻辑
        return new User();
    }
}
```

Scala:

```scala
trait Copyable[T <: Copyable[T]] {
  def copy(): T
}

class Document extends Copyable[Document] {
  override def copy(): Document = {
    // 实现复制逻辑
    new Document()
  }
}
```

## 问题

- 类型递归：F-Bounded 类型定义本身是递归的，这可能导致复杂的类型错误和编译器问题。
- 类型层次结构：在复杂的继承层次结构中使用 F-Bounded Polymorphism 可能会变得非常复杂。
- 替代方案：一些语言提供了其他机制来解决相同的问题，如 Scala 的 self-types 或 TypeScript 的 this 类型。

