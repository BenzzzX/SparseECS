#include "pch.h"
#include "Storages.hpp"
#include "View.hpp"
#include <functional>
#include <iostream>

#define View(...) ecs::view<__VA_ARGS__> view;
#define Job(p, f) ecs::for_view<p>(view, f)
#define Share const
#define JobSystem(s, f) struct \
{ \
	private: static void job f \
	public: ecs::implict_view<decltype(job)> view; \
	void Update() \
	{ \
		for_view<s>(view, job); \
	} \
}
using namespace ecs;


//定义数据
struct Location { float x; };
struct Velocity { float x; };
struct ScriptableMover
{
	std::string script;
};

namespace ecs
{	//把数据定义为 component
	using Locations = Component(Location, sparse_vector);
	using Velocities = Component(Velocity, sparse_vector);
	using ScriptableMovers = Component(ScriptableMover, dense_vector);
}

struct VirtualMachine
{
	void Process(const std::string&, ...) {}
};

//定义一个 system,包含一个 view 和一个 update 函数
struct RetainSystem
{
	//view 代表这个 system 的操作范围和权限
	View(Locations, Velocities, Share ScriptableMovers);
	VirtualMachine vm;

	void Update()
	{	//在 system 中执行逻辑,参数将会自动填充
		Job(par, [this] //并行逻辑,同时拥有这三个 component 的 entity 将被匹配
		(Location& loc, Velocity& vel, const ScriptableMover& mover)
			{
				vm.Process(mover.script, loc, vel); //通过脚本更改速度和位置
			});
		Job(seq, [this] //线性逻辑
		(const Location& loc)
			{
				std::cout << loc.x << "\n"; //输出更改后的位置
			});
	}
};


//从一个纯函数中直接生成 system,view 将会自动推算
using AnotherSystem = JobSystem(par,
(Location& loc, const Velocity& vel)
{
	loc.x += vel.x; //对于没有脚本的 entity 直接位置加速度
});


int main()
{
	Locations locs;
	Velocities vels;
	ScriptableMovers movers;
	entities ents;

	auto e = ents.create();
	locs.create(e, { 1 });
	vels.create(e, { 1 });
	movers.create(e, { "" });

	e = ents.create();
	locs.create(e, { 1 });
	vels.create(e, { 1 });

	AnotherSystem as{ as_view(locs, vels) };
	RetainSystem ss{ as_view(locs, vels, movers) };

	as.Update();
	ss.Update();

	system("Pause");
}