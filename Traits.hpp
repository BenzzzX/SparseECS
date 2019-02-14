#pragma once

namespace ecs
{
	template<typename T>
	struct hbv_map_trait;

	/*
	定义一个component管理器并启用自动分派
	参数为(component类型,容器类型)
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