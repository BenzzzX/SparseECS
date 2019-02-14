#pragma once
#include "../Components.hpp"
namespace ecs
{
	/*
	sparse_vector 把数组分为很多块,并在一块完全为空时释放掉内存
	适用于整体分散局部集中且较小的数据以得到较好的性能
	*/
	template<typename T>
	class sparse_vector
	{
		using index_t = common::index_t;
		const common::hbv& _entities;
		std::vector<T*> _components;
		static constexpr index_t Level = 2u;
		static constexpr index_t BucketSize = 1 << 12;
		index_t bucket_of(index_t i) const { return i >> 12; }
		index_t index_of(index_t i) const { return i & (BucketSize - 1); }

	public:
		sparse_vector(const common::hbv& entities)
			: _entities(entities), _components(10u, nullptr) {}

		T& get(index_t e)
		{
			return _components[bucket_of(e)][index_of(e)];
		}

		const T &get(index_t e) const
		{
			return _components[bucket_of(e)][index_of(e)];
		}

		T &create(index_t e, const T& arg)
		{
			index_t bucket = bucket_of(e);
			if (_components.size() <= bucket)
				_components.resize(bucket + _components.size(), nullptr);
			if (_components[bucket] == nullptr)
				_components[bucket] = (T*)malloc(sizeof(T)*BucketSize);
			return *(new (_components[bucket] + index_of(e)) T{ arg });
		}

		void batch_create(index_t begin, index_t end, const T& arg)
		{
			index_t first = bucket_of(begin);
			index_t last = bucket_of(end - 1);
			if (_components.size() <= last)
				_components.resize(last + _components.size(), nullptr);
			for (index_t i = first; i <= last; ++i)
				if (_entities.layer(Level, i) && !_components[i])
					_components[i] = (T*)malloc(sizeof(T)*BucketSize);
			if constexpr(std::is_pod_v<T>)
			{
				for (index_t i = first + 1; i < last; ++i)
					std::fill_n(_components[i], BucketSize, arg);
				index_t firstIndex = index_of(begin);
				if (last > first)
				{
					std::fill_n(_components[first] + firstIndex, BucketSize - firstIndex, arg);
					std::fill_n(_components[last], index_of(end), arg);
				}
				else std::fill_n(_components[first] + firstIndex, index_of(end) - firstIndex, arg);
			}
			else
			{
				for (index_t i = begin; i < end; ++i)
				{
					new (_components[bucket_of(i)] + index_of(i)) T{ arg };
				}
			}
		}

		void remove(index_t e)
		{
			index_t bucket = bucket_of(e);
			if constexpr(!std::is_pod_v<T>)
			{
				_components[bucket][index_of(e)].~T();
			}
			if (!_entities.layer(Level, bucket) && _components[bucket])
				free(_components[bucket]);
		}

		void batch_remove(const and_chbv& remove)
		{
			if constexpr(!std::is_pod_v<T>)
			{
				common::for_each(remove, [this](index_t i)
				{
					_components[bucket_of(i)][index_of(i)].~T();
				});
			}
		}

		void after_batch_remove()
		{
			common::for_each<Level - 1>(_entities, [this](index_t i)
			{
				if (!_entities.layer(Level, i) && _components[i])
					free(_components[i]);
			});
		}
	};

	DefStorage(sparse_vector)
	{
	public:
		DefConstructor(sparse_vector) : generic(_has) {}
		void batch_remove(const common::hbv& remove) noexcept
		{
			generic::batch_remove(remove);
			container.after_batch_remove();
		}
	};
}