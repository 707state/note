# 日志如何被应用？

首先需要区分一下Raft算法里面的几个关键数据结构的含义。

1. nextIndex[]: Leader维护的一个数组，长度等于集群节点数，表示的是Leader下一次要发送给Follower i的日志条目的索引。

2. matchIndex[]: Leader维护的数组，表示Follower i已经复制到的最大日志索引。当Follower确认到AppendEntries成功时，Leader就更新matchIndex\[i\]=last\_entry\_sent。同时，Leader可以用matchIndex[]数组来判断哪些日志项已经被多数节点存储从而推进commitIndex。

3. commitIndex: 用于在Leader中维护已经提交的日志，每次有新日志复制到多数节点时，Leader就可以提升自己的commitIndex，并通知Follower（下一次Append Entries RPC发送给Follower让Follower提交）。

4. log[]: 每个raft节点都有一个日志数组，用于存储LogEntries，每个日志都要记录<term, index, command>的三元组。

5. lastApplied: 每个节点独立维护一个，用于表示每一个节点已经应用到状态机的最大日志索引。这里lastApplied 必须不大于commitIndex。
