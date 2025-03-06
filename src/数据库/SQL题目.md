
<!--toc:start-->
- [1581 进店但是从未进行过交易的顾客](#1581-进店但是从未进行过交易的顾客)
- [197 上升的温度](#197-上升的温度)
- [1661 每台机器的平均运行时间](#1661-每台机器的平均运行时间)
- [1280 学生们参加各科测试的次数](#1280-学生们参加各科测试的次数)
- [570 至少有5名直接下属的经理](#570-至少有5名直接下属的经理)
- [1934 确认率](#1934-确认率)
- [1251 平均售价](#1251-平均售价)
- [1211 查询结果的质量和占比](#1211-查询结果的质量和占比)
- [1193 每月交易Ⅰ](#1193-每月交易ⅰ)
<!--toc:end-->

# 1581 进店但是从未进行过交易的顾客

表：Visits

+-------------+---------+
| Column Name | Type    |
+-------------+---------+
| visit_id    | int     |
| customer_id | int     |
+-------------+---------+
visit_id 是该表中具有唯一值的列。
该表包含有关光临过购物中心的顾客的信息。



表：Transactions

+----------------+---------+
| Column Name    | Type    |
+----------------+---------+
| transaction_id | int     |
| visit_id       | int     |
| amount         | int     |
+----------------+---------+
transaction_id 是该表中具有唯一值的列。
此表包含 visit_id 期间进行的交易的信息。



有一些顾客可能光顾了购物中心但没有进行交易。请你编写一个解决方案，来查找这些顾客的 ID ，以及他们只光顾不交易的次数。

返回以 任何顺序 排序的结果表。

<details>

这道题目考验读题，Left Join之后就得到了transaction_id为空的就是要求的。

```sql
select customer_id,count(customer_id) count_no_trans
FROM Visits v
LEFT JOIN transactions t
ON v.visit_id=t.visit_id
WHERE transaction_id IS NULL
GROUP BY customer_id;
```

</details>

# 197 上升的温度

编写解决方案，找出与之前（昨天的）日期相比温度更高的所有日期的 id 。

返回结果 无顺序要求 。

<details>

考察MySQL内置函数

```sql
select a.id from  Weather as a,Weather as b where datediff(a.recordDate,b.recordDate) = 1 and a.Temperature >b.Temperature;
```

</details>

# 1661 每台机器的平均运行时间

表: Activity

+----------------+---------+
| Column Name    | Type    |
+----------------+---------+
| machine_id     | int     |
| process_id     | int     |
| activity_type  | enum    |
| timestamp      | float   |
+----------------+---------+
该表展示了一家工厂网站的用户活动。
(machine_id, process_id, activity_type) 是当前表的主键（具有唯一值的列的组合）。
machine_id 是一台机器的ID号。
process_id 是运行在各机器上的进程ID号。
activity_type 是枚举类型 ('start', 'end')。
timestamp 是浮点类型,代表当前时间(以秒为单位)。
'start' 代表该进程在这台机器上的开始运行时间戳 , 'end' 代表该进程在这台机器上的终止运行时间戳。
同一台机器，同一个进程都有一对开始时间戳和结束时间戳，而且开始时间戳永远在结束时间戳前面。



现在有一个工厂网站由几台机器运行，每台机器上运行着 相同数量的进程 。编写解决方案，计算每台机器各自完成一个进程任务的平均耗时。

完成一个进程任务的时间指进程的'end' 时间戳 减去 'start' 时间戳。平均耗时通过计算每台机器上所有进程任务的总耗费时间除以机器上的总进程数量获得。

结果表必须包含machine_id（机器ID） 和对应的 average time（平均耗时） 别名 processing_time，且四舍五入保留3位小数。

以 任意顺序 返回表。

<details>

对于一张表的复杂运算，尝试连接两张表看看能得到什么，之后根据得到的表一步一步得到结果
之后我们要对无意义的行进行过滤
（1）.同一台机器
（2）.同一个进程
（3）.从start 到end
select需要的数据
根据machine_id进行分组便于累加t2.timestamp - t1.timestamp
求平均值并保留3位小数
拆解代码见下（对于初学者，建议将拆解的每一部分代码的结果都输出出来，仔细观察结果并思考，你一定会对MySQL题目有一个新的认识）：

```sql
select t1.machine_id,round(avg(t2.timestamp-t1.timestamp),3) processing_time
from activity t1, activity t2
where t1.machine_id = t2.machine_id
and t1.process_id = t2.process_id
and t1.activity_type='start'
and t2.activity_type='end'
group by t1.machine_id;
```

</details>

# 1280 学生们参加各科测试的次数

学生表: Students

+---------------+---------+
| Column Name   | Type    |
+---------------+---------+
| student_id    | int     |
| student_name  | varchar |
+---------------+---------+
在 SQL 中，主键为 student_id（学生ID）。
该表内的每一行都记录有学校一名学生的信息。



科目表: Subjects

+--------------+---------+
| Column Name  | Type    |
+--------------+---------+
| subject_name | varchar |
+--------------+---------+
在 SQL 中，主键为 subject_name（科目名称）。
每一行记录学校的一门科目名称。



考试表: Examinations

+--------------+---------+
| Column Name  | Type    |
+--------------+---------+
| student_id   | int     |
| subject_name | varchar |
+--------------+---------+
这个表可能包含重复数据（换句话说，在 SQL 中，这个表没有主键）。
学生表里的一个学生修读科目表里的每一门科目。
这张考试表的每一行记录就表示学生表里的某个学生参加了一次科目表里某门科目的测试。



查询出每个学生参加每一门科目测试的次数，结果按 student_id 和 subject_name 排序。

<details>

```sql
SELECT
    s.student_id, s.student_name, sub.subject_name, IFNULL(grouped.attended_exams, 0) AS attended_exams
FROM
    Students s
CROSS JOIN
    Subjects sub
LEFT JOIN (
    SELECT student_id, subject_name, COUNT(*) AS attended_exams
    FROM Examinations
    GROUP BY student_id, subject_name
) grouped
ON s.student_id = grouped.student_id AND sub.subject_name = grouped.subject_name
ORDER BY s.student_id, sub.subject_name;
```

</details>

# 570 至少有5名直接下属的经理

+-------------+---------+
| Column Name | Type    |
+-------------+---------+
| id          | int     |
| name        | varchar |
| department  | varchar |
| managerId   | int     |
+-------------+---------+
id 是此表的主键（具有唯一值的列）。
该表的每一行表示雇员的名字、他们的部门和他们的经理的id。
如果managerId为空，则该员工没有经理。
没有员工会成为自己的管理者。

编写一个解决方案，找出至少有五个直接下属的经理。

以 任意顺序 返回结果表。

<details>

```sql
select Name from (
select Manager.Name as Name, count(Report.Id) as cnt
from Employee as Manager
join Employee as Report
on Manager.Id=Report.ManagerId
group by Manager.Id
) as ReportCount
where cnt>=5;
```

</details>

# 1934 确认率

表: Signups

+----------------+----------+
| Column Name    | Type     |
+----------------+----------+
| user_id        | int      |
| time_stamp     | datetime |
+----------------+----------+
User_id是该表的主键。
每一行都包含ID为user_id的用户的注册时间信息。



表: Confirmations

+----------------+----------+
| Column Name    | Type     |
+----------------+----------+
| user_id        | int      |
| time_stamp     | datetime |
| action         | ENUM     |
+----------------+----------+
(user_id, time_stamp)是该表的主键。
user_id是一个引用到注册表的外键。
action是类型为('confirmed'， 'timeout')的ENUM
该表的每一行都表示ID为user_id的用户在time_stamp请求了一条确认消息，该确认消息要么被确认('confirmed')，要么被过期('timeout')。



用户的 确认率 是 'confirmed' 消息的数量除以请求的确认消息的总数。没有请求任何确认消息的用户的确认率为 0 。确认率四舍五入到 小数点后两位 。

编写一个SQL查询来查找每个用户的 确认率 。

以 任意顺序 返回结果表。

<details>

这道题目的要点是：AVG除了可以计算某字段的均值外,还可以计算符合条件的记录数占比。

```sql
select s.user_id, ROUND(IFNULL(AVG(c.action='confirmed'),0),2) as confirmation_rate
from Signups as s
left join Confirmations as c
on s.user_id = c.user_id
group by s.user_id;
```

</details>

# 1251 平均售价

表：Prices

+---------------+---------+
| Column Name   | Type    |
+---------------+---------+
| product_id    | int     |
| start_date    | date    |
| end_date      | date    |
| price         | int     |
+---------------+---------+
(product_id，start_date，end_date) 是 prices 表的主键（具有唯一值的列的组合）。
prices 表的每一行表示的是某个产品在一段时期内的价格。
每个产品的对应时间段是不会重叠的，这也意味着同一个产品的价格时段不会出现交叉。



表：UnitsSold

+---------------+---------+
| Column Name   | Type    |
+---------------+---------+
| product_id    | int     |
| purchase_date | date    |
| units         | int     |
+---------------+---------+
该表可能包含重复数据。
该表的每一行表示的是每种产品的出售日期，单位和产品 id。



编写解决方案以查找每种产品的平均售价。average_price 应该 四舍五入到小数点后两位。如果产品没有任何售出，则假设其平均售价为 0。

返回结果表 无顺序要求 。

<details>

```sql
select product_id, IFNULL(Round(SUM(sales)/SUM(units),2),0) as average_price
from (
  select Prices.product_id as product_id,
  Prices.price * UnitsSold.units as sales,
  UnitsSold.units as units
  from Prices
  left join UnitsSold on Prices.product_id = UnitsSold.product_id
  and (UnitsSold.purchase_date between Prices.start_date and Prices.end_date)
) T
group by product_id;
```

</details>

# 1211 查询结果的质量和占比

Queries 表：

+-------------+---------+
| Column Name | Type    |
+-------------+---------+
| query_name  | varchar |
| result      | varchar |
| position    | int     |
| rating      | int     |
+-------------+---------+
此表可能有重复的行。
此表包含了一些从数据库中收集的查询信息。
“位置”（position）列的值为 1 到 500 。
“评分”（rating）列的值为 1 到 5 。评分小于 3 的查询被定义为质量很差的查询。



将查询结果的质量 quality 定义为：

    各查询结果的评分与其位置之间比率的平均值。

将劣质查询百分比 poor_query_percentage 定义为：

    评分小于 3 的查询结果占全部查询结果的百分比。

编写解决方案，找出每次的 query_name 、 quality 和 poor_query_percentage。

quality 和 poor_query_percentage 都应 四舍五入到小数点后两位 。

以 任意顺序 返回结果表。

<details>

```sql
select query_name, ROUND(AVG(rating/position),2) quality,
ROUND(SUM(IF(rating < 3,1,0))*100/COUNT(*),2) poor_query_percentage
from Queries
where query_name IS NOT NULL
group by query_name;
```

</details>

# 1193 每月交易Ⅰ

表：Transactions

+---------------+---------+
| Column Name   | Type    |
+---------------+---------+
| id            | int     |
| country       | varchar |
| state         | enum    |
| amount        | int     |
| trans_date    | date    |
+---------------+---------+
id 是这个表的主键。
该表包含有关传入事务的信息。
state 列类型为 ["approved", "declined"] 之一。



编写一个 sql 查询来查找每个月和每个国家/地区的事务数及其总金额、已批准的事务数及其总金额。

以 任意顺序 返回结果表。

<details>

这道题目的知识点在于DATE_FORMAT函数。

```sql
select DATE_FORMAT(trans_date,'%Y-%m') as month,
country,
COUNT(*) as trans_count,
COUNT(IF(state='approved',1,NULL)) as approved_count,
SUM(amount) AS trans_total_amount,
SUM(IF(state='approved',amount,0)) AS approved_total_amount
from Transactions
group by month,country;
```

</details>

# 550 游戏玩法分析Ⅱ

Table: Activity

+--------------+---------+
| Column Name  | Type    |
+--------------+---------+
| player_id    | int     |
| device_id    | int     |
| event_date   | date    |
| games_played | int     |
+--------------+---------+
（player_id，event_date）是此表的主键（具有唯一值的列的组合）。
这张表显示了某些游戏的玩家的活动情况。
每一行是一个玩家的记录，他在某一天使用某个设备注销之前登录并玩了很多游戏（可能是 0）。



编写解决方案，报告在首次登录的第二天再次登录的玩家的 比率，四舍五入到小数点后两位。换句话说，你需要计算从首次登录日期开始至少连续两天登录的玩家的数量，然后除以玩家总数。

<details>

AVG非常灵活，可以对bool值进行计算。

```sql
select ROUND(avg(a.event_date is not NULL),2) fraction
from (
    select player_id,min(event_date) as login
    from Activity
    group by player_id
) p
left join Activity a
on p.player_id=a.player_id and datediff(a.event_date,p.login)=1;
```

</details>

# 1890 2020年最后一次登陆

+----------------+----------+
| 列名           | 类型      |
+----------------+----------+
| user_id        | int      |
| time_stamp     | datetime |
+----------------+----------+
(user_id, time_stamp) 是这个表的主键(具有唯一值的列的组合)。
每一行包含的信息是user_id 这个用户的登录时间。



编写解决方案以获取在 2020 年登录过的所有用户的本年度 最后一次 登录时间。结果集 不 包含 2020 年没有登录过的用户。

返回的结果集可以按 任意顺序 排列。

<details>

重点在于最大值上。

```sql
select user_id, max(time_stamp) as last_stamp
from Logins
where year(time_stamp)='2020'
group by user_id;
```

</details>
