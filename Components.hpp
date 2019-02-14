#pragma once
#include "HBV.hpp"
#include "MPL.hpp"

namespace ecs
{
	using index_t = common::index_t;
	namespace component_detail
	{
		using and_chbv = decltype(common::and(common::hbv{}, common::hbv{}));

		//������ϸ���,�ṩ����ӿ�,�ṩ������
		struct components_abstract
		{
			virtual bool contain(index_t e) const = 0;
			virtual void remove(index_t e) = 0;
			virtual void instantiate(index_t e, index_t proto) = 0;
			virtual void batch_instantiate(index_t begin, index_t end, index_t proto) = 0;
			virtual void batch_remove(const common::hbv& remove) = 0;
			virtual ~components_abstract() {}
		};

		//������Ϸ���,�ṩ����ʵ��
		template<template<typename> class C, typename T>
		class components_generic : public components_abstract
		{
		protected:
			common::hbv _has;
			
			//���ڲ���ӵ��Ĭ��ʵ�ֵĿ�ѡ�ӿ�
			template<typename T>
			using batch_create_trait = decltype(&T::batch_create);
			template<typename T>
			using batch_remove_trait = decltype(&T::batch_remove);
			template<typename T>
			using instantiate_trait = decltype(&T::instantiate);

		public:

			template<typename... Ts>
			components_generic(Ts&&... args) noexcept : container(std::forward<Ts>(args)...) {}
			~components_generic() noexcept
			{
				batch_remove(_has);
			}

			C<T> container;
			using type  = T;
			template<typename T>
			using storage = C<T>;

			decltype(auto) filter() const noexcept
			{
				return (const common::hbv&)_has;
			}

			decltype(auto) get(index_t e) noexcept
			{
				return container.get(e);
			}

			decltype(auto) get(index_t e) const noexcept
			{
				return container.get(e);
			}

			decltype(auto) create(index_t e, const T& arg)  noexcept
			{
				if (contain(e))
					container.remove(e);
				return container.create(e, arg);
			}

			void instantiate(index_t e, index_t proto) noexcept
			{
				if constexpr(common::is_detected<instantiate_trait, C<T>>::value)
					container.instantiate(e, proto);
				else
					container.create(e, container.get(proto));
			}

			bool contain(index_t e) const noexcept
			{
				return _has.test(e);
			}

			void remove(index_t e) noexcept
			{
				container.remove(e);
			}

			//��������һ������������
			void batch_create(index_t begin, index_t end, const T& arg) noexcept
			{
				if constexpr(common::is_detected<batch_create_trait, C<T>>::value)
				{
					container.batch_create(begin, end, arg);
				}
				else
				{
					for (index_t i = begin; i < end; ++i)
						container.create(i, arg);
				}
			}

			void batch_instantiate(index_t begin, index_t end, index_t proto) noexcept
			{
				const T& prototype = container.get(proto);
				batch_create(begin, end, prototype);
			}

			//����ɾ��һ��hbv��ǵļ���
			void batch_remove(const common::hbv& remove) noexcept
			{
				//��Ч��ɾ��
				auto toRemove = common::and(remove, _has);
				if constexpr(common::is_detected<batch_remove_trait, C<T>>::value)
				{
					container.batch_remove(toRemove);
				}
				else
				{
					common::for_each(toRemove, [this](index_t i)
					{
						container.remove(i);
					});
				}
			}
		};
	}
	using component_detail::and_chbv;

	/*
	components �� component �Ĺ�����
	components ���� hbv_map<id, component> �ĸ���,�ṩ filter �� get ����
	filter ���� tracer,Ĭ��ֻ�ṩ has tracer,�������û�����
	hbv_map �е� map ���û��Զ���, Ĭ���ṩһЩʵ��(�ο�Storage)
	*/
	template<typename T, template<typename> class C = sparse_vector>
	class components/*_specialization*/ final : public component_detail::components_generic<C, T>
	{
		using generic = component_detail::components_generic<C, T>;
	public:
		components() noexcept : generic() {}
	};

#define DefStorage(name) \
	template<typename T> \
	class components<T, name> final : public component_detail::components_generic<name, T>
#define DefConstructor(name) \
    using generic = component_detail::components_generic<name, T>; \
	components() noexcept

	
}