


---
title: "Mysql技术内幕：InnoDB学习 (7)"
author: "jask"
date: "2024-08-11"
output: pdf_document
header-includes:
  - \usepackage{fontspec}
  - \usepackage{xeCJK}
  - \setmainfont{ComicShannsMono Nerd Font} 
  - \setCJKmainfont{Noto Sans CJK SC}  # 替换为可用的字体
  - \setCJKmonofont{Noto Sans CJK SC}
  - \setCJKsansfont{Noto Sans CJK SC}
---

# Mysql技术内幕

## 备份与恢复

可以根据不同的类型来划分备份的方法。根据备份的方法不同可以将备份分为：

Hot Backup（热备）

Cold Backup（冷备）

Warm Backup（温备）

### Hot Backup
Hot Backup是指数据库运行中直接备份，对正在运行的数据库操作没有任何的影响。这种方式在MySQL官方手册中称为Online Backup（在线备份）。

Cold Backup发生在数据库停止的情况下。又叫Offline Backup。

按照备份后文件的内容，备份又可以分为：

逻辑备份

裸文件备份

若按照备份数据库的内容来分，备份又可以分为：

完全备份

增量备份

日志备份

### Cold Backup
对于InnoDB存储引擎的冷备非常简单，只需要备份MySQL数据库的frm文件，共享表空间文件，独立表空间文件（*.ibd），重做日志文件。

冷备的优点是：

备份简单，只要复制相关文件即可。

备份文件易于在不同操作系统，不同MySQL版本上进行恢复。

恢复相当简单，只需要把文件恢复到指定位置即可。

恢复速度快，不需要执行任何SQL语句，也不需要重建索引。

冷备的缺点是：

InnoDB存储引擎冷备的文件通常比逻辑文件大很多，因为表空间中存放着很多其他的数据，如undo段，插入缓冲等信息。

冷备也不总是可以轻易地跨平台。操作系统、MySQL的版本、文件大小写敏感和浮点数格式都会成为问题。


