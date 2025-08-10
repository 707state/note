<!--toc:start-->
- [B树](#b树)
  - [WAL](#wal)
  - [Pager页面管理](#pager页面管理)
    - [get_page](#getpage)
    - [write_page](#writepage)
- [write_page_at_offset](#writepageatoffset)
  - [page](#page)
    - [insert_byte_at_offset](#insertbyteatoffset)
  - [node](#node)
    - [split](#split)
  - [BTree](#btree)
    - [BTree](#btree)
    - [B树Builder](#b树builder)
    - [BTree操作](#btree操作)
      - [插入](#插入)
      - [搜索](#搜索)
      - [删除](#删除)
      - [合并](#合并)
<!--toc:end-->

# B树

## WAL

记录根节点偏移量，实现如下：

<details><summary>Click to expand</summary>

``` rs
pub struct Wal {
    file: File,
}
impl Wal {
    pub fn new(parent_directoy: PathBuf) -> Result<Self, Error> {
        let fd = OpenOptions::new()
            .create(true)
            .read(true)
            .write(true)
            .truncate(true)
            .open(parent_directoy.join("wal"))?;

        Ok(Self { file: fd })
    }
    pub fn get_root(&mut self) -> Result<Offset, Error> {//读取偏移量
        let mut buff: [u8; PTR_SIZE] = [0x00; PTR_SIZE];
        let file_len = self.file.seek(SeekFrom::End(0))? as usize;
        let mut root_offset: usize = 0;
        if file_len > 0 {
            root_offset = (file_len / PTR_SIZE - 1) * PTR_SIZE;
        }
        self.file.seek(SeekFrom::Start(root_offset as u64))?;
        self.file.read_exact(&mut buff)?;
        Offset::try_from(buff)
    }

    pub fn set_root(&mut self, offset: Offset) -> Result<(), Error> {//设置偏移量
        self.file.seek(SeekFrom::End(0))?;
        self.file.write_all(&offset.0.to_be_bytes())?;
        Ok(())
    }
}
```

</details>

## Pager页面管理

负责管理文件中的页面（即数据块），并提供从磁盘读取和写入页面的接口。

### get_page

<details><summary>Click to expand</summary>

``` rs
    pub fn get_page(&mut self, offset: &Offset) -> Result<Page, Error> {
        let mut page: [u8; PAGE_SIZE] = [0x00; PAGE_SIZE];
        self.file.seek(SeekFrom::Start(offset.0 as u64))?;
        self.file.read_exact(&mut page)?;
        Ok(Page::new(page))
    }
```

</details>

从磁盘上的特定偏移量读取一个页面。B+
树的节点通常被存储在磁盘上的不同页面中，因此当需要访问特定节点时，Pager
会从磁盘中读取相应的页面。偏移量 Offset 指向页面在文件中的位置。

### write_page

<details><summary>Click to expand</summary>

``` rs
    pub fn write_page(&mut self, page: Page) -> Result<Offset, Error> {
        self.file.seek(SeekFrom::Start(self.curser as u64))?;
        self.file.write_all(&page.get_data())?;
        let res = Offset(self.curser);
        self.curser += PAGE_SIZE;
        Ok(res)
    }
```

</details>

将页面写入磁盘文件的当前游标位置，并返回该页面的偏移量（Offset）。在 B+
树的插入或删除操作后，节点可能会发生变动，此时 Pager
会将变动后的节点（即页面）写入磁盘。这个函数通常在插入新节点或页面时使用，并自动将页面顺序写入磁盘。

# write_page_at_offset

<details><summary>Click to expand</summary>

``` rs
    pub fn write_page_at_offset(&mut self, page: Page, offset: &Offset) -> Result<(), Error> {
        self.file.seek(SeekFrom::Start(offset.0 as u64))?;
        self.file.write_all(&page.get_data())?;
        Ok(())
```

</details>

在给定的偏移量 Offset 上写入页面。用于覆盖磁盘中某个位置的页面数据。B+
树中如果某个页面（节点）需要更新，而位置已经确定，则通过这个函数覆盖原有页面的数据。

## page

### insert_byte_at_offset

<details><summary>Click to expand</summary>

``` rs
    pub fn insert_bytes_at_offset(
        &mut self,
        bytes: &[u8],
        offset: usize,
        end_offset: usize,
        size: usize,
    ) -> Result<(), Error> {
        // This Should not occur - better verify.
        if end_offset + size > self.data.len() {
            return Err(Error::UnexpectedError);
        }
        for idx in (offset..=end_offset).rev() {
            self.data[idx + size] = self.data[idx]
        }
        self.data[offset..offset + size].clone_from_slice(bytes);
        Ok(())
    }
```

</details>

在指定offset处插入size大小的bytes。

## node

定义：

<details><summary>Click to expand</summary>

``` rs
pub struct Node {
    pub node_type: NodeType,
    pub is_root: bool,
    pub parent_offset: Option<Offset>,
}
```

</details>

### split

<details><summary>Click to expand</summary>

``` rs
    pub fn split(&mut self, b: usize) -> Result<(Key, Node), Error> {
        match self.node_type {
            NodeType::Internal(ref mut children, ref mut keys) => {
                // Populate siblings keys.
                let mut sibling_keys = keys.split_off(b - 1);
                // Pop median key - to be added to the parent..
                let median_key = sibling_keys.remove(0);
                // Populate siblings children.
                let sibling_children = children.split_off(b);
                Ok((
                    median_key,
                    Node::new(
                        NodeType::Internal(sibling_children, sibling_keys),
                        false,
                        self.parent_offset.clone(),
                    ),
                ))
            }
            NodeType::Leaf(ref mut pairs) => {
                // Populate siblings pairs.
                let sibling_pairs = pairs.split_off(b);
                // Pop median key.
                let median_pair = pairs.get(b - 1).ok_or(Error::UnexpectedError)?.clone();

                Ok((
                    Key(median_pair.key),
                    Node::new(
                        NodeType::Leaf(sibling_pairs),
                        false,
                        self.parent_offset.clone(),
                    ),
                ))
            }
            NodeType::Unexpected => Err(Error::UnexpectedError),
        }
    }
}
```

</details>

节点分裂操作。

## BTree

### BTree

<details><summary>Click to expand</summary>

``` rs
pub struct BTree {
    pager: Pager,
    b: usize,
    wal: Wal,
}
```

</details>

### B树Builder

<details><summary>Click to expand</summary>

``` rs
pub struct BTreeBuilder {
    /// Path to the tree file.
    path: &'static Path,
    /// The BTree parameter, an inner node contains no more than 2*b-1 keys and no less than b-1 keys
    /// and no more than 2*b children and no less than b children.
    b: usize,
}
```

</details>

### BTree操作

判断node有没有满

<details><summary>Click to expand</summary>

``` rs
    fn is_node_full(&self, node: &Node) -> Result<bool, Error> {
        match &node.node_type {
            NodeType::Leaf(pairs) => Ok(pairs.len() == (2 * self.b)),
            NodeType::Internal(_, keys) => Ok(keys.len() == (2 * self.b - 1)),
            NodeType::Unexpected => Err(Error::UnexpectedError),
        }
    }
```

</details>

#### 插入

插入的逻辑（不包含实际操作）

<details><summary>Click to expand</summary>

``` rs
    pub fn insert(&mut self, kv: KeyValuePair) -> Result<(), Error> {
        let root_offset = self.wal.get_root()?;
        let root_page = self.pager.get_page(&root_offset)?;
        let new_root_offset: Offset;
        let mut new_root: Node;
        let mut root = Node::try_from(root_page)?;
        if self.is_node_full(&root)? {//节点满了，需要分裂
            // split the root creating a new root and child nodes along the way.
            new_root = Node::new(NodeType::Internal(vec![], vec![]), true, None);
            // write the new root to disk to aquire an offset for the new root.
            new_root_offset = self.pager.write_page(Page::try_from(&new_root)?)?;
            // set the old roots parent to the new root.
            root.parent_offset = Some(new_root_offset.clone());
            root.is_root = false;
            // split the old root.
            let (median, sibling) = root.split(self.b)?;//分裂，返回中位值和新创建的兄弟节点
            // write the old root with its new data to disk in a *new* location.
            let old_root_offset = self.pager.write_page(Page::try_from(&root)?)?;
            // write the newly created sibling to disk.
            let sibling_offset = self.pager.write_page(Page::try_from(&sibling)?)?;
            // update the new root with its children and key.
            new_root.node_type =
                NodeType::Internal(vec![old_root_offset, sibling_offset], vec![median]);
            // write the new_root to disk.
            self.pager
                .write_page_at_offset(Page::try_from(&new_root)?, &new_root_offset)?;
        } else {
            new_root = root.clone();
            new_root_offset = self.pager.write_page(Page::try_from(&new_root)?)?;
        }
        // continue recursively.
        self.insert_non_full(&mut new_root, new_root_offset.clone(), kv)?;//分裂后就不再满了
        // finish by setting the root to its new copy.
        self.wal.set_root(new_root_offset)
    }
```

</details>

插入非满的节点

<details><summary>Click to expand</summary>

``` rs
    fn insert_non_full(
        &mut self,
        node: &mut Node,
        node_offset: Offset,
        kv: KeyValuePair,
    ) -> Result<(), Error> {
        match &mut node.node_type {
            NodeType::Leaf(ref mut pairs) => {
                let idx = pairs.binary_search(&kv).unwrap_or_else(|x| x);
                pairs.insert(idx, kv);
                self.pager
                    .write_page_at_offset(Page::try_from(&*node)?, &node_offset)
            }
            NodeType::Internal(ref mut children, ref mut keys) => {
                let idx = keys
                    .binary_search(&Key(kv.key.clone()))
                    .unwrap_or_else(|x| x);
                let child_offset = children.get(idx).ok_or(Error::UnexpectedError)?.clone();
                let child_page = self.pager.get_page(&child_offset)?;
                let mut child = Node::try_from(child_page)?;
                // Copy each branching-node on the root-to-leaf walk.
                // write_page appends the given page to the db file thus creating a new node.
                let new_child_offset = self.pager.write_page(Page::try_from(&child)?)?;
                // Assign copied child at the proper place.
                children[idx] = new_child_offset.to_owned();
                if self.is_node_full(&child)? {
                    // split will split the child at b leaving the [0, b-1] keys
                    // while moving the set of [b, 2b-1] keys to the sibling.
                    let (median, mut sibling) = child.split(self.b)?;
                    self.pager
                        .write_page_at_offset(Page::try_from(&child)?, &new_child_offset)?;
                    // Write the newly created sibling to disk.
                    let sibling_offset = self.pager.write_page(Page::try_from(&sibling)?)?;
                    // Siblings keys are larger than the splitted child thus need to be inserted
                    // at the next index.
                    children.insert(idx + 1, sibling_offset.clone());
                    keys.insert(idx, median.clone());

                    // Write the parent page to disk.
                    self.pager
                        .write_page_at_offset(Page::try_from(&*node)?, &node_offset)?;
                    // Continue recursively.
                    if kv.key <= median.0 {
                        self.insert_non_full(&mut child, new_child_offset, kv)
                    } else {
                        self.insert_non_full(&mut sibling, sibling_offset, kv)
                    }
                } else {
                    self.pager
                        .write_page_at_offset(Page::try_from(&*node)?, &node_offset)?;
                    self.insert_non_full(&mut child, new_child_offset, kv)
                }
            }
            NodeType::Unexpected => Err(Error::UnexpectedError),
        }
    }
```

</details>

#### 搜索

搜索一个节点

<details><summary>Click to expand</summary>

``` rs
    fn search_node(&mut self, node: Node, search: &str) -> Result<KeyValuePair, Error> {
        match node.node_type {
            NodeType::Internal(children, keys) => {
                let idx = keys
                    .binary_search(&Key(search.to_string()))
                    .unwrap_or_else(|x| x);
                // Retrieve child page from disk and deserialize.
                let child_offset = children.get(idx).ok_or(Error::UnexpectedError)?;
                let page = self.pager.get_page(child_offset)?;
                let child_node = Node::try_from(page)?;
                self.search_node(child_node, search)
            }
            NodeType::Leaf(pairs) => {
                if let Ok(idx) =
                    pairs.binary_search_by_key(&search.to_string(), |pair| pair.key.clone())
                {
                    return Ok(pairs[idx].clone());
                }
                Err(Error::KeyNotFound)
            }
            NodeType::Unexpected => Err(Error::UnexpectedError),
        }
    }
```

</details>

#### 删除

删除的逻辑

<details><summary>Click to expand</summary>

``` rs
    pub fn delete(&mut self, key: Key) -> Result<(), Error> {
        let root_offset = self.wal.get_root()?;
        let root_page = self.pager.get_page(&root_offset)?;
        // Shadow the new root and rewrite it.
        let mut new_root = Node::try_from(root_page)?;
        let new_root_page = Page::try_from(&new_root)?;
        let new_root_offset = self.pager.write_page(new_root_page)?;
        self.delete_key_from_subtree(key, &mut new_root, &new_root_offset)?;
        self.wal.set_root(new_root_offset)
    }
```

</details>

从子树删除一个key

<details><summary>Click to expand</summary>

``` rs
    fn delete_key_from_subtree(
        &mut self,
        key: Key,
        node: &mut Node,
        node_offset: &Offset,
    ) -> Result<(), Error> {
        match &mut node.node_type {
            NodeType::Leaf(ref mut pairs) => {
                let key_idx = pairs
                    .binary_search_by_key(&key, |kv| Key(kv.key.clone()))
                    .map_err(|_| Error::KeyNotFound)?;
                pairs.remove(key_idx);
                self.pager
                    .write_page_at_offset(Page::try_from(&*node)?, node_offset)?;
                // Check for underflow - if it occures,
                // we need to merge with a sibling.
                // this can only occur if node is not the root (as it cannot "underflow").
                // continue recoursively up the tree.
                self.borrow_if_needed(node.to_owned(), &key)?;
            }
            NodeType::Internal(children, keys) => {
                let node_idx = keys.binary_search(&key).unwrap_or_else(|x| x);
                // Retrieve child page from disk and deserialize,
                // copy over the child page and continue recursively.
                let child_offset = children.get(node_idx).ok_or(Error::UnexpectedError)?;
                let child_page = self.pager.get_page(child_offset)?;
                let mut child_node = Node::try_from(child_page)?;
                // Fix the parent_offset as the child node is a child of a copied parent
                // in a copy-on-write root to leaf traversal.
                // This is important for the case of a node underflow which might require a leaf to root traversal.
                child_node.parent_offset = Some(node_offset.to_owned());
                let new_child_page = Page::try_from(&child_node)?;
                let new_child_offset = self.pager.write_page(new_child_page)?;
                // Assign the new pointer in the parent and continue reccoursively.
                children[node_idx] = new_child_offset.to_owned();
                self.pager
                    .write_page_at_offset(Page::try_from(&*node)?, node_offset)?;
                return self.delete_key_from_subtree(key, &mut child_node, &new_child_offset);
            }
            NodeType::Unexpected => return Err(Error::UnexpectedError),
        }
        Ok(())
    }
```

</details>

#### 合并

<details><summary>Click to expand</summary>

``` rs
    fn merge(&self, first: Node, second: Node) -> Result<Node, Error> {
        match first.node_type {
            NodeType::Leaf(first_pairs) => {
                if let NodeType::Leaf(second_pairs) = second.node_type {
                    let merged_pairs: Vec<KeyValuePair> = first_pairs
                        .into_iter()
                        .chain(second_pairs.into_iter())
                        .collect();
                    let node_type = NodeType::Leaf(merged_pairs);
                    Ok(Node::new(node_type, first.is_root, first.parent_offset))
                } else {
                    Err(Error::UnexpectedError)
                }
            }
            NodeType::Internal(first_offsets, first_keys) => {
                if let NodeType::Internal(second_offsets, second_keys) = second.node_type {
                    let merged_keys: Vec<Key> = first_keys
                        .into_iter()
                        .chain(second_keys.into_iter())
                        .collect();
                    let merged_offsets: Vec<Offset> = first_offsets
                        .into_iter()
                        .chain(second_offsets.into_iter())
                        .collect();
                    let node_type = NodeType::Internal(merged_offsets, merged_keys);
                    Ok(Node::new(node_type, first.is_root, first.parent_offset))
                } else {
                    Err(Error::UnexpectedError)
                }
            }
            NodeType::Unexpected => Err(Error::UnexpectedError),
        }
    }
```

</details>
