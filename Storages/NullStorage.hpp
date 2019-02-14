#pragma once
#include "../Components.hpp"

namespace ecs
{
	/*
	ø’ map,”√”⁄ø’ component(º¥flag)
	*/
	template<typename T>
	class null_storage
	{
	public:
		T & get(index_t e)
		{
			return *(T*)nullptr;
		}

		const T &get(index_t e) const
		{
			return *(T*)nullptr;
		}

		void batch_create(index_t begin, index_t end, const T& arg)
		{
		}

		void instantiate(index_t e, index_t proto)
		{
		}

		T &create(index_t e, const T& arg)
		{
			return *(T*)nullptr;
		}

		void batch_remove(const and_chbv& remove)
		{
		}

		void remove(index_t e)
		{
		}
	};

	DefStorage(null_storage)
	{
	public:
		DefConstructor(null_storage) : generic() {}

		auto &get(index_t e) noexcept
		{
			static_assert(false, "don't get from placeholder");
			return generic::get(e);
		}

		const auto &get(index_t e) const noexcept
		{
			static_assert(false, "don't get from placeholder");
			return generic::get(e);
		}
	};
}