#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include <intrin.h>

namespace common
{
	namespace hbv_detail
	{
		using index_t = uint32_t;
		using flag_t = int64_t;

		//取得最低的一位标志位的位置
		//001000 -> 3
		__forceinline index_t lowbit_pos(flag_t id)
		{
			unsigned long result;
			return _BitScanForward64(&result, id) ? result : 0;
		}

		//取得最高的一位标志位的位置
		__forceinline index_t highbit_pos(flag_t id)
		{
			unsigned long result;
			return _BitScanReverse64(&result, id) ? result : 0;
		}


		//分层位数组的常数,硬编码,勿动😀

		//每个节点64位,即2<<6
		constexpr index_t BitsPerLayer = 6u;
		constexpr flag_t EmptyNode = 0u;
		constexpr flag_t FullNode = EmptyNode - 1u;
		//硬编码分层为4层,方便优化和编码
		constexpr index_t LayerCount = 4u;

		//节点位置
		template<index_t layer>
		constexpr index_t index_of(index_t id) noexcept
		{
			return id >> ((LayerCount - layer)*BitsPerLayer);
		}

		//节点值
		template<index_t layer>
		constexpr flag_t value_of(index_t id) noexcept
		{
			constexpr index_t mask = (1 << BitsPerLayer) - 1;
			index_t index = index_of<layer + 1>(id);
			return flag_t(1u) << (index & mask);
		}

		//为了减少内存消耗,当大量连续位没有被使用时,释放block
		//注意此类只用于hbv
		class block_vector
		{
			static constexpr index_t bits = BitsPerLayer;
			static constexpr index_t mask = (1 << bits) - 1;
			std::vector<flag_t*> _blocks;
			index_t _size = 0;
		public:
			~block_vector()
			{
				clear();
			}
			flag_t & operator[](index_t i)
			{
				return _blocks[i >> bits][i & mask];
			}

			flag_t operator[](index_t i) const
			{
				return _blocks[i >> bits][i & mask];
			}

			bool valid(index_t i) const
			{
				return _blocks[i >> bits] != nullptr;
			}

			index_t size() const
			{
				return _size;
			}

			void clear()
			{
				for (int i = 0; i < _blocks.size(); ++i)
					try_erase_block(i);
			}

			void resize(index_t n)
			{
				index_t b = n >> bits;
				_blocks.resize(b + 1, nullptr);
				_size = n;
			}

			void erase_block(index_t i)
			{
				free(_blocks[i]);
				_blocks[i] = nullptr;
			}

			void try_erase_block(index_t i)
			{
				if (_blocks[i] != nullptr)
					erase_block(i);
			}

			void add_block(index_t i)
			{
				_blocks[i] = (flag_t*)malloc(sizeof(flag_t) * (1 << bits));
				memset(_blocks[i], 0, sizeof(flag_t) * (1 << bits));
			}

			void try_add_block(index_t i)
			{
				if (_blocks[i] == nullptr)
					add_block(i);
			}

			void reset(index_t begin, index_t end)
			{
				index_t b = end >> bits;
				index_t s = begin >> bits;
				for (index_t i = s + 1; i < b; ++i)
					try_erase_block(i);
				if (b > s)
				{
					if (_blocks[s] != nullptr)
						memset(_blocks[s] + (begin & mask), -1, ((1 << bits) - (begin & mask)) * sizeof(flag_t));
					if (_blocks[b] != nullptr)
						memset(_blocks[b], -1, (end & mask) * sizeof(flag_t));
				}
				else if (_blocks[s] != nullptr)
				{
					memset(_blocks[s] + (begin & mask), -1, ((end - begin) & mask) * sizeof(flag_t));
				}
			}

			void fill(index_t begin, index_t end)
			{
				index_t b = end >> bits;
				index_t s = begin >> bits;
				for (index_t i = s; i <= b; ++i)
					try_add_block(i);
				for (index_t i = s + 1; i < b; ++i)
					memset(_blocks[i], -1, (1 << bits) * sizeof(flag_t));
				if (b > s)
				{
					memset(_blocks[s] + (begin & mask), -1, ((1 << bits) - (begin & mask)) * sizeof(flag_t));
					memset(_blocks[b], -1, (end & mask) * sizeof(flag_t));
				}
				else
				{
					memset(_blocks[s] + (begin & mask), -1, ((end - begin) & mask) * sizeof(flag_t));
				}
			}

