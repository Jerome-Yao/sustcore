# Static Reflection And Runtime Type Check

本文说明 `include/sus/rtti.h` 中的类型标识工具。它用于在不依赖 C++ RTTI 的环境中为基类对象提供运行时类型检查和安全向下转换。

## 背景

Sustcore 内核对象经常通过基类指针传递，例如:

- `cap::Payload`
- `kernel/vfs/ops.h` 中的 `IINode`
- 块设备操作接口 `IBlockDeviceOps`
- CPU 描述接口 `Cpu`

这些对象需要在运行时判断真实类型，但内核不应依赖标准 C++ RTTI 和 `dynamic_cast`。`RTTIBase<BaseType, TypeIdEnum>` 提供了轻量替代。

## 基本模型

派生体系需要提供:

1. 一个枚举类型作为类型 ID。
2. 一个基类继承 `RTTIBase<BaseType, TypeIdEnum>`。
3. 每个派生类提供 `static constexpr IDENTIFIER`。
4. 每个派生类实现或继承 `type_id()`。

概念约束 `DrvdClassTrait` 会检查:

- `TypeIdEnum` 是枚举。
- 派生类是基类的子类。
- 基类对象可调用 `type_id()`。
- 派生类存在 `IDENTIFIER`。

## 示例

```cpp
enum class INodeType {
    FILE,
    DIRECTORY,
};

class IINode : public RTTIBase<IINode, INodeType> {
protected:
    virtual INodeType type_id() const = 0;
};

class IFile : public IINode {
public:
    static constexpr INodeType IDENTIFIER = INodeType::FILE;

protected:
    INodeType type_id() const override {
        return IDENTIFIER;
    }
};
```

使用时:

```cpp
IINode *inode = ...;

if (inode->is<IFile>()) {
    auto *file = inode->as<IFile>();
    // file 非空
}
```

也可以通过静态函数转换:

```cpp
auto *file = RTTIBase<IINode, INodeType>::cast<IFile>(inode);
```

## 接口

`RTTIBase` 提供:

- `is<T>()`: 判断当前对象的 `type_id()` 是否等于 `T::IDENTIFIER`。
- `as<T>()`: 若类型匹配则返回 `T *`，否则返回 `nullptr`。
- `as<T>() const`: const 版本。
- `cast<T>(BaseType *base)`: 静态转换辅助。
- `cast<T>(const BaseType *base)`: const 静态转换辅助。

## 与能力系统的关系

能力系统的 `cap::Payload` 基类使用同一套模型。每种 payload 通过 `PayloadType` 标识真实对象类型，能力对象封装层通常先 lookup capability，再检查 payload 类型，最后构造具体 `CapObj`。

这让 syscall 层可以用统一流程处理能力:

1. 查找 `Capability *`。
2. 检查 `payload()->type_id()` 或使用封装对象构造时的类型检查。
3. 构造 `MemoryObject`、`EndpointObject`、`PCBObject` 等对象。
4. 在对象方法内执行权限检查和业务逻辑。

## 注意事项

- `type_id()` 是受保护的虚函数，外部通常通过 `is<T>()`、`as<T>()` 或模块封装接口使用。
- `as<T>()` 返回裸指针，不改变所有权，也不延长对象生命周期。
- 类型标识只判断最具体的 `IDENTIFIER`，不表达复杂继承层次中的“子类型集合”关系。
- 新增派生类型时必须保证 `IDENTIFIER` 唯一，否则类型检查会产生错误结果。
- 如果基类对象可能为空，应先检查指针或使用 `util::nonnull<T *>` 表达非空前置条件。
