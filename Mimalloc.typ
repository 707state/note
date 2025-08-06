#set text(font: "DejaVu Sans Mono", size: 12pt)
#set page(paper: "a4", margin: (x: auto, y: auto))
= Mimalloc内存分配

== 在Mimalloc页中进行的Free List的Sharding
在mimalloc中，其通过将内存分页，每个页负责一个特定大小的内存的分配，每个页有一个Free List，因此内存分配的空间局部性较好。

== Local Free List 
mimalloc希望能够限制内存分配与内存释放在最差情况下的时间消耗，如果上层应用需要释放一个非常大的结构体，其可能需要递归的释放一些子结构体，因而可能带来非常大的时间消耗。因而在koka与lean语言中，其运行时系统使用引用计数来追踪各种数据，然后保留一个延迟释放的队列(deferred decrement list)，当每次释放的内存块超过一定数量后就将剩余需要释放的内存块加入该队列，在之后需要释放的时候再进行释放。那么下一个需要确定的问题是什么时候再去从该队列中释放内存。从延迟释放队列中继续进行释放的时机最好应当是内存分配器需要更多空间的时候，因此这需要内存分配器与上层应用的协作。

在mimalloc中，其提供一个回调函数，当进行了一定次数内存的分配与释放后会主动调用该回调函数来通知上层应用。mimalloc在实现时检测当前进行内存分配的页的Free List是否为空，如果为空则调用该回调，但是为了避免用于一直不断的分配与释放内存，导致Free List一直不为空，而导致回调函数一直得不到回调。因此mimalloc将Free List第二次进行Sharding，将其分为Free List与Local Free List。

在mimalloc中，其提供一个回调函数，当进行了一定次数内存的分配与释放后会主动调用该回调函数来通知上层应用。mimalloc在实现时检测当前进行内存分配的页的Free List是否为空，如果为空则调用该回调，但是为了避免用于一直不断的分配与释放内存，导致Free List一直不为空，而导致回调函数一直得不到回调。因此mimalloc将Free List第二次进行Sharding，将其分为Free List与Local Free List。

== Thread Free List 
在mimalloc中，其提供一个回调函数，当进行了一定次数内存的分配与释放后会主动调用该回调函数来通知上层应用。mimalloc在实现时检测当前进行内存分配的页的Free List是否为空，如果为空则调用该回调，但是为了避免用于一直不断的分配与释放内存，导致Free List一直不为空，而导致回调函数一直得不到回调。因此mimalloc将Free List第二次进行Sharding，将其分为Free List与Local Free List。

== Full List 
由于在mimalloc中每个堆中都有一个数组pages，该数组中每个元素都是一个由相应大小的页组成的队列；同时还有一个pages_direct的数组，该数组中每个元素对应一个内存块的大小类型，每个元素均为指向负责对应大小内存块分配的页的指针。因此mimalloc在进行内存分配时会首先从该数组指向的页中尝试进行分配，如果分配失败则调用malloc_generic，在该函数中会遍历pages数组中对应大小的队列，此时如果对应的队列中有很多页均是满的，且队列很长那么每次分配的时候都会进行队列的遍历，导致性能的损失。

因此mimalloc构建了一个Full List，将所有已经没有空闲空间的页放入该队列中，仅当该页中有一些空闲空间被释放后才会将其放回pages对应的队列中。而在由于内存的释放可能由对应堆的拥有者线程进行也可能由其他线程进行，因此需要一定的方式提醒对应的堆该页已经有空闲块了，同时为了避免使用锁导致的开销，mimalloc通过加入一个Thread Delayed Free List，如果一个页处于Full List中，那么在释放时会将内存块加入Thread Delayed Free List中，该队列会在调用malloc_generic时进行检测与清除(由于时Thread Local的堆，因此仅可能是拥有者来进行)，因此此时仅需通过原子操作即可完成。那么还有一个问题是当释放内存的时候，其他线程如何知道是将内存块加入Thread Free List中还是Thread Delayed Free List中。mimalloc通过设置NORMAL、DELAYED、DELAYING三种状态来完成该操作。

== Mimalloc源码
堆的定义
```c
// A heap owns a set of pages.
struct mi_heap_s {
  mi_tld_t*             tld;
  _Atomic(mi_block_t*)  thread_delayed_free;
  mi_threadid_t         thread_id;                           // thread this heap belongs too
  mi_arena_id_t         arena_id;                            // arena id if the heap belongs to a specific arena (or 0)
  uintptr_t             cookie;                              // random cookie to verify pointers (see `_mi_ptr_cookie`)
  uintptr_t             keys[2];                             // two random keys used to encode the `thread_delayed_free` list
  mi_random_ctx_t       random;                              // random number context used for secure allocation
  size_t                page_count;                          // total number of pages in the `pages` queues.
  size_t                page_retired_min;                    // smallest retired index (retired pages are fully free, but still in the page queues)
  size_t                page_retired_max;                    // largest retired index into the `pages` array.
  mi_heap_t*            next;                                // list of heaps per thread
  bool                  no_reclaim;                          // `true` if this heap should not reclaim abandoned pages
  uint8_t               tag;                                 // custom tag, can be used for separating heaps based on the object types
  mi_page_t*            pages_free_direct[MI_PAGES_DIRECT];  // optimize: array where every entry points a page with possibly free blocks in the corresponding queue for that size.
  mi_page_queue_t       pages[MI_BIN_FULL + 1];              // queue of pages for each size class (or "bin")
};
```
在mimalloc中，每个线程都有一个Thread Local的堆，每个线程在进行内存的分配时均从该线程对应的堆上进行分配。在一个堆中会有一个或多个segment，一个segment会对应一个或多个页，而内存的分配就是在这些页上进行。mimalloc将页分为三类：

    small类型的segment的大小为4M，其负责分配大小小于MI_SMALL_SIZE_MAX的内存块，该segment中一个页的大小均为64KB，因此在一个segment中会包含多个页，每个页中会有多个块
    large类型的segment的大小为4M，其负责分配大小处于MI_SMALL_SIZE_MAX与MI_LARGE_SIZE_MAX之间的内存块，该segment中仅会有一个页，该页占据该segment的剩余所有空间，该页中会有多个块
    huge类型的segment，该类segment的负责分配大小大于MI_LARGE_SIZE_MAX的内存块，该类segment的大小取决于需要分配的内存的大小，该segment中也仅包含一个页，该页中仅会有一个块


根据heap的定义我们可以看到其有pages_free_direct数组、pages数组、Thread Delayed Free List以及一些元信息。其中pages_free_direct数组中每个元素对应一个内存块大小的类别，其内容为一个指针，指向一个负责分配对应大小内存块的页，mimalloc在分配比较小的内存时可以通过该数组直接找到对应的页，然后试图从该页上分配内存，从而提升效率。pages数组中每个元素为一个队列，该队列中所有的页大小均相同，这些页可能来自不同的segment，其中数组的最后一个元素(即pages[MI_BIN_FULL])就是前文提到的Full List，倒数第二个元素(即pages[MIN_BIN_HUGE])包含了所有的huge类型的页。thread_delayed_free就是前文提到的Thread Delayed Free List，用来让线程的拥有者能够将页面从Full List中移除。

```c
// Thread local data
struct mi_tld_s {
  unsigned long long  heartbeat;     // monotonic heartbeat count
  bool                recurse;       // true if deferred was called; used to prevent infinite recursion.
  mi_heap_t*          heap_backing;  // backing heap of this thread (cannot be deleted)
  mi_heap_t*          heaps;         // list of heaps in this thread (so we can abandon all when the thread terminates)
  mi_segments_tld_t   segments;      // segment tld
  mi_os_tld_t         os;            // os tld
  mi_stats_t          stats;         // statistics
};
```
在heap的定义中我们需要特别注意的一个成员是tld(即Thread Local Data)。其成员包括指向对应堆的heap_backing，以及用于segment分配的segment tld以及os tld。


\_mi_malloc_generic的流程可以归纳为：

    如果需要的话进行全局数据/线程相关的数据/堆的初始化
    调用回调函数(即实现前文所说的deferred free)
    找到或分配新的页
    从页中分配内存

=== 初始化
前面我们提到过每个线程都有一个Thread Local的堆，该堆默认被设为\_mi_heap_empty。如果调用\_mi_malloc_generic时发现该线程的堆为\_mi_heap_empty则进行初始化。mi_thread_init会首先调用mi_process_init来进行进程相关数据的初始化，之后初始化Thread Local的堆。

在mimalloc中，如果一个线程结束了，那么其对应的Thread Local的堆就可以释放了，但是在该堆中还可能存在有一些内存块正在被使用，且此时会将对应的segment设置为ABANDON，之后由其他线程来获取该segment，之后利用该segment进行对应的内存分配与释放(mimalloc也有一个no_reclaim的选项，设置了该选项的堆不会主动获取其他线程ABANDON的segment)。

=== Huge类型页面的分配
由于huge类型的页面对应的segment中仅有一个页，且该页仅能分配一个块，因此其会重新分配一个segment，从中建立新的页面。mi_huge_page_alloc会调用mi_page_fresh_alloc分配一个页面，然后将其插入堆对应的BIN中(即heap->pages[MI_BIN_HUGE])。由下图可以看到Small与Large类型页面分配时所调用的mi_find_free_page也会调用该函数来进行页面的分配，接下来我们就介绍一下mi_page_fresh_alloc。
