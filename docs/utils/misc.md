# Misc Utilities

本文汇总路径、区间、单位、日志、事件包和基础类型等不便单独归类的工具。

## `util::Path`

`util::Path` 位于 `include/sus/path.h`，实现位于 `libs/basecpp/path.cpp`。它是内核内部使用的简单路径处理类，不是完整 `std::filesystem::path` 替代品。

核心接口:

- 构造: `Path()`、`Path(string-like)`、`Path(std::string_view)`
- `concat(other)`: 直接拼接，不自动添加 `/`
- `operator/`: 路径组件拼接，会在左侧非空时添加 `/`
- `c_str()`、`view()`、转换为 `std::string`
- `is_absolute()`、`is_relative()`
- `parent_path()`、`filename()`、`stem()`、`extension()`
- `normalize()`
- `relative_to(base)`
- `starts_with(prefix)`、`ends_with(suffix)`
- 按路径段迭代

绝对路径迭代时根目录 `/` 会作为第一个元素。例如 `/home/user/docs` 的迭代结果是 `/`、`home`、`user`、`docs`。

`normalize()` 会处理重复斜杠、`.` 和 `..`:

```cpp
util::Path("///usr//local/../bin").normalize(); // "/usr/bin"
util::Path("./a/b/../c/./d").normalize();       // "a/c/d"
util::Path("/../../etc").normalize();           // "/etc"
```

当前实现假设路径字符串适合内核内部路径语义。空路径、带尾随斜杠路径、绝对路径后缀匹配等边界应参考测试行为，不应直接套用宿主文件系统语义。

## `util::range::Range<T>`

`include/sus/range.h` 提供半开区间 `[begin, end)`:

```cpp
template <typename T>
struct Range {
    T begin;
    T end;
};
```

核心接口:

- `nullable()`: `begin >= end` 时为空。
- `size()`: `end - begin`。
- `operator<=>`、`operator==`。
- `intersection(a, b)`: 计算交集。
- `is_intersecting(a, b)`: 判断两个区间是否相交。
- `within(range, value)`: 判断值是否在区间内。
- `within(range, other)`: 判断另一区间是否被包含。

该工具常用于地址区间、页区间、资源范围等半开区间场景。`T` 需要支持比较和减法。

## `units`

`include/sus/units.h` 提供带单位的轻量值类型。

### `units::frequency`

内部以毫赫兹保存，提供:

- `to_milihz()`、`to_hz()`、`to_khz()`、`to_mhz()`、`to_ghz()`
- `from_milihz()`、`from_hz()`、`from_khz()`、`from_mhz()`、`from_ghz()`
- 加减、乘除运算

### `units::time`

内部以纳秒保存，提供:

- `to_nanoseconds()`、`to_microseconds()`、`to_milliseconds()`、`to_seconds()`
- `from_nanoseconds()`、`from_microseconds()`、`from_milliseconds()`、`from_seconds()`
- 加减、乘除运算
- `time * frequency` 转为 tick 数量

### `units::rt_time`

内部以秒保存，提供:

- `to_seconds()`、`to_minutes()`、`to_hours()`、`to_days()`、`to_weeks()`
- `to_formatted_time()`，把纪元秒转换为年月日时分秒结构

`days_to_ymd()` 使用 Howard Hinnant 的日期算法转换纪元天数。

## 日志工具

`include/sus/logger.h` 定义通用 `Logger<LogInfo, PutFunctor>`，`kernel/logger.h` 基于 `kputs` 生成内核日志器。

日志级别:

- `DEBUG`
- `INFO`
- `WARN`
- `ERROR`
- `FATAL`
- `DISABLE`

声明日志器使用:

```cpp
DECLARE_LOGGER(kputer, LogLevel::INFO, TASK);
```

使用时通常通过 `loggers` 命名空间:

```cpp
loggers::TASK::INFO("create task pid=%lu", pid);
loggers::DEVICE::DEBUG("probe compatible=%s", compatible);
```

宏 `DEBUG`、`INFO`、`WARN`、`ERROR`、`FATAL` 会自动记录 `__FILE__`、`__LINE__` 和 `__func__`。

`GLOBAL_LOG_LEVEL` 和每个 logger 的级别共同决定实际输出。日志缓冲区当前固定为 256 字节，超长消息会被截断。

## 事件包

`include/sustcore/epacks.h` 定义了轻量事件 payload:

```cpp
struct TimerTickEvent {
    units::time last;
    units::time now;
    units::time delta;
};

struct NoPresentEvent {
    VirAddr access_address;
};
```

它们用于跨模块传递事件上下文，例如定时器 tick 和缺页访问地址。

## 基础类型与宏

`include/sus/types.h` 定义基础整数别名和机器字长类型:

- `byte`、`word`、`dword`、`qword`
- `b8`、`b16`、`b32`、`b64`
- `machine_bits`、`signed_machine_bits`
- `umb_t`、`smb_t`、`off_t`、`addr_t`
- `sus_i8`、`sus_i16`、`sus_i32`、`sus_i64`
- `sus_u8`、`sus_u16`、`sus_u32`、`sus_u64`

逻辑和位蕴含宏:

- `BOOL_IMPLIES(a, b)`: 逻辑蕴含。
- `BITS_IMPLIES(x, y)`: 位权限蕴含，含义近似 `(x & y) == y`。

编译属性宏:

- `PACKED`
- `NAKED`
- `ALIGNED(x)`
- `SECTION(x)`

这些宏主要用于底层 ABI、寄存器布局、链接脚本段放置和权限位检查。业务代码中应优先使用更具体的类型和辅助函数，不要把裸整数位操作散落到逻辑深处。