			void resize(index_t n, bool fill)
			{
				index_t b = n >> bits;
				_blocks.resize(b + 1, nullptr);
				index_t s = _size >> bits;
				if (fill) this->fill(_size, n);
				_size = n;
			}
		};

		/*
		分层位数组(Hierarchical Bit Vector),利用额外的位数组来记录下层位数组的连续空位,整体符合如下规则
		Layer(n-1)[i] = Layer(n)[i] & Layer(n)[i + 1] & ... & Layer(n)[i + 63]
		通过跳过连续的空位来加速稀疏位数组的遍历,在数据紧密但位置分散的时候能取得很好的性能
		*/
		class hbv final
		{
			//硬编码4层
			flag_t _layer0;
			std::vector<flag_t> _layer1;
			std::vector<flag_t> _layer2;
			//block大小对应layer2
			block_vector _layer3;
			bool default_value;
		public:
			hbv(index_t max = 10, bool default_value = false) : default_value(default_value)
			{
				max -= 1;
				//简单的填充
				if (default_value)
				{
					_layer3.resize(index_of<3>(max) + 1, true);
					_layer2.resize(index_of<2>(max) + 1, FullNode);
					_layer1.resize(index_of<1>(max) + 1, FullNode);
					_layer0 = FullNode;
				}
				else
				{
					_layer3.resize(index_of<3>(max) + 1, false);
					_layer2.resize(index_of<2>(max) + 1, 0u);
					_layer1.resize(index_of<1>(max) + 1, 0u);
					_layer0 = 0u;
				}
			}

			//生长容量,填入更小的容量无效
			void grow_to(index_t to) noexcept
			{
				to -= 1;
				to = std::min<index_t>(16'777'216u, to);
				if (to < size()) return;
				if (default_value)
				{
					_layer3.resize(index_of<3>(to) + 1, true);
					_layer2.resize(index_of<2>(to) + 1, FullNode);
					_layer1.resize(index_of<1>(to) + 1, FullNode);
				}
				else
				{
					_layer3.resize(index_of<3>(to) + 1, false);
					_layer2.resize(index_of<2>(to) + 1, 0u);
					_layer1.resize(index_of<1>(to) + 1, 0u);
				}
			}

			index_t size() const noexcept
			{
				return ((_layer3.size() - 1) << BitsPerLayer) + 1;
			}

			//范围设置标志位,性能大幅高于依次设置
			void range_set(index_t begin, index_t end, bool value)
			{
				if (value)
					set_range_true(begin, end);
				else
					set_range_false(begin, end);
			}

			//设置标志位, 性能一般
			void set(index_t id, bool value) noexcept
			{
				index_t index_3 = index_of<3>(id);
				flag_t value_3 = value_of<3>(id);

				if (value)
				{
					//bubble for new node
					bubble_fill(id);
					_layer3[index_3] |= value_3;
				}
				else
				{
					//bubble for empty node
					_layer3[index_3] &= ~value_3;
					bubble_empty(id);
				}
			}

			//判断特定位, 性能一般
			bool test(index_t id) const noexcept
			{
				index_t index_3 = index_of<3>(id);
				if (index_3 > _layer3.size()) return default_value;
				return  _layer3.valid(index_3) && (_layer3[index_3] & value_of<3>(id));
			}

			//清零位数组
			void clear() noexcept
			{
				if (default_value)
				{
					_layer3.fill(0, _layer3.size());
					std::fill(_layer2.begin(), _layer2.end(), FullNode);
					std::fill(_layer1.begin(), _layer1.end(), FullNode);
					_layer0 = FullNode;
				}
				else
				{
					_layer3.clear();
					std::fill(_layer2.begin(), _layer2.end(), 0u);
					std::fill(_layer1.begin(), _layer1.end(), 0u);
					_layer0 = 0u;
				}
			}

			//直接读指定层标志位
			flag_t layer0() const noexcept
			{
				return _layer0;
			}

			flag_t layer1(index_t id) const noexcept
			{
				if (id >= _layer1.size())
					return default_value ? FullNode : 0u;
				return _layer1[id];
			}

			flag_t layer2(index_t id) const noexcept
			{
				if (id >= _layer2.size())
					return default_value ? FullNode : 0u;
				return _layer2[id];
			}

			flag_t layer3(index_t id) const noexcept
			{
				if (id >= _layer3.size())
					return default_value ? FullNode : 0u;
				return _layer3[id];
			}

			flag_t layer(index_t level, index_t id) const noexcept
			{
				switch (level)
				{
				case 0:
					return layer0();
				case 1:
					return layer1(id);
				case 2:
					return layer2(id);
				case 3:
					return layer3(id);
				default:
					return 0;
				}
			}

			//合并位数组, 性能高于普通遍历
			//集合加(或) 
			template<typename T>
			void merge_add(const T& vec)
			{
				std::array<flag_t, LayerCount - 1> nodes{};
				std::array<index_t, LayerCount - 1> prefix{};
				nodes[0] = vec.layer0();
				index_t size = last(vec) + 1;
				grow_to(size);
				index_t level = 0;
				if (nodes[0] == EmptyNode) return;
				for (;;)
				{
					index_t low = lowbit_pos(nodes[level]);
					nodes[level] &= ~(flag_t(1u) << low);
					index_t id = prefix[level] | low;

					++level;
					if (level == 3)
					{
						flag_t node = vec.layer3(id);
						bubble_fill(id << BitsPerLayer);
						_layer3[id] |= node;
						do
						{
							//root is empty, stop iterating
							if (level == 0)
								return;
							--level;
						} while (nodes[level] == EmptyNode);
					}
					else
					{
						nodes[level] = vec.layer(level, id);
						prefix[level] = id << BitsPerLayer;
					}
				}
			}

			//集合减(与非)
			template<typename T>
			void merge_sub(const T& vec)
			{
				std::array<flag_t, LayerCount - 1> nodes{};
				std::array<index_t, LayerCount - 1> prefix{};
				nodes[0] = vec.layer0() & _layer0;
				index_t level = 0;
				if (nodes[0] == EmptyNode) return;
				for (;;)
				{
					index_t low = lowbit_pos(nodes[level]);
					nodes[level] &= ~(flag_t(1u) << low);
					index_t id = prefix[level] | low;

					++level;
					if (level == 3)
					{
						if (id >= _layer3.size()) return;

						flag_t node = vec.layer3(id);
						_layer3[id] &= ~node;
						bubble_empty(id << BitsPerLayer);
						do
						{
							//root is empty, stop iterating
							if (level == 0)
								return;
							--level;
						} while (nodes[level] == EmptyNode);
					}
					else
					{
						nodes[level] = vec.layer(level, id) & layer(level, id);
						prefix[level] = id << BitsPerLayer;
					}
				}
			}
			
			
			
		private:
			void bubble_empty(index_t id)
			{
				//简单的尝试上浮空节点,直到遇到非空节点位置
				index_t index_3 = index_of<3>(id);
				if (_layer3[index_3] != EmptyNode) return;
				index_t index_2 = index_of<2>(id);
				_layer2[index_2] &= ~value_of<2>(id);
				if (_layer2[index_2] != EmptyNode) return;
				index_t index_1 = index_of<1>(id);
				_layer1[index_1] &= ~value_of<1>(id);
				if (_layer1[index_1] != EmptyNode) return;
				_layer3.erase_block(index_2);
				_layer0 &= ~value_of<0>(id);
			}

			void bubble_fill(index_t id)
			{
				index_t index_3 = index_of<3>(id);
				_layer3.try_add_block(index_of<2>(id));
				if (_layer3[index_3] == EmptyNode)
				{
					//直接修改父节点,1 | 1 = 1
					_layer2[index_of<2>(id)] |= value_of<2>(id);
					_layer1[index_of<1>(id)] |= value_of<1>(id);
					_layer0 |= value_of<0>(id);
				}
			}

			void set_range_true(index_t begin, index_t end)
			{
				index_t startPos = begin;
				index_t endPos = end - 1;

				//范围设置可以直接覆盖父节点的边界
				//主要需要注意的为范围在同一节点内的情况

				if (index_of<3>(startPos) == index_of<3>(endPos))
				{
					_layer3.try_add_block(index_of<2>(startPos));
					_layer3[index_of<3>(startPos)] |= (value_of<3>(endPos) - value_of<3>(startPos)) + value_of<3>(endPos);
				}
				else
				{
					index_t start = index_of<3>(startPos);
					index_t end = index_of<3>(endPos);
					if (start + 1 < end)
						_layer3.fill(start + 1, end);
					_layer3.try_add_block(index_of<2>(startPos));
					_layer3.try_add_block(index_of<2>(endPos));
					_layer3[start] |= FullNode - value_of<3>(startPos) + 1;
					_layer3[end] |= (value_of<3>(endPos) - 1) + value_of<3>(endPos);
				}

#define			SET_LAYER12(N) \
				if (index_of<N>(startPos) == index_of<N>(endPos)) \
				{ \
					_layer##N[index_of<N>(startPos)] |= (value_of<N>(endPos) - value_of<N>(startPos)) + value_of<N>(endPos); \
				} \
				else \
				{ \
					index_t start = index_of<N>(startPos); \
					index_t end = index_of<N>(endPos); \
					if (start + 1 < end) \
						std::fill(&_layer##N[start + 1], &_layer##N[end], FullNode); \
					_layer##N[start] |= FullNode - value_of<N>(startPos) + 1; \
					_layer##N[end] |= (value_of<N>(endPos) - 1) + value_of<N>(endPos); \
				}

				SET_LAYER12(2);
				SET_LAYER12(1);

				_layer0 |= (value_of<0>(endPos) - value_of<0>(startPos)) + value_of<0>(endPos);
#undef		SET_LAYER12
			}

			void set_range_false(index_t begin, index_t end)
			{
				index_t startPos = begin;
				index_t endPos = end - 1;
				//对于范围清零则要复杂得多,因为父节点的边界并不能直接清零
				//而是需要先清零底层再进行冒泡
				if (index_of<2>(startPos) == index_of<2>(endPos))
				{
					if (value_of<2>(endPos) != value_of<2>(startPos))
						_layer2[index_of<2>(startPos)] &= ~(value_of<2>(endPos) - value_of<2>(startPos) * 2);
				}
				else
				{
					index_t start = index_of<2>(startPos);
					index_t end = index_of<2>(endPos);
					if (start + 1 < end)
						std::fill(&_layer2[start + 1], &_layer2[end], EmptyNode);
					_layer2[start] &= (value_of<2>(startPos) - 1) + value_of<2>(startPos);
					_layer2[end] &= FullNode - value_of<2>(endPos) + 1;
				}

				if (index_of<1>(startPos) == index_of<1>(endPos))
				{
					if (value_of<1>(endPos) != value_of<1>(startPos))
						_layer1[index_of<1>(startPos)] &= ~(value_of<1>(endPos) - value_of<1>(startPos) * 2);
				}
				else
				{
					index_t start = index_of<1>(startPos);
					index_t end = index_of<1>(endPos);
					if (start + 1 < end)
						std::fill(&_layer1[start + 1], &_layer1[end], EmptyNode);
					_layer1[start] &= (value_of<1>(startPos) - 1) + value_of<1>(startPos);
					_layer1[end] &= FullNode - value_of<1>(endPos) + 1;
				}

				if (index_of<3>(startPos) == index_of<3>(endPos) && _layer3.valid(index_of<3>(startPos)))
				{
					_layer3[index_of<3>(startPos)] &= ~((value_of<3>(endPos) - value_of<3>(startPos)) + value_of<3>(endPos));
					bubble_empty(startPos);
				}
				else
				{
					index_t start = index_of<3>(startPos);
					index_t end = index_of<3>(endPos);
					if (start + 1 < end)
						_layer3.reset(start + 1, end);
					if (_layer3.valid(start))
					{
						_layer3[start] &= ~(FullNode - value_of<3>(startPos) + 1);
						bubble_empty(startPos);
					}
					if (_layer3.valid(end))
					{
						_layer3[end] &= ~((value_of<3>(endPos) - 1) + value_of<3>(endPos));
						bubble_empty(endPos);
					}
				}
				_layer0 &= ~((value_of<0>(endPos) - value_of<0>(startPos)) + value_of<0>(endPos));
			}
		};

		/*
		复合分层位数组(Compound Hierarchical Bit Vector),惰性的在数组间应用函数,在查询时真正执行
		*/
		template<typename F, typename... Ts>
		class chbv
		{
			template<typename T>
			struct storage { using type = T; };
			template<>
			struct storage<hbv> { using type = const hbv&; };
			template<typename T>
			using storage_t = typename storage<T>::type;
			const std::tuple<storage_t<Ts>...> _nodes;
			F op = {};
		public:
			template<typename... Ts>
			chbv(Ts&&... args) : _nodes(std::forward<Ts>(args)...) {}

			flag_t layer0() const noexcept
			{
				return compose_layer0(std::make_index_sequence<sizeof...(Ts)>());
			}

			flag_t layer1(index_t id) const noexcept
			{
				return compose_layer1(id, std::make_index_sequence<sizeof...(Ts)>());
			}

			flag_t layer2(index_t id) const noexcept
			{
				return compose_layer2(id, std::make_index_sequence<sizeof...(Ts)>());
			}

			flag_t layer3(index_t id) const noexcept
			{
				return compose_layer3(id, std::make_index_sequence<sizeof...(Ts)>());
			}

			bool contain(index_t id) const noexcept
			{
				return compose_contain(id, std::make_index_sequence<sizeof...(Ts)>());
			}

			flag_t layer(index_t level, index_t id) const noexcept
			{
				switch (level)
				{
				case 0:
					return layer0();
				case 1:
					return layer1(id);
				case 2:
					return layer2(id);
				case 3:
					return layer3(id);
				default:
					return 0;
				}
			}
		private:

			template<index_t... i>
			flag_t compose_layer0(std::index_sequence<i...>) const noexcept
			{
				return op(std::get<i>(_nodes).layer0()...);
			}

			template<index_t... i>
			flag_t compose_layer1(index_t id, std::index_sequence<i...>) const noexcept
			{
				return op(std::get<i>(_nodes).layer1(id)...);
			}

			template<index_t... i>
			flag_t compose_layer2(index_t id, std::index_sequence<i...>) const noexcept
			{
				return op(std::get<i>(_nodes).layer2(id)...);
			}

			template<index_t... i>
			flag_t compose_layer3(index_t id, std::index_sequence<i...>) const noexcept
			{
				return op(std::get<i>(_nodes).layer3(id)...);
			}

			template<index_t... i>
			bool compose_contain(index_t id, std::index_sequence<i...>) const noexcept
			{
				return op(std::get<i>(_nodes).contain(id)...);
			}
		};

		struct and_op_t
		{
			template<typename... Ts>
			flag_t operator()(Ts... args) const
			{
				return (args & ...);
			}
		};

		struct or_op_t
		{
			template<typename... Ts>
			flag_t operator()(Ts... args) const
			{
				return (args | ...);
			}
		};

		struct not_op_t {};

		/*
		组合位数组的取反版本
		注意因为分层位数组的算法原理并不支持非操作, 此方法会使得分层位数组退化为普通位数组(慢!
		*/
		template<typename T>
		class chbv_not
		{
			template<typename T>
			struct storage { using type = T; };

			template<>
			struct storage<hbv> { using type = const hbv&; };

			template<typename T>
			using storage_t = typename storage<T>::type;

			storage_t<T> _node;
		public:
			template<typename T>
			chbv_not(T&& arg) : _node(std::forward<T>(arg)) { }

			flag_t layer0() const noexcept
			{
				return FullNode;
			}

			flag_t layer1(index_t id) const noexcept
			{
				return FullNode;
			}

			flag_t layer2(index_t id) const noexcept
			{
				return FullNode;
			}

			flag_t layer3(index_t id) const noexcept
			{
				return ~_node.layer3(id);
			}

			bool contain(index_t id) const noexcept
			{
				return ~_node.contain(id);
			}

			flag_t layer(index_t level, index_t id) const noexcept
			{
				switch (level)
				{
				case 0:
					return layer0();
				case 1:
					return layer1(id);
				case 2:
					return layer2(id);
				case 3:
					return layer3(id);
				default:
					return 0;
				}
			}
		};

		

		//取得位数组(或组合位数组)的第一个标志位
		template<index_t Level = 3, typename T>
		int32_t last(const T& vec) noexcept
		{
			flag_t nodes{};
			nodes = vec.layer0();
			if (nodes == EmptyNode) return -1;
			index_t id{};

			for (int32_t level = 0;; ++level)
			{
				index_t high = highbit_pos(nodes);
				id |= high;
				if (level >= Level)
					return id;
				id <<= BitsPerLayer;
				nodes = vec.layer(level + 1, id);
			}
		}

		//取得位数组(或组合位数组)的最后一个标志位
		template<index_t Level = 3, typename T>
		int32_t first(const T& vec) noexcept
		{
			flag_t nodes{};
			nodes = vec.layer0();
			if (nodes == EmptyNode) return -1;
			index_t id{};

			for (int32_t level = 0;; ++level)
			{
				index_t high = lowbit_pos(nodes);
				id |= high;
				if (level >= Level)
					return id;
				id <<= BitsPerLayer;
				nodes = vec.layer(level + 1, id);
			}
		}

		//遍历位数组(或组合位数组)
		template<index_t Level = 3, typename T, typename F>
		void for_each(const T& vec, const F& f) noexcept
		{
			std::array<flag_t, Level + 1> nodes{};
			std::array<index_t, Level + 1> prefix{};
			nodes[0] = vec.layer0();
			index_t level = 0;
			if (nodes[0] == EmptyNode) return;

			while (true)
			{
				//遍历节点
				index_t low = lowbit_pos(nodes[level]);
				nodes[level] &= ~(flag_t(1u) << low);
				index_t id = prefix[level] | low;
				//上层节点,遍历子节点
				if (level < Level)
				{
					++level;
					nodes[level] = vec.layer(level, id);
					prefix[level] = id << BitsPerLayer;
				}
				else
				{
					f(id);
					//子节点遍历完,回到上层节点
					while (nodes[level] == EmptyNode)
					{
						//直到Layer0被遍历完
						if (level == 0)
							return;
						--level;
					}
				}
			}
		}

		//组合位数组
		template<typename... Ts>
		__forceinline chbv<and_op_t, std::decay_t<Ts>...> and(Ts&&... args)
		{
			return { std::forward<Ts>(args)... };
		}

		template<typename... Ts>
		__forceinline chbv<or_op_t, std::decay_t<Ts>...> or(Ts&&... args)
		{
			return { std::forward<Ts>(args)... };
		}

		//取反位数组
		//注意因为分层位数组的算法原理并不支持非操作
		//此方法会使得分层位数组退化为普通位数组(慢!
		template<typename T>
		__forceinline chbv_not<std::decay_t<T>> not(T&& arg)
		{
			return { std::forward<T>(arg) };
		}

		//判断位数组(或组合位数组)是否为空
		template<typename T>
		__forceinline bool empty(const T& vec)
		{
			return vec.layer0() == 0u;
		}
	}

	using hbv_detail::index_t;
	using hbv_detail::hbv;
	using hbv_detail::and;
	using hbv_detail::or ;
	using hbv_detail::not;
	using hbv_detail::empty;
	using hbv_detail::last;
	using hbv_detail::first;
	using hbv_detail::for_each;
}
