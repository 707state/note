<!--toc:start-->
- [Mysql执行流程](#mysql执行流程)
  - [Server](#server)
  - [Storage Engine](#storage-engine)
  - [连接器](#连接器)
  - [长连接](#长连接)
    - [减少内存占用](#减少内存占用)
  - [查询缓存](#查询缓存)
  - [解析SQL](#解析sql)
    - [预处理阶段](#预处理阶段)
    - [优化阶段](#优化阶段)
    - [执行器](#执行器)
- [数据库文件](#数据库文件)
  - [表空间文件的结构](#表空间文件的结构)
  - [行的格式](#行的格式)
    - [变长字段长度列表](#变长字段长度列表)
      - [为什么变长字段长度列表的信息要按照逆序存放？](#为什么变长字段长度列表的信息要按照逆序存放)
      - [每个数据库表的行格式都有变长字段字节数列表吗？](#每个数据库表的行格式都有变长字段字节数列表吗)
    - [NULL值列表](#null值列表)
      - [数据库的每个行格式都有NULL值列表吗？](#数据库的每个行格式都有null值列表吗)
      - [NULL值列表是固定1字节空间吗？如果是这样的话，一个记录有9个字段值为NULL,这时候怎么表示？](#null值列表是固定1字节空间吗如果是这样的话一个记录有9个字段值为null这时候怎么表示)
    - [记录头信息](#记录头信息)
    - [记录的真实数据](#记录的真实数据)
    - [varchar(n)中n的最大取值](#varcharn中n的最大取值)
    - [行溢出后，MySQL怎么处理？](#行溢出后mysql怎么处理)
<!--toc:end-->

# Mysql执行流程

## Server

建立连接、分析和执行sql。

## Storage Engine

负责数据的存储和提取。

## 连接器

Mysql/Postgres这些数据库都采用Client/Server的架构，因此想要连接数据库都需要客户端进行连接。

## 长连接

Mysql当中与HTTP一样，有长连接，好处是减少了建立/销毁连接的开销，代价是内存占用变大。

### 减少内存占用

1. 定期断开长连接

2. 客户端主动重置连接

## 查询缓存

如果客户端发送的SQL是查询语句，那么Mysql就会先去到缓存当中查询，如果命中就返回，否则就进入到执行器当中。

在Mysql8.0就直接删除了查询缓存。

## 解析SQL

每一条SQL语句的流程是这样的：

1. prepare阶段

2. optimize阶段

3. execute阶段

### 预处理阶段

1. 检查SQL查询语句对应的表是否存在

2. 将select \*中的\*符号拓展

### 优化阶段

确定SQL语句的执行方案。

优化器会基于查询成本的考虑，来决定使用那个索引。

如果没有索引可以选择，那么就会进行全表查询，效率是最低的。

如果有主键索引以及其他类型的索引（二级索引），再去执行时，就会有这样的问题：需要决定使用哪一个索引。

这里有一个知识点：_什么是覆盖索引？_

覆盖索引就是说，索引所拥有的全部列的数据全部都查询出来了，就完全可以用索引来进行查询，从而避免回表的开销。

### 执行器

在执行器当中，有三种方式的执行过程：

1. 主键索引查询

2. 全表扫描

3. 索引下推

假定有一个表Product, 其结构如下：

+-----+-----+-----+
|id   |  product_no   |  name   |
+-----+-----+-----+
| 1    |  0001   | apple    |
+-----+-----+-----+
|  2   |  0002   |   banana  |
+-----+-----+-----+

其中，id被设置为主键索引，name被设置为普通索引。

假如执行这条语句：

```sql
    select id from product where id = 1;
```

这里是主键+等值查询，所以优化器会选择const类型查询，也就是使用主键索引查询一条记录。流程如下：
- 执行器第一次查询，会调用read\_first\_record函数指针指向的函数，因为优化器设置为const类型的查询,这个函数指针就会被设置为innodb引擎索引查询的接口，把条件id=1交给引擎层面，让存储引擎来定位符合记录的第一条记录。

- 存储引擎通过B+树结构定位到第一个记录，如果记录不存在，就会向执行器上报找不到的错误，然后结束查询；否则就返回给执行器。

- 执行器从存储引擎读到记录后，接着判断记录是否符合查询条件，如果符合就会发送给客户端，否则就跳过。

- 如果还要继续查询，就会在调用read_record的接口，但是因为优化器将类型设置为const，所以这个函数指针被指向一个永远返回-1的函数，所以执行到这里就停止了。

如果不存在索引，就会发生全表扫描，优化器选择的类型为ALL。

例如：

```sql
    select * from product where name="apple";
```

如果不在name上创建索引，就不会有覆盖索引，此时的流程是这样的：

- 执行器第一次查询，调用read\_first\_record函数接口，因为查询类型被设置为ALL，所以指针指向的是InnoDB的全扫描的接口，让存储引擎读取其中的第一条记录。

- 执行器会判断读到的这条记录的name是否符合条件，是的话就直接发送到客户端（客户端等待查询完毕才会将结果输出）。
- 之后继续查询，就会调用read\_record函数指针指向的函数，因为优化器选择的访问类型为all，所以read\_record指向的还是InnoDB全扫描的接口，接着向存储引擎曾要求继续读刚才那条记录的下一条记录，存储引擎把下一条记录取出来以后将其返回给执行器，执行器继续处理。
- 知道所有记录读取完毕，然后向执行器返回读取完毕的信息。

这里有一个索引下推的点

索引下推能减少二级索引在查询时的回表操作，提高查询的效率，因为它将Server层部分负责的事情下放到存储引擎层做。
|-----|----|-----|-----|
| id | name | age | reward |
|-----|-----|-----|-----|
| 1 | John | 21 | 1000 |
| 2 | Mike | 33 | 3000 |


假如在上面的表里面，对age和reward字段建立联合索引，并有下面的查询语句：
```sql
select * from t_user where age > 30 and reward = 1000;
```

当联合索引遇到范围查询(>,<)时就会停止匹配，也就是age字段能用联合索引，但是reward字段就不能用到了。

不用索引下推的话，在每一次从二级索引中定位到这条记录都会获取n主键值，然后进行回表操作，将完整的记录返回给Server层，然后再Server层判断记录是否等于1000，如果满足就发送给客户端，否则跳过。

也就是说，*在没有索引下推时，每查询到一条二级索引记录，都要进行回表操作，然后将记录返回给Server，由Server判断该记录的rewardo是否等于1000*

在使用了索引下推之后，判断记录的reward是否为1000的工作交给了存储引擎层，过程如下：
Server层调用存储引擎接口找到满足查询条件的第一条二级索引记录，然后先不执行回表操作二十先判断该索引包含的列(reward列)的条件i是否成立。如果条件不成立，则直接跳过二级索引；如果成立，再把结果返回给Server层。

可以看到，*在使用了索引下推之后，虽然reward列不能使用联合索引，但是因为他在联合索引内，所以直接在存储引擎过滤满足reward=1000的记录后才执行回表操作获取整个记录。比起没有索引下推，节省了很多回表操作*

# 数据库文件

## 表空间文件的结构

表由段(segment)、区(extent)、页(page)、行(row)组成。

InnoDB的逻辑存储结构如下：

![InnoDB存储结构](../image/InnoDB.png)

1. 行

所有记录都是按行来存放的，每行记录根据不同的行格式，有不同的存储结构。

2. 页

记录是按照行存放的，但是数据库的读取是按照页来读写的，每个页的默认大小为16KB，也就是最多保证16kb的连续存储空间。

页是InnoDB管理内存的最小单元，也就是说每次数据库读写都是以16kb为单位的，一次最少从磁盘中读取16kb的内容到内存中，一次最少把内存的16kb内容刷新到磁盘中。

3. 区

InnoDB中的数据组织方式是B+树，B+树每一层都通过双向链表连接起来，如果是以页为单位来分配存储空间，那么链表相邻的两个页之间的物理位置并不是连续的，那么磁盘查询时就会有大量的随机分配IO，带来非常大的延迟。

怎么解决呢？*在表中数据量大的时候，为某个索引分配空间的时候就不再按照页为分配单位了，而是按照区为单位分配，每个区大小为1MB，对于16kb的页就是连续的64个页划为一个区，这样就使得相邻的页的物理位置也相邻，能够使用顺序IO*

4. 段

表空间是由多个段组成的，段是由多个区组成的，段一般分为数据段、索引段、回滚段等。

索引段：存放B+树的非叶子节点的区的集合；

数据段：存放B+树的叶子节点的区的集合；

回滚段：存放的是回滚数据的区的集合，这里与MVCC机制有关。

## 行的格式
InnoDB中有四种行：Redundant, Compact, Dynamic和Compressed。

Compact行格式：

| 变长字段长度 | NULL值列表 | 记录头信息 | row\_id | trx\_id | roll\_ptr | 列1值 | 列2值 | 列n值 |
| ------------ | ---------- | ---------- | ------- | ------- | --------- | ----- | ----- | ----- |

前三个是记录的额外信息，后面的是记录的真实数据。

### 变长字段长度列表

varchar(n) 和 char(n) 的区别是什么，char 是定长的，varchar 是变长的，变长字段实际存储的数据的长度（大小）不固定的。

在变长字段长度列表里面，读取数据的时候根据这个列表去读取对应长度的数据，其他的TEXT, BLOB这样的变长字段也是这样的实现的。

变长字段在真实数据占用的字节数会按照列的顺序逆序存放。

#### 为什么变长字段长度列表的信息要按照逆序存放？

因为记录头信息中指向下一个记录的指针指向的是下一条记录的记录头信息和真实数据之间的位置，这样的号炊具是向左读就是记录头信息，向右读就是真实数据。

为什么要逆序存放呢？这样就可以使位置靠前的记录的真实数据和数据对应的字段长度信息可以同时在一个CPU Cache Line中，可以提高CPU Cache的命中率。
#### 每个数据库表的行格式都有变长字段字节数列表吗？
当数据表没有变长字段的时候，比如全部都是 int 类型的字段，这时候表里的行格式就不会有「变长字段长度列表」了

### NULL值列表

表中的某些列可能会存储 NULL 值，如果把这些 NULL 值都放到记录的真实数据中会比较浪费空间，所以 Compact 行格式把这些值为 NULL 的列存储到 NULL值列表中。

如果存在允许 NULL 值的列，则每个列对应一个二进制位（bit），二进制位按照列的顺序逆序排列。
- 二进制位的值为1是，代表该列的值为NULL。
- 0时，表示不为NULL。

另外，NULL 值列表必须用整数个字节的位表示（1字节8位），如果使用的二进制位个数不足整数个字节，则在字节的高位补 0。
#### 数据库的每个行格式都有NULL值列表吗？

也不是必须的。

当数据库的字段都定义成NOT NULL的时候，表里面的行格式就不会用NULL值列表。

所以在设计数据库表的时候，通常都是建议将字段设置为  NOT NULL，这样可以至少节省 1 字节的空间（NULL 值列表至少占用 1 字节空间）。

#### NULL值列表是固定1字节空间吗？如果是这样的话，一个记录有9个字段值为NULL,这时候怎么表示？

NULL值列表不是固定1字节的。当一个记录9个字段值都为NULL, 就会创建2字节空间的NULL值列表。

### 记录头信息

重点：
1. delete_mask: 标识此条数据是否被删除。
2. next_record: 下一条记录的位置。
3. record_type: 当前记录的类型。


### 记录的真实数据

记录真实数据的字段还有三个隐藏字段：row\_id, trx\_id, roll\_pointer。
| row\_id | trx\_id | roll\_ptr | column 1 value | column 2 value | column n value |
| --- | --- | --- | --- | --- | --- |

如果建表时指定主键/唯一约束，就不会有row\_id隐藏字段了。

trx\_id是用于处理事务的，占用6个字节。

roll\_pointer 用于记录上一个版本的指针，占用7个字节，必需。
### varchar(n)中n的最大取值

MySQL规定了TEXT、BLOB这种大对象类型之外，其他所有的列（不包含隐藏列和记录头信息）占用的字节长度加起来不能超过65535字节。

就是说，一行记录除了TEXT、BLOBs的列，限制最大为65535个字节（一行，不是一列）。

varchar(n)中的n代表的是最多存储的字符数量，不是字节大小。

要计算最大能存储的字节数，还需要看字符集。如果是ASCII字符集，一个字符占用一个字节，那varchar(100)意味着最大能允许存储100字节的数据。

### 行溢出后，MySQL怎么处理？

如果一个数据页存不了一条记录，InnoDB存储引擎会自动把溢出的数据存放到移除页中。

Compact行格式针对行溢出的处理：当行溢出发生时，在记录的真实数据处只会保存该列的一部分数据，而把剩余的数据放在溢出页中，然后真实数据处用20字节存储指向溢出页的地址，从而可以找到剩余数据所在的页。

Compressed 和 Dynamic 这两种格式采用完全的行溢出方式，记录的真实数据处不会存储该列的一部分数据，只存储 20 个字节的指针来指向溢出页。而实际的数据都存储在溢出页中。
