<!--toc:start-->
- [Fisher-Yates shuffle](#fisher-yates-shuffle)
  - [C++实现](#c实现)
- [Inside-out Algorithm](#inside-out-algorithm)
  - [C++实现](#c实现)
- [Sattolo\'s algorithm](#sattolos-algorithm)
- [其他Shuffle算法](#其他shuffle算法)
  - [Naive 的方法](#naive-的方法)
<!--toc:end-->

# Fisher-Yates shuffle

又称为洗牌算法。这个算法生成的随机排列是等概率的，并且非常高效。

伪代码表述：

\-- To shuffle an array a of n elements (indices 0..n-1): for i from n−1
downto 1 do j ← random integer such that 0 ≤ j ≤ i exchange a\[j\] and
a\[i\]

## C++实现

``` cpp

template<typename T>
void Shuffle(T& container){
    std::random_device rd;//获取随机数生成器
    std::mt19937 gen(rd());//Mersenne Twister生成器
    //洗牌算法
    for(int i=container.size()-1;i>=0;i--){
        //生成[0,i]范围的随机数
        std::uniform_int_distribution<std::size_t> dis(0,i);
        //交换当前元素和随机索引的元素
        std::swap(container[i],container[dis(gen)]);
    }
}
```

# Inside-out Algorithm

\"inside-out\"算法的核心思想是逐步生成一个随机排列。每次将当前元素放置到一个随机位置，生成的数组会在每一步保证是部分随机排列。

    遍历数组的每个位置 ii：
        生成一个随机索引 jj，其中 j∈[0,i]j∈[0,i]。
        如果 j≠ij=i，则将索引 jj 的元素放入位置 ii。
        将当前元素存放到随机索引 jj。

    最终生成一个完全随机的数组。

## C++实现

``` cpp
// The "inside-out" shuffle algorithm
template <typename Container>
void InsideOutShuffle(Container& container) {
    // Random number generator
    std::random_device rd;    // Seed
    std::mt19937 gen(rd());   // Mersenne Twister PRNG

    // Inside-out algorithm
    for (std::size_t i = 0; i < container.size(); ++i) {
        // Generate a random index from [0, i]
        std::uniform_int_distribution<std::size_t> dis(0, i);
        std::size_t rand_index = dis(gen);

        if (rand_index != i) {
            // Place the current element into the randomly chosen earlier position
            container[i] = container[rand_index];
        }
        // The current position becomes a new unique random element
        container[rand_index] = i;
    }
}
```

# Sattolo\'s algorithm

实现：

``` python
from random import randrange

def sattolo_cycle(items) -> None:
    """Sattolo's algorithm."""
    i = len(items)
    while i > 1:
        i = i - 1
        j = randrange(i)  # 0 <= j <= i-1
        items[j], items[i] = items[i], items[j]
```

# 其他Shuffle算法

## Naive 的方法

``` python
from random import randrange

def naive_shuffle(items) -> None:
    """A naive method. This is an example of what not to do -- use Fisher-Yates instead."""
    n = len(items)
    for i in range(n):
        j = randrange(n)  # 0 <= j <= n-1
        items[j], items[i] = items[i], items[j]
```
