# Highlight
**ecs 模块是 header only 的.由重要组件 hbv 和 storage 组成.整体不超过 2k loc.**

## hbv
hbv 是**一种** bitmap like 的数据结构,维护压缩的布尔数组  
并**额外支持**
* 遍历
* 惰性合并
* 合并

## storage
storage 是**一类** map like 的数据结构  
维护**基于整数的**键值对但**不支持遍历**

## hbv+storage = hbv-map
hbv-map 是将一个 hbv 和一个(或多个)storage 结合在一起的一个新的数据结构.  
hbv-map 同时支持**查询**,**遍历**和**惰性合并**.

## hbv-map = components
ecs 模块使用 hbv-map 来维护 component 的集合,其中
* hbv 作为 tracer,跟踪 entity 对应的 component 的状态和事件(如拥有状态和创建事件).  
**除了"拥有状态 tracer" 其他 tracer 需要手动开启.**
* storage 作为容器,存放 entity 对应的 component 的数据.  
**storage 可以任意选择符合需求的实例.**

## hbv-map = entities
ecs 模块使用 hbv-map 来维护 entity 的集合,其中
* hbv 跟踪 entity 的生存情况,分配尽可能连续的 id 并回收利用 id.
* storage 储存 entity 的有效性信息,用以查询.

## components + entities = ecs
ecs 模块利用 hbv-map 可合并的特性来实现 ecs 的核心算法:筛选  
这种实现满足了 ecs 最原始的定义,并拥有极低的消耗(非常小的常数级

## ecs with batch
ecs 模块为这些数据结构都实现了 batch 版本的算法来提供高性能,其中包括:
* 批量创建
* 批量实例化
* 批量删除(对用户不可见)

## ecs with view
ecs 模块定义了 view 的概念:一个逻辑的可见度范围,其中包括:
* 逻辑可见的资源列表
* 逻辑对资源的可操作度,目前只区分读(共享)/写(占有)

在 view 的概念之上可以搭建 system,**但 ecs 模块并不定义 system**  
		
ecs 模块提供了基于 view 的工具,其中包括:
* 在 view 上执行函数,函数参数将会自动填充,且 component 会自动识别并遍历(可以选择遍历策略为线性/并行)
* 对于一个函数,可以自动提取他的 view

## sample
基于 ecs 模块可以轻松的搭建上层模块,以下为一个例子
```c++
#define View(...) ecs::view<__VA_ARGS__> view;
#define Job(p, f) ecs::for_view<p>(view, f)
#define Share const
//定义数据
struct Location { float x; };
struct Velocity { float x; };

namespace ecs
{	//把数据定义为 component,容器为 sparse_vector
	using Locations = Component(Location, sparse_vector);
	using Velocities = Component(Velocity, sparse_vector);
}

//定义一个 system,包含一个 view 和一个 update 函数
struct SomeSystem
{
	View(Locations, Share Velocities);

	void Update()
	{
		Job(par, [this] //并行逻辑
		(Location& loc, const Velocity& vel)
		{
			loc.x += vel.x;
		});
		Job(seq, [this] //线性逻辑
		(const Location& loc, const Velocity&) 
		{
			std::cout << loc.x << "\n"; //输出移动后的位置
		});
	}
};
```
