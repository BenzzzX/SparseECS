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
		//������Ҫ����const�޶�����get
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

		/*�򵥵����Ա���*/
		struct seq
		{
			template<typename T, typename F>
			__forceinline static void for_each(const T& vec, const F& f) noexcept
			{
				common::for_each(vec, f);
			}
		};

		/*����std�Ĳ��б���*/
		struct par
		{
			/*
			���̱߳���ǰ����,���̱߳������һ�㲢ִ�к���
			ע��,�������ɱ���С��ʱ��,���ַ��������ή���ٶ�(�����ڴ����Ľϴ�
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

		//�˴���ħ��
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
			//�Ѳ�����������Դ:��Ԫ�صõ�����
			using need_assets = common::map_t<upgrade, requests>;
			//���ܻ����ظ��Ĳ���,��const����const
			template<typename T>
			using keep_mutable = std::negation<std::conjunction<common::is_const<T>, common::contain<common::remove_const_t<T>, need_assets>>>;
			using assets = common::filter_t<keep_mutable, common::unique_t<need_assets>>;
			using view = common::rewrap_t<view, assets>;
		};

		/*
		��ȡһ�� job ������ view,job �����㺯������: filter_list(request_list...)
		�� some_job(some_component&, fuck&)
		����:
			���ڲ���
				1. const ����,share ��Ӧ����Դ
				2. ����,borrow ��Ӧ����Դ
				3. ��ֵ,share ��Ӧ����Դ
		*/
		template<typename F>
		using implict_view = typename implict_view_helper<F>::view;

		/*
		�� view ��ִ��һ���߼�(��ͨ��ģ�����ָ����������),�߼������㺯������: filter_list(request_list...)
		�� some_job(some_component&, fuck&)
		ִ������: 
			1. ���� componen �� entity ����,��Ӷ�Ӧ�� has filter
			2. ���
				1. filter ����������,���� filter ɸѡ entity ����
				2. filter ����������,ֱ�ӵ���һ��
			   ����ʱ job �Ĳ������Զ����
		�����,job ����ͨ�� index_t ����ֱ�ӻ�ȡ��ǰ����,ͨ������ remove �¼�
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
				//ͨ�� iterator policy ����ִ��
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