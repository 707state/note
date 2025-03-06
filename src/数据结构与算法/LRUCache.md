-   [146 LRU缓存](#146-lru缓存)
    -   [定义](#定义)
    -   [实现](#实现)

# 146 LRU缓存 {#146-lru缓存}

## 定义

LRU是Least Recently
Used的缩写，即最近最少使用，是一种常用的页面置换算法，选择最近最久未使用的页面予以淘汰。该算法赋予每个页面一个访问字段，用来记录一个页面自上次被访问以来所经历的时间
t，当须淘汰一个页面时，选择现有页面中其 t
值最大的，即最近最少使用的页面予以淘汰。

## 实现

<details><summary>Click to expand</summary>

``` cpp
struct Node{
    int key,val;
    Node *prev,*next;
};

class LRUCache {
    int capacity;
    Node *dummy;
    std::unordered_map<int,Node* > node_map;
    void remove(Node *x){
        x->prev->next=x->next;
        x->next->prev=x->prev;
    }
    void push_front(Node *x){
        x->prev=dummy;
        x->next=dummy->next;
        dummy->next->prev=x;
        dummy->next=x;
    }
    Node *get_node(int k){
        auto iter=node_map.find(k);
        if(iter!=node_map.end())[[likely]]{
            auto node=iter->second;
            remove(node);
            push_front(node);
            return node;
        }   
        return nullptr;
    }

public:

    LRUCache(int capacity):capacity(capacity),dummy(new Node()) {
        dummy->next=dummy;
        dummy->prev=dummy;
    }
    
    int get(int key) {
        auto node=get_node(key);
        if(node) return node->val;
        return -1;
    }


    void put(int key, int value) {
        auto node=get_node(key);
        if(node){
            node->val=value;
            return;
        }
        node_map[key]=node=new Node(key,value);
        push_front(node);
        if(node_map.size()>capacity){
            auto node=dummy->prev;
            node_map.erase(node->key);
            remove(node);
            delete node;
        }
    }
};
```

</details>
