-   [Functional Rust](#functional-rust)
    -   [Functor](#functor)
    -   [Monad](#monad)
        -   [do作用](#do作用)
    -   [Monoid](#monoid)
    -   [Apply](#apply)
    -   [Applicative](#applicative)
    -   [SemiGroup](#semigroup)
    -   [Semigroupoid](#semigroupoid)
    -   [State](#state)
    -   [Fold](#fold)
    -   [Free](#free)
    -   [Traverse](#traverse)

# Functional Rust

## Functor

实现如下：

``` rust
pub trait Functor<'a> {
    /// The inner type which will be mapped over
    type Unwrapped;

    /// Target of the fmap operation. Like `Self`, but has a different wrapped up value underneath.
    type Wrapped<B: 'a>: Functor<'a, Unwrapped = B>;

    /// fmap is used to apply a function of type (a -> b) to a value of type f a, where f is a functor, to produce a value of type f b.
    fn fmap<F, B: 'a>(self, f: F) -> Self::Wrapped<B>
    where
        F: Fn(Self::Unwrapped) -> B + 'a;
}
```

fmap是Functor的核心方法，用于将一个函数应用到Functor包含的值上，生成一个新的Functor实例。
fmap方法接受一个函数f和一个泛型类型参数B，并将f应用于当前Functor实例的Unwrapped值。
f的类型是Fn(Self::Unwrapped) -\> B，即一个从Unwrapped到B的函数。
fmap的返回类型是Self::Wrapped\<B\>，即内部值被映射后的新Functor类型。

Option就是一种Functor

``` rust
impl<'a, A> Functor<'a> for Option<A> {
    type Unwrapped = A;
    type Wrapped<B: 'a> = Option<B>;

    fn fmap<F, B: 'a>(self, f: F) -> Self::Wrapped<B>
    where
        F: Fn(Self::Unwrapped) -> B,
    {
        #[allow(clippy::manual_map)]
        match self {
            Some(a) => Some(f(a)),
            None => None,
        }
    }
}
```

同样的Result也是。

``` rust
impl<'a, A, E> Functor<'a> for Result<A, E> {
    type Unwrapped = A;
    type Wrapped<B: 'a> = Result<B, E>;

    fn fmap<F, B: 'a>(self, f: F) -> Self::Wrapped<B>
    where
        F: Fn(Self::Unwrapped) -> B,
    {
        match self {
            Result::Ok(a) => Result::Ok(f(a)),
            Result::Err(e) => Result::Err(e),
        }
    }
}
```

## Monad

Rust缺少Higher-Kind
Type，不能直接在trait之间构建严格的继承关系，因此Monad没有直接从Applicative派生，而是作为一个独立的trait，实现了"单子"所需的bind和of操作。

``` rust
pub trait Monad<'a> {
    type Unwrapped;
    type Wrapped<T: 'a>;

    /// Sequentially compose actions, piping values through successive function chains.
    ///
    /// The applied linking function must be unary and return data in the same
    /// type of container as the input. The chain function essentially "unwraps"
    /// a contained value, applies a linking function that returns
    /// the initial (wrapped) type, and collects them into a flat(ter) structure.
    fn bind<F, B: 'a>(self, f: F) -> Self::Wrapped<B>
    where

        F: FnOnce(Self::Unwrapped) -> Self::Wrapped<B> + 'a;

    /// Lift a value into a context
    fn of<T: 'a>(value: T) -> Self::Wrapped<T>;
}
```

核心：

> fn bind
> bind是单子的核心操作，它允许对Unwrapped类型的值进行链式操作。bind方法接收一个单参数函数f，该函数接收一个Unwrapped类型的值，并返回一个Self::Wrapped`<B>`{=html}类型的结果。该函数的作用是"解包"当前容器中的值，将其传递给f，再将f的结果重新包装回到容器中，从而实现链式操作。

------------------------------------------------------------------------

> fn of
> of方法用于将一个普通值提升为单子类型的上下文中。例如，对于Option类型的Monad实例，可以将5提升为Some(5)，即创建一个包装5的Option。
> Monad可以对Option实现。

------------------------------------------------------------------------

``` rust
impl<'a, A> Monad<'a> for Option<A> {
    type Unwrapped = A;
    type Wrapped<B: 'a> = Option<B>;

    fn bind<F, B: 'a>(self, f: F) -> Self::Wrapped<B>
    where
        F: FnOnce(Self::Unwrapped) -> Self::Wrapped<B> + 'a,
    {
        match self {
            Some(x) => f(x),
            None => None,
        }

        // self.and_then(f)
    }

    fn of<T: 'a>(value: T) -> Self::Wrapped<T> {
        Some(value)
    }
}
```

``` rust
/// Provides the Haskell monadic syntactic sugar `do`.
#[macro_export]
macro_rules! m {
// let-binding
(let $p:pat = $e:expr ; $($r:tt)*) => {{
  let $p = $e;
  m!($($r)*)
}};
// const-bind
(_ <- $x:expr ; $($r:tt)*) => {
  $x.bind(move |_| { m!($($r)*) })
};
// bind
($binding:ident <- $x:expr ; $($r:tt)*) => {
  $x.bind(move |$binding| { m!($($r)*) })
};
// const-bind
($e:expr ; $($a:tt)*) => {
    $e.bind(move |_| m!($($a)*))
};
// pure
($a:expr) => {
  $a
}
}
```

m!宏提供了一种类似于Haskell中do语法的语法糖，用于简化对单子（Monad）的操作，使链式调用更加直观。这种宏可以帮助我们以接近命令式的风格编写代码，而实际上是通过链式绑定操作来完成单子的组合。

### do作用

在Haskell的do语法中，可以使用一种类似"赋值"的语法，将单子操作逐步组合起来。这个宏m!模仿了do语法，将Rust中单子的bind操作包装成多种模式来实现类似的效果，具体功能如下：

    支持let绑定：可以在do块中执行let绑定语句，以便存储临时结果。
    支持常量绑定（忽略结果）：可以在单子操作中忽略返回值，仅保留执行的副作用。
    支持单子绑定：从单子中提取值，并将其传递到后续操作中。
    支持返回纯值：在操作链的最后可以返回一个纯值，而不是一个绑定的单子。

## Monoid

单位元，类似于数学概念中的具有一个结合律的二元操作和一个单位元。

``` rust
/// In abstract algebra, a `Monoid` is a set equipped with an associative binary operation and an identity element.
/// In category theory, a `Monoid` is a "single object category" equipped with two morphisms:
/// - μ: M ⊗ M → M called multiplication (a.k.a the associative operation of the `Semigroup`)
/// - η: I → M called unit (a.k.a the `mempty` defined in this trait)
pub trait Monoid: Semigroup {
    /// The identity element/morphism of the monoid
    fn mempty() -> Self;
}

impl Monoid for String {
    fn mempty() -> Self {
        String::from("")
    }
}

impl Monoid for i32 {
    fn mempty() -> Self {
        0
    }
}
```

## Apply

Functor的扩展，提供了一种"在上下文中"应用函数的方式。该trait允许在容器类型（如Option、Result等）中将一个函数应用到另一个值上，从而提供了更丰富的操作语义。这种结构通常被称为"Applicative"，在函数式编程中用于将多个上下文包装的值组合起来。

``` rust
/// An extension of `Functor`, `Apply` provides a way to _apply_ arguments
/// to functions when both are wrapped in the same kind of container. This can be
/// seen as running function application "in a context".
///
/// For a nice, illustrated introduction,
/// see [Functors, Applicatives, And Monads In Pictures](http://adit.io/posts/2013-04-17-functors,_applicatives,_and_monads_in_pictures.html).
///
pub trait Apply<'a>: Functor<'a> {
    /// Apply a function wrapped in a context to to a value wrapped in the same type of context
    fn ap<F, B: 'a>(self, f: Self::Wrapped<F>) -> Self::Wrapped<B>
    where
        F: FnOnce(Self::Unwrapped) -> B + 'a;

    /// Lift an (unwrapped) binary function and apply to two wrapped values
    fn lift_a2<F, B: 'a, C: 'a>(self, b: Self::Wrapped<B>, f: F) -> Self::Wrapped<C>
    where
        F: FnOnce(Self::Unwrapped, B) -> C + 'a;

    // Since Rust doesnt'have (auto)currying, we are forced to manually implement
    // lift_a3, lift_a4, etc.

    // But in Rust we don't neet it, since lift is baked into the language via '?'
    // let a = self?;
    // let b = b?;
    // Some(f(a, b))
}
```

对Option的实现：

``` rust
impl<'a, A> Apply<'a> for Option<A> {
    fn ap<F, B: 'a>(self, f: Self::Wrapped<F>) -> Self::Wrapped<B>
    where
        F: FnOnce(Self::Unwrapped) -> B + 'a,
    {
        match self {
            Some(x) => match f {
                Some(z) => Some(z(x)),
                None => None,
            },
            None => None,
        }

        // self.and_then(|x| f.fmap(|z| z(x)))
    }

    fn lift_a2<F, B: 'a, C: 'a>(self, b: Self::Wrapped<B>, f: F) -> Self::Wrapped<C>
    where
        F: FnOnce(Self::Unwrapped, B) -> C,
    {
        match self {
            Some(x) => match b {
                Some(y) => Some(f(x, y)),
                None => None,
            },
            None => None,
        }

        // self.and_then(|a_u| b.map(|b_u| f(a_u, b_u)))
    }
}
```

ap方法接受一个包含函数的容器f，并将它应用到self这个包含值的容器中。

ap会"解包"函数和参数，然后在它们的上下文中应用函数。例如，对于两个Option类型的值Some(2)和Some(\|x\|
x + 1)，调用ap会返回Some(3)。

## Applicative

对Apply的扩展。

``` rust
pub trait Applicative<'a>: Apply<'a> {
    /// Lift a value into a context
    fn of(value: Self::Unwrapped) -> Self::Wrapped<Self::Unwrapped>;
}
```

从value解析为当前类型。

``` rust
impl<'a, A: 'a + Clone> Applicative<'a> for Option<A> {
    fn of(value: Self::Unwrapped) -> Self::Wrapped<Self::Unwrapped> {
        Some(value)
    }
}
```

## SemiGroup

Semigroup是一个具有结合性操作的代数结构。这个trait要求实现一个结合性操作mappend，它接受两个值并将它们组合在一起。
结合律：mappend必须满足结合律，即对于任意a、b和c，都有a.mappend(b).mappend(c)
== a.mappend(b.mappend(c))。

``` rust
pub trait Semigroup {
    /// The associative operation of the `Semigroup` which takes two values and them together
    fn mappend(self, other: Self) -> Self;
}

impl Semigroup for String {
    fn mappend(self, other: Self) -> Self {
        self + &other
    }
}
impl<A: Monoid> Semigroup for Option<A> {
    fn mappend(self, other: Self) -> Self {
        self.and_then(|v| other.map(|v2| v.mappend(v2)))
    }
}
```

这样就为Option提供了结合性操作。

## Semigroupoid

``` rust
pub trait Semigroupoid {
    /// Take two morphisms and return their composition "the math way".
    /// That is, `(b -> c) -> (a -> b) -> (a -> c)`.
    fn compose(self, other: Self) -> Self;
}

/// Morphisms in Rust type system are normal functions: `Fn/FnMut/FnOnce`,
/// but due to limitations of type system there is no way to implement the `Semigroupoid` trait.
/// Let's stick with this generic `compose` function
#[allow(dead_code)]
pub fn compose<A, B, C, G, F>(f: F, g: G) -> impl Fn(A) -> C
where
    F: Fn(A) -> B,
    G: Fn(B) -> C,
{
    move |x| g(f(x))
}

impl<A> Semigroupoid for Option<A>
where
    A: Semigroup + Clone,
{
    fn compose(self, other: Self) -> Self {
        self.lift_a2(other, |a, b| a.mappend(b))
    }
}
```

Semigroupoid是一个代数结构，要求类型具备组合两个映射（函数）或"态射"（morphisms）的能力。

compose方法：该方法接受两个映射（或函数），并返回它们的组合。具体来说，它会返回一个新的映射，表示(b
-\> c) -\> (a -\> b) -\> (a -\>
c)，即将第一个函数和第二个函数组合起来，使得它可以直接作用于a并返回c。

在数学中，组合两个映射就是通过将一个函数的输出传递给另一个函数。例如，给定两个函数f:
A -\> B和g: B -\> C，组合后会得到g(f(x))，一个从A到C的映射。

Option`<A>`{=html}的Semigroupoid实现：Option`<A>`{=html}被实现了Semigroupoid，即它支持将两个Option类型的值进行组合操作。
要求：A类型必须实现Semigroup，这意味着A有一个mappend操作可以将两个值结合。
实现：

lift_a2方法用来将两个Option包装的值和一个二元函数进行组合。在此实现中，\|a,
b\| a.mappend(b)是二元函数，它会将两个A类型的值进行mappend操作。

如果self或other为None，则组合结果为None，因为Option是一个容器类型，组合的结果也应该是一个容器。

## State

State类型及其实现，模拟了具有隐式状态的纯函数式编程中的State
monad（状态单子）。它展示了如何在Rust中实现和使用State
monad，同时为它实现了Functor、Apply、Applicative和Monad四个类型的trait。

``` rust
pub struct State<'a, S, A> {
    /// The (apparently) "stateful" function
    pub runner: Box<dyn 'a + FnOnce(S) -> (A, S)>,
}
impl<'a, S: 'a, A: 'a> State<'a, S, A> {
    /// Constructs a new `State` by passing in the function which uses and updates the state
    pub fn new<F>(runner: F) -> Self
    where
        F: FnOnce(S) -> (A, S) + 'a,
    {
        Self {
            runner: Box::new(runner),
        }
    }

    /// Run a `State` by passing in some initial state to actualy run the enclosed
    /// state runner.
    pub fn execute(self, state: S) -> (A, S) {
        (self.runner)(state)
    }
}
```

**execute**方法接受一个初始状态，并通过runner函数执行状态变换，返回值(A,
S)，其中A是计算结果，S是更新后的状态。

以Functor为例：

``` rust
impl<'a, S: 'a, A: 'a> Functor<'a> for State<'a, S, A> {
    type Unwrapped = A;

    type Wrapped<B: 'a> = State<'a, S, B>;

    fn fmap<F, B: 'a>(self, f: F) -> Self::Wrapped<B>
    where
        F: FnOnce(Self::Unwrapped) -> B + 'a,
    {
        State {
            runner: Box::new(move |s| {
                let (a1, s1) = (self.runner)(s);
                (f(a1), s1)
            }),
        }
    }
}
```

State类型实现了Functor，允许通过fmap方法对状态的计算结果进行变换。

fmap接受一个函数f，它将状态计算结果A变换为B，并返回一个新的State实例，保持原有的状态。

## Fold

没什么好说的。

``` rust
pub trait Foldable {
    // The internal type of the `Foldable` which will be wrapped over
    type Unwrapped: Monoid;

    /// Right-associative fold over a structure to alter and/or reduce
    /// it to a single summary value. The right-association makes it possible to
    /// cease computation on infinite streams of data.
    ///
    /// The folder must be a binary function, with the second argument being the
    /// accumulated value thus far.
    fn foldr<B: Monoid, F>(self, init: B, folder: F) -> B
    where
        F: Fn(B, &Self::Unwrapped) -> B;
}

impl<A: Monoid> Foldable for Vec<A> {
    type Unwrapped = A;

    fn foldr<B, F>(self, init: B, folder: F) -> B
    where
        F: Fn(B, &Self::Unwrapped) -> B,
    {
        self.iter().rfold(init, folder)
    }
}
```

## Free

实现了一个Free
monad的定义，结合了FunctorOnce和Monad的特性，允许构建一个自由变换（free
transformation）的计算结构。这种结构广泛应用于处理"效果"或"命令"的语言，尤其是在函数式编程中，可以用来表示延迟执行、计算的组合或解释。

Free是一个枚举类型，表示一个自由单子。它有两种变体：

    Pure(A)：表示一个纯值（A），它是自由单子的基础。
    Free(Box<F::Wrapped<Free<'a, F, A>>>)：表示一个嵌套的自由单子，它包装了一个F::Wrapped<Free<'a, F, A>>，即F的包裹类型的自由单子。这允许构造更复杂的计算和表达式。

F是一个泛型，要求它实现FunctorOnce\<\'a\>，这表示F是一个可应用的容器，并且能够将函数映射到其内部的值上。FunctorOnce的fmap操作允许将一个函数作用于其内的值，并将结果返回。

A是自由单子的返回类型。

``` rust
pub enum Free<'a, F, A: 'a>
where
    F: FunctorOnce<'a> + 'a,
{
    Pure(A),
    Free(Box<F::Wrapped<Free<'a, F, A>>>),
}

impl<'a, F, A> FunctorOnce<'a> for Free<'a, F, A>
where
    F: FunctorOnce<'a> + 'a,
{
    type Unwrapped = A;
    type Wrapped<B: 'a> = Free<'a, F::Wrapped<Free<'a, F, A>>, B>;

    fn fmap<G, B: 'a>(self, f: G) -> Self::Wrapped<B>
    where
        G: FnOnce(Self::Unwrapped) -> B + 'a,
    {
        match self {
            Free::Pure(a) => Free::Pure(f(a)),
            Free::Free(b) => {
                // Free (fmap g <$> fx)
                Free::Free(Box::new((*b).fmap(move |a| a.fmap(f))))
            }
        }
    }
}

impl<'a, F, A> Monad<'a> for Free<'a, F, A>
where
    F: FunctorOnce<'a> + 'a,
{
    type Unwrapped = A;
    type Wrapped<T: 'a> = Free<'a, F::Wrapped<Free<'a, F, A>>, T>;

    fn bind<E, B: 'a>(self, f: E) -> Self::Wrapped<B>
    where
        E: FnOnce(Self::Unwrapped) -> Self::Wrapped<B> + 'a,
    {
        // Pure a >>= f = f a
        // Free m >>= f = Free ((>>= f) <$> m)
        match self {
            Free::Pure(a) => f(a),
            Free::Free(m) => Free::Free(Box::new((*m).fmap(|a| a.bind(f)))),
        }
    }

    fn of<T: 'a>(value: T) -> Self::Wrapped<T> {
        Free::Pure(value)
    }
}

#[allow(unused)]
pub fn lift_f<'a, F, A>(command: F) -> Free<'a, F, A>
where
    F: FunctorOnce<'a, Unwrapped = A>,
{
    // Free (fmap Pure command)
    Free::Free(Box::new(command.fmap(|a| Free::Pure(a))))
}
```

Free
monad：通过自由单子的结构，可以延迟执行计算，并通过fmap和bind操作构建复杂的计算链。自由单子广泛应用于实现DSL（领域特定语言），它能够在纯函数式编程中模拟对状态、IO等副作用的处理。
FunctorOnce 和 Monad
实现：Free类型实现了这两个trait，使得它可以像常规的Functor和Monad一样进行操作，支持映射、绑定和提升等操作。
lift_f：提供了一种将普通的FunctorOnce命令提升为自由单子的机制，使得代码可以灵活地在自由单子和普通命令之间转换。

## Traverse

对结构中所有的元素执行操作。

``` rust
pub trait Traversable<'a>: Functor<'a> {
    /// Convert elements to actions, then evaluate the actions from left-to-right
    /// and collect the results.
    ///
    /// Haskell signature
    /// traverse  :: Applicative f => (a -> f b) -> t a -> f (t b)
    fn traverse<F, B: 'a, W>(self, f: F) -> W::Wrapped<Self::Wrapped<B>>
    where
        F: Fn(&Self::Unwrapped) -> W::Wrapped<B>,
        W: Applicative<'a, Unwrapped = Self::Wrapped<B>, Wrapped<Self::Wrapped<B>> = W>
            + 'a
            + Monoid,
        <W as Functor<'a>>::Wrapped<Self::Wrapped<B>>: Applicative<'a>,
        <Self as Functor<'a>>::Wrapped<B>: 'a;

    /// Evaluate each action in the structure from left to right, and collect the results
    ///
    /// Haskell signature
    /// sequenceA :: Applicative f => t (f a) -> f (t a)
    fn sequence_a<B: 'a, W>(self) -> W::Wrapped<Self::Wrapped<B>>
    where
        W: Applicative<'a, Unwrapped = Self::Wrapped<B>, Wrapped<Self::Wrapped<B>> = W>
            + 'a
            + Monoid,
        W: Applicative<'a, Wrapped<B> = Self::Unwrapped>,
        <Self as Functor<'a>>::Wrapped<B>: 'a,
        Self::Unwrapped: Applicative<'a> + 'a + Copy;
}

impl<'a, A: Monoid> Traversable<'a> for Vec<A> {
    fn traverse<F, B: 'a, W>(self, f: F) -> W::Wrapped<Self::Wrapped<B>>
    where
        F: Fn(&Self::Unwrapped) -> W::Wrapped<B>,
        W: Applicative<'a, Unwrapped = Self::Wrapped<B>, Wrapped<Self::Wrapped<B>> = W>
            + 'a
            + Monoid,
        <W as Functor<'a>>::Wrapped<Self::Wrapped<B>>: Applicative<'a> + Monoid,
        <Self as Functor<'a>>::Wrapped<B>: 'a,
    {
        self.foldr(W::of(vec![]), |k, v| {
            let c = f(v);

            k.lift_a2(c, |mut acc, v: B| {
                acc.insert(0, v);
                acc
            })
        })
    }

    fn sequence_a<B: 'a, W>(self) -> W::Wrapped<Self::Wrapped<B>>
    where
        W: Applicative<'a, Unwrapped = Self::Wrapped<B>, Wrapped<Self::Wrapped<B>> = W>
            + 'a
            + Monoid,
        <Self as Functor<'a>>::Wrapped<B>: 'a,
        W: Applicative<'a, Wrapped<B> = Self::Unwrapped>,
        Self::Unwrapped: Applicative<'a> + 'a + Copy,
    {
        self.traverse::<_, _, W>(|a| *a)
    }
}
```
