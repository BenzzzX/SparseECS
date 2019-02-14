#pragma once
#include "SparseVector.hpp"

namespace ecs
{
	/*
	dense_vector 用一个紧密的数组来存放数据并用一个重定向表来定位数据
	适用于较大的数据以避免浪费内存
	*/
	template<typename T>
	class dense_vector
	{
		struct elem 
		{ 
			T data; index_t owner; 
			elem(const T& d, index_t o) : data(d), owner(o) {}
		};
		std::vector<elem> _components;
		sparse_vector<index_t> _redirector;
	public:
		dense_vector(const common::hbv& entities)
			: _redirector(entities)
		{
		}

		T &get(index_t e)
		{
			return _components[_redirector.get(e)].data;
		}

		const T &get(index_t e) const
		{
			return _components[_redirector.get(e)].data;
		}

		T &create(index_t e, const T& arg)
		{
			_redirector.create(e, (index_t)_components.size());
			_components.emplace_back( arg, e );
			return _components.back().data;
		}

		void remove(index_t e)
		{
			if (_components.size() > 1)
			{
				auto& id = _redirector.get(e);
				auto& ld = _redirector.get(_components.back().owner);
				_components[id] = std::move(_components[ld]);
				ld = id;
			}
			_components.pop_back();
			_redirector.remove(e);
		}
	};

	DefStorage(dense_vector)
	{
	public:
		DefConstructor(dense_vector) : generic(_has) {}
	};
}