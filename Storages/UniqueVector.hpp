#pragma once
#include "SparseVector.hpp"

namespace ecs
{
	template<typename T>
	struct is_shared_ptr : std::false_type {};
	template<typename T>
	struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

	/*
	unique_vector 只持有数据的唯一的指针并为每一个数据维护一个 filter,可以在 filter 之间随意切换
	适用于链接不变的较大的运行时资源(如mesh)
	*/
	template<typename T>
	class unique_vector
	{
		static_assert(std::is_pointer_v<T> || is_shared_ptr<T>{} , "unique vector only work with pointers");
		std::vector<T> _components;
		std::vector<common::hbv> _filters;
		sparse_vector<index_t> _redirector;
		using ref = decltype(*std::declval<T>());

		void create_on(index_t e, index_t i) noexcept
		{
			auto& filter = _filters[i];
			if (filter.size() < e)
				filter.grow_to(e * 3 / 2);
			filter.set(e, true);
			_redirector.create(e, i);
		}
	public:
		//如果 current filter 小于零,则为 'has' filter
		int32_t currentFilter;

		unique_vector(const common::hbv& entities)
			: _redirector(entities), currentFilter(-1) { }

		//note: 如果 current filter > 0, 参数 e 将会被忽略
		const std::decay_t<ref> *get(index_t e) const
		{
			if (currentFilter>0)
				return _components[currentFilter];
			else
				return _components[_redirector.get(e)];
		}

		std::decay_t<ref> *get(index_t e)
		{
			if (currentFilter>0)
				return _components[currentFilter];
			else
				return _components[_redirector.get(e)];
		}

		index_t unique_size() const
		{
			return _components.size();
		}

		int32_t find_unique(const T& arg) const
		{
			for (index_t i = 0; i < _components.size(); ++i)
				if (arg == _components[i])
					return i;
			return -1;
		}

		const common::hbv &filter() const
		{
			return _filters[FilterId];
		}

		void batch_create(index_t begin, index_t end, const T& arg)
		{
			create(begin, arg);
			index_t prototype = _redirector.get(begin);
			auto& filter = _filters[prototype];
			if (filter.size() < end)
				filter.grow_to(end + 1u);
			filter.range_set(begin + 1, end, true);
			_redirector.batch_create(begin + 1, end, prototype);
		}

		void instantiate(index_t e, index_t proto)
		{
			index_t prototype = _redirector.get(proto);
			_filters[prototype].set(e, true);
			_redirector.create(e, prototype);
		}

		const T &create(index_t e, const T& arg)
		{
			for (index_t i = 0; i < _components.size(); ++i)
				if (arg == _components[i])
				{
					create_on(e, i);
					return arg;
				}
			index_t i = 0;
			for (; i < _components.size(); ++i)
				if (nullptr == _components[i]) break;
			if (i == _components.size())
			{
				_components.emplace_back(arg);
				_filters.emplace_back();
			}
			create_on(e, i);
			return _components.back();
		}

		void batch_remove(const and_chbv& remove)
		{
			for (int i = 0; i < _filters.size(); ++i)
			{
				_filters[i].merge_sub(remove);
				if (common::empty(_filters[i]))
					_components[i] = nullptr;
			}
		}

		void remove(index_t e)
		{
			index_t id = _redirector.get(e);
			auto& filter = _filters[id];
			filter.set(e, false);
			_redirector.remove(e);
			if (common::empty(filter))
				_components[id] = nullptr;
		}
	};

	DefStorage(unique_vector)
	{
	public:
		DefConstructor(unique_vector) : generic(has()) {}
		index_t unique_size() const noexcept
		{
			return container.unique_size();
		}

		void set_unique_as_filter(const T& value) noexcept
		{
			int32_t id = container.find_unique(value);
			container.currentFilter = id;
		}

		const auto& get_unique_as_filter(int32_t i) noexcept
		{
			container.currentFilter = i;
			//Hack!在filter状态下,返回和id无关,所以可以随便传一个数
			return container.get(0);
		}

		void reset_filter() noexcept
		{
			container.currentFilter = -1;
		}

		decltype(auto) filter() const noexcept
		{
			if (container.currentFilter >= 0)
			{
				return container.filter();
			}
			else
			{
				return generic::filter();
			}
		}
	};
}