#pragma once
#include "Entities.hpp"
#include "Components.hpp"
#include "Traits.hpp"
#include <execution>
#include <algorithm>


namespace ecs
{
	template<typename T>
	struct is_atomic_arg : std::true_type {};
	template<typename T>
	struct is_atomic_arg<std::reference_wrapper<T>> : std::false_type {};
	template<typename T>
	struct is_atomic_arg<T&> : std::false_type {};


	template<typename... Ts>
	using view = std::tuple<Ts&...>;

	namespace view_detail
	{
		//这里需要无视const限定符的get
		template<typename T, typename C>
		static constexpr decltype(auto) nonstrict_get(C& c)
		{
			using nonstrict_type = std::conditional_t
			<
				contain_v
				<
					T,
					rewrap_t<typelist, C>
				>,
				T,
				std::remove_reference_t<T> const&
			>;
			return std::get<nonstrict_type>(c);
		}

		template<typename T>
		struct is_filter : std::false_type {};

		template<typename T>
		using is_hbv_map_element = common::is_complete<hbv_map_trait<T>>;

		template<typename... Ts>
		struct compound_filter_helper
		{
			template<typename T, typename S>
			__forceinline static decltype(auto) pick(S &components) noexcept
			{
				using type = typename hbv_map_trait<T>::hbv_map;
				return nonstrict_get<type&>(components).filter();
			}

			template<typename S>
			__forceinline static decltype(auto) call(S &components) noexcept
			{
				return common::and(pick<Ts>(components)...);
			}
		};

		template<typename... Ts>
		struct iterator_helper
		{
			template<typename T, typename S>
			__forceinline static decltype(auto) pick(S &components, index_t id) noexcept
			{
				if constexpr(is_hbv_map_element<T>{})
				{
					using type = typename hbv_map_trait<T>::hbv_map;
					return nonstrict_get<type&>(components).get(id);
				}
				else
				{
					return nonstrict_get<T&>(components);
				}
			}

			template<typename F, typename S>
			__forceinline static void call(S &components, index_t id, F&& f) noexcept
			{
				f(pick<Ts>(components, id)...);
			}
		};

		/*简单的线性遍历*/
		struct seq
		{
			template<typename T, typename F>
			__forceinline static void for_each(const T& vec, const F& f) noexcept
			{
				common::for_each(vec, f);
			}
		};

		/*基于std的并行遍历*/
		struct par
		{
			/*
			单线程遍历前三层,子线程遍历最后一层并执行函数
			注意,当函数成本很小的时候,这种方法反而会降低速度(分配内存消耗较大
			*/
			template<typename T, typename F>
			__forceinline static void for_each(const T& vec, const F& f) noexcept
			{
				std::vector<index_t> indicesBuffer;
				indicesBuffer.reserve(512u);
				common::for_each<2>(vec, [&indicesBuffer](index_t id)
				{
					indicesBuffer.push_back(id);
				});
				using namespace common::hbv_detail;
				std::for_each(std::execution::par, std::begin(indicesBuffer), std::end(indicesBuffer), [&f, &vec](index_t id)
				{
					flag_t node = vec.layer3(id);
					index_t prefix = id << BitsPerLayer;
					do
					{
						index_t low = lowbit_pos(node);
						node &= ~(flag_t(1) << low);
						f(prefix | low);
					} while (node);
				});
			}
		};

		//此处黑魔法
		template<typename F>
		struct implict_view_helper
		{
			using function_info = common::generic_function_trait<std::decay_t<F>>;
			using requests = common::remove_t<index_t, typename function_info::argument_type>;
			template<typename T>
			struct upgrade_helper
			{
				using type = std::conditional_t<is_hbv_map_element<T>{}, typename hbv_map_trait<T>::hbv_map, T> ;
			};
			template<typename T>
			using upgrade_helper_t = typename upgrade_helper<std::decay_t<T>>::type;
			template<typename T>
			using upgrade = std::conditional_t<is_atomic_arg<T>::value, std::add_const_t<upgrade_helper_t<T>>, upgrade_helper_t<T>>;
			//把参数升级到资源:从元素得到容器
			using need_assets = common::map_t<upgrade, requests>;
			//可能会有重复的参数,非const覆盖const
			template<typename T>
			using keep_mutable = std::negation<std::conjunction<common::is_const<T>, common::contain<common::remove_const_t<T>, need_assets>>>;
			using assets = common::filter_t<keep_mutable, common::unique_t<need_assets>>;
			using view = common::rewrap_t<view, assets>;
		};

		/*
		获取一个 job 隐含的 view,job 需满足函数概念: filter_list(request_list...)
		如 some_job(some_component&, fuck&)
		规则:
			对于参数
				1. const 引用,share 对应的资源
				2. 引用,borrow 对应的资源
				3. 左值,share 对应的资源
		*/
		template<typename F>
		using implict_view = typename implict_view_helper<F>::view;

		/*
		在 view 上执行一个逻辑(可通过模板参数指定迭代策略),逻辑需满足函数概念: filter_list(request_list...)
		如 some_job(some_component&, fuck&)
		执行流程: 
			1. 对于 componen 和 entity 参数,添加对应的 has filter
			2. 如果
				1. filter 数量大于零,根据 filter 筛选 entity 调用
				2. filter 数量等于零,直接调用一次
			   调用时 job 的参数将自动填充
		特殊的,job 可以通过 index_t 参数直接获取当前索引,通常用于 remove 事件
		*/
		template<typename iterator_strategy, typename S, typename F>
		void for_view(S view, F&& job)
		{
			using namespace common;
			using function_info = generic_function_trait<std::decay_t<F>>;
			using requests = map_t<std::decay_t, typename function_info::argument_type>;
			using elements = filter_t<is_hbv_map_element, requests>;
			if constexpr (size<elements> == 0)
			{
				static_assert(!contain_v<index_t, requests>, "index is not making sense without filter!");
				rewrap_t<iterator_helper, requests>::call(view, 0, job);
			}
			else
			{
				const auto filter = rewrap_t<compound_filter_helper, elements>::call(view);
				//通过 iterator policy 遍历执行
				iterator_strategy::for_each(filter, [&view, &job](index_t i)
				{
					rewrap_t<iterator_helper, requests>::call(view, i, job);
				});
			}
		}
	}

	using view_detail::implict_view;
	using view_detail::for_view;
	using view_detail::par;
	using view_detail::seq;

	template<typename... Ts>
	auto as_view(Ts&... args)
	{
		return view<Ts...>{args...};
	}
}