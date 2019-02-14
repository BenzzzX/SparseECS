#pragma once

namespace ecs
{
	template<typename T>
	struct hbv_map_trait;

	/*
	����һ��component�������������Զ�����
	����Ϊ(component����,��������)
	*/
#define Component(ctype, cstorage, ...) \
	ecs::components<ctype, cstorage, __VA_ARGS__>; \
	template<> \
	struct hbv_map_trait<ctype> \
	{ \
		using type = ctype; \
		using hbv_map = ecs::components<type, cstorage, __VA_ARGS__>; \
	}

	template<>
	struct hbv_map_trait<entity>
	{
		using type = entity;
		using hbv_map = entities;
	};
}