#pragma once
#include "HBV.hpp"

namespace ecs
{
	using common::index_t;

	/*
	entity 从原理上应该只是一个身份 id
	这里添加了一个 generation 来验证 entity 的有效性
	*/
	struct entity
	{
		//hbv最大支持下标只用了24位
		index_t id : 24;
		index_t gen : 8;

		operator index_t() const
		{
			return id;
		}
	};


	/*
	entities 为 entity 管理器,保证尽量分配出连续的 id 和追踪 entity 的有效性
	kill 会使 entity 进入 killed 状态,并在调用 die 后正式生效(这样能保证顺序无关性)
	entities 满足 hbv_map<id, entity> 的概念,提供 filter 和 get 方法
	*/
	class entities final //todo: 把entities改为原子类型
	{
		//hbv并不记录有效位的数量,这里手动记录
		index_t _freeCount;
		index_t _killedCount;
		std::vector<std::uint8_t> _generation;
		common::hbv _dead;
		common::hbv _alive;
		common::hbv _killed;

		void grow_to(index_t to)
		{
			_freeCount += to - (index_t)_generation.size();
			_dead.grow_to(to);
			_generation.resize(to, 0u);
			_killed.grow_to(to);
			_alive.grow_to(to);
		}

		void grow(index_t base = 50u)
		{
			index_t origSize = (index_t)_generation.size();
			grow_to(origSize / 2u + origSize + base);
		}
	public:
		entities() : _generation(10u), _dead(10u, true), _killed(10u), _alive(10u), _freeCount(10u), _killedCount(0u) {}

		std::pair<index_t, index_t> batch_create(index_t n)
		{
			index_t end = last(_alive) + 1;
			if (end + n > _generation.size())
				grow_to(end + n);
			_freeCount -= n;
			_alive.range_set(end, end + n, true);
			for (index_t i = end; i < end + n; ++i)
				++_generation[i];
			return { end ,end + n };
		}

		index_t free_count()
		{
			return _freeCount;
		}

		const common::hbv& killed()
		{
			return _killed;
		}

		void die()
		{
			_freeCount += _killedCount;
			_killedCount = 0;
			_dead.merge_add(_killed);
			_alive.merge_sub(_killed);
			_killed.clear();
		}

		const common::hbv& filter() const
		{
			return _alive;
		}

		entity get(index_t i) const
		{
			return { i, _generation[i] };
		}

		entity create()
		{
			auto id = common::first(_dead);
			if (id == -1)
			{
				grow();
				id = common::first(_dead);
			}
			--_freeCount;
			index_t i = id;
			auto g = _generation[i]++;
			_dead.set(i, false);
			_alive.set(i, true);
			return entity{ i, g };
		}

		bool alive(entity e) const
		{
			return _generation[e.id] == e.gen
				&& _alive.test(e.id);
		}

		void kill(entity e)
		{
			_killedCount++;
			_killed.set(e.id, true);
		}
	};
}