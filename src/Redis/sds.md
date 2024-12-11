# sds的实现细节

## sdsnewlen

这里是创建一个长度为initlen的sds, 具体实现在_sdsnewlen中。

核心逻辑：

1. 参数解析
	•	s：当前的 SDS 字符串。
	•	size：调整后的新分配大小。
	•	would_regrow：是否可能在未来扩展字符串的标志。
	•	如果为真（1），则避免使用类型 SDS_TYPE_5，因为该类型是只读且不适合频繁扩展。

2. 判断是否需要调整

```c
if (sdsalloc(s) == size) return s;
```

	•	如果当前分配大小（sdsalloc(s)）已经等于目标大小，则无需调整，直接返回。

3. 截断长度（收缩时）

```c
if (size < len) len = size;
```

	•	如果新分配大小小于当前内容长度，则将字符串的逻辑长度截断为 size。

4. 确定新的 SDS 类型

```c
type = sdsReqType(size);
if (would_regrow && type == SDS_TYPE_5) type = SDS_TYPE_8;
hdrlen = sdsHdrSize(type);
```


	•	根据新大小确定最小适配的 SDS 类型（SDS_TYPE_5 到 SDS_TYPE_64）。
	•	如果未来可能增长（would_regrow 为真），避免使用 SDS_TYPE_5，改用 SDS_TYPE_8。

5. 判断是否需要重新分配内存

```c
int use_realloc = (oldtype == type || (type < oldtype && type > SDS_TYPE_8));
size_t newlen = use_realloc ? oldhdrlen + size + 1 : hdrlen + size + 1;
```

	•	当以下任一条件满足时，使用 realloc 优化内存操作：
	1.	新的 SDS 类型与旧的相同（不需要调整头部大小）。
	2.	新的 SDS 类型比旧的更小，但不是 SDS_TYPE_5。
	•	如果类型发生较大变化（例如从 SDS_TYPE_32 到 SDS_TYPE_8），则重新分配内存并复制数据。

6. 执行内存调整

使用 realloc 调整（优化路径）：

```c
if (use_realloc) {
    #if defined(USE_JEMALLOC)
        alloc_already_optimal = (je_nallocx(newlen, 0) == zmalloc_size(sh));
    #endif
    if (!alloc_already_optimal) {
        newsh = s_realloc(sh, newlen);
        if (newsh == NULL) return NULL;
        s = (char*)newsh + oldhdrlen;
    }
}
```

	•	如果使用 jemalloc，通过 je_nallocx 确认是否需要重新分配内存。
	•	如果需要，则调用 s_realloc 进行调整。

手动分配新内存并复制数据：

```c
else {
    newsh = s_malloc(newlen);
    if (newsh == NULL) return NULL;
    memcpy((char*)newsh + hdrlen, s, len);
    s_free(sh);
    s = (char*)newsh + hdrlen;
    s[-1] = type;
}
```

	•	为新类型分配合适的内存空间。
	•	复制原有数据到新分配的内存。
	•	更新 SDS 的类型标识。

7. 更新元数据

```c
s[len] = 0;
sdssetlen(s, len);
sdssetalloc(s, size);
```

	•	确保字符串以 \0 结尾。
	•	更新字符串长度（len）和分配大小（alloc）。


函数整体流程
	1.	判断是否需要调整内存大小。
	2.	根据新大小确定合适的 SDS 类型。
	3.	如果类型不变或变更很小，尝试直接通过 realloc 调整内存。
	4.	如果类型发生较大变化，手动重新分配内存并复制数据。
	5.	更新字符串长度、分配大小及类型信息。

