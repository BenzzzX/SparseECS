# Highlight
**ecs ģ���� header only ��.����Ҫ��� hbv �� storage ���.���岻���� 2k loc.**

## hbv
hbv ��**һ��** bitmap like �����ݽṹ,ά��ѹ���Ĳ�������  
��**����֧��**
* ����
* ���Ժϲ�
* �ϲ�

## storage
storage ��**һ��** map like �����ݽṹ  
ά��**����������**��ֵ�Ե�**��֧�ֱ���**

## hbv+storage = hbv-map
hbv-map �ǽ�һ�� hbv ��һ��(����)storage �����һ���һ���µ����ݽṹ.  
hbv-map ͬʱ֧��**��ѯ**,**����**��**���Ժϲ�**.

## hbv-map = components
ecs ģ��ʹ�� hbv-map ��ά�� component �ļ���,����
* hbv ��Ϊ tracer,���� entity ��Ӧ�� component ��״̬���¼�(��ӵ��״̬�ʹ����¼�).  
**����"ӵ��״̬ tracer" ���� tracer ��Ҫ�ֶ�����.**
* storage ��Ϊ����,��� entity ��Ӧ�� component ������.  
**storage ��������ѡ����������ʵ��.**

## hbv-map = entities
ecs ģ��ʹ�� hbv-map ��ά�� entity �ļ���,����
* hbv ���� entity ���������,���価���������� id ���������� id.
* storage ���� entity ����Ч����Ϣ,���Բ�ѯ.

## components + entities = ecs
ecs ģ������ hbv-map �ɺϲ���������ʵ�� ecs �ĺ����㷨:ɸѡ  
����ʵ�������� ecs ��ԭʼ�Ķ���,��ӵ�м��͵�����(�ǳ�С�ĳ�����

## ecs with batch
ecs ģ��Ϊ��Щ���ݽṹ��ʵ���� batch �汾���㷨���ṩ������,���а���:
* ��������
* ����ʵ����
* ����ɾ��(���û����ɼ�)

## ecs with view
ecs ģ�鶨���� view �ĸ���:һ���߼��Ŀɼ��ȷ�Χ,���а���:
* �߼��ɼ�����Դ�б�
* �߼�����Դ�Ŀɲ�����,Ŀǰֻ���ֶ�(����)/д(ռ��)

�� view �ĸ���֮�Ͽ��Դ system,**�� ecs ģ�鲢������ system**  
		
ecs ģ���ṩ�˻��� view �Ĺ���,���а���:
* �� view ��ִ�к���,�������������Զ����,�� component ���Զ�ʶ�𲢱���(����ѡ���������Ϊ����/����)
* ����һ������,�����Զ���ȡ���� view

## sample
���� ecs ģ��������ɵĴ�ϲ�ģ��,����Ϊһ������
```c++
#define View(...) ecs::view<__VA_ARGS__> view;
#define Job(p, f) ecs::for_view<p>(view, f)
#define Share const
//��������
struct Location { float x; };
struct Velocity { float x; };

namespace ecs
{	//�����ݶ���Ϊ component,����Ϊ sparse_vector
	using Locations = Component(Location, sparse_vector);
	using Velocities = Component(Velocity, sparse_vector);
}

//����һ�� system,����һ�� view ��һ�� update ����
struct SomeSystem
{
	View(Locations, Share Velocities);

	void Update()
	{
		Job(par, [this] //�����߼�
		(Location& loc, const Velocity& vel)
		{
			loc.x += vel.x;
		});
		Job(seq, [this] //�����߼�
		(const Location& loc, const Velocity&) 
		{
			std::cout << loc.x << "\n"; //����ƶ����λ��
		});
	}
};
```
