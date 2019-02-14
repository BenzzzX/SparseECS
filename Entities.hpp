#pragma once
#include "HBV.hpp"

namespace ecs
{
	using common::index_t;

	/*
	entity ��ԭ����Ӧ��ֻ��һ����� id
	���������һ�� generation ����֤ entity ����Ч��
	*/
	struct entity
	{
		//hbv���֧���±�ֻ����24λ
		index_t id : 24;
		index_t gen : 8;

		operator index_t() const
		{
			return id;
		}
	};


	/*
	entities Ϊ entity ������,��֤��������������� id ��׷�� entity ����Ч��
	kill ��ʹ entity ���� killed ״̬,���ڵ��� die ����ʽ��Ч(�����ܱ�֤˳���޹���)
	entities ���� hbv_map<id, entity> �ĸ���,�ṩ filter �� get ����
	*/
	class entities final //todo: ��entities��Ϊԭ������
	{
		//hbv������¼��Чλ������,�����ֶ���¼
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