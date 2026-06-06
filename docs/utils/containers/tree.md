# Tree Utilities

本文说明 `include/sus/tree.h` 中的树结构工具。当前有两套实现: 嵌入式 `util::tree::Tree` 和继承式 `util::tree_base::TreeBase`。

## `util::tree::TreeNode`

`TreeNode<node_t, Tag>` 是嵌入式树节点字段:

```cpp
template <typename node_t, typename Tag = tree_tag>
struct TreeNode {
    node_t *parent = nullptr;
    util::ArrayList<node_t *> children;
};
```

当 `Tag` 是 `tree_lca_tag` 时，节点额外维护 `depth`，用于最近公共祖先计算。

典型定义:

```cpp
struct Node {
    util::tree::TreeNode<Node, util::tree::tree_lca_tag> tree_node;
    int id;
};

using NodeTree = util::tree::Tree<
    Node,
    &Node::tree_node,
    util::tree::tree_lca_tag>;
```

## `util::tree::Tree`

`Tree<node_t, tree_node_ptr, Tag>` 是一组静态操作，树结构本身存放在对象内嵌的 `TreeNode` 字段中。

核心接口:

- `link_child(parent, child)`
- `is_root(node)`
- `foreach_pre(node, func)`
- `foreach_post(node, func)`
- `foreach_child(node, func)`
- `get_parent(node)`
- `get_children(node)`
- `is_ancestor(ancestor, descendant)`
- `lca(a, b)`

### 不变量

- `link_child()` 要求子节点当前没有父节点。
- `tree_lca_tag` 模式下插入时会维护子节点深度，并断言只能插入叶子节点。
- `lca()` 只有在 `tree_lca_tag` 模式下有意义；普通 `tree_tag` 模式返回 `nullptr`。
- 该结构不拥有节点对象，节点销毁前必须保证外部树关系已经不再使用。

## 遍历顺序

`foreach_pre` 是前序遍历，先访问当前节点，再访问子树。

`foreach_post` 是后序遍历，先访问所有子树，再访问当前节点。

`foreach_child` 只访问直接子节点，不递归。

测试中的树:

```text
0
|-- 1
|   |-- 3
|   `-- 4
`-- 2
```

前序遍历顺序是 `0, 1, 3, 4, 2`。

## `util::tree_base::TreeBase`

`TreeBase<Derived, CustomTag, Trait>` 使用 CRTP 方式让对象通过继承获得树结构。它内部保存:

- `Derived *parent`
- `util::ArrayList<Derived *> children`

典型定义:

```cpp
using TreeA = util::tree_base::TreeBase<class Data, class TreeATag>;
using TreeB = util::tree_base::LCATreeBase<class Data, class TreeBTag>;

class Data : public TreeA, public TreeB {
public:
    int data = 0;
};
```

`CustomTag` 用于区分同一对象上的多棵树。上例中同一组 `Data` 可以同时参与 `TreeA` 和 `TreeB` 两套拓扑。

核心接口:

- `get_parent()`、`set_parent()`
- `get_children()`
- `add_child()`、`remove_child()`
- `link_parent()`、`link_child()`
- `unlink_parent()`、`unlink_child()`
- `is_root()`
- `foreach_pre()`、`foreach_post()`、`foreach_child()`
- `is_ancestor_of()`
- `get_depth()`
- `lca(a, b)`

## Trait

`TreeBase` 的第三个模板参数用于注入维护逻辑:

- `EmptyTrait`: 默认空行为。
- `DepthTrait`: 链接/解除链接时维护整棵子树深度，支持 `get_depth()` 和 `lca()`。
- `LazyDepthTrait`: 懒更新深度缓存，用版本号减少重复更新。

`LCATreeBase<Derived, CustomTag>` 是使用 `DepthTrait` 的别名。

## 调试检查

非 `NDEBUG` 构建下，`TreeBase::check_loop()` 会在设置父节点前检查是否形成循环。如果发现循环会触发断言。

## 选择建议

- 对象已经有清晰成员字段，希望像链表一样显式嵌入树节点: 使用 `util::tree::Tree`。
- 需要通过继承让对象直接获得树操作，或者同一对象需要参与多棵树: 使用 `util::tree_base::TreeBase`。
- 需要频繁查询最近公共祖先: 使用 `tree_lca_tag` 或 `LCATreeBase`。
- 树结构只表达关系，不表达所有权；对象释放仍由对应子系统管理。
