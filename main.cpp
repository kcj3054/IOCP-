#include "GameServer.h"
#include <string>
#include <iostream>



const UINT16 SERVER_PORT = 7700;
const UINT16 MAX_CLIENT = 100;		//�� �����Ҽ� �ִ� Ŭ���̾�Ʈ ��
const UINT32 MAX_IO_WORKER_THREAD = 4;  //������ Ǯ�� ���� ������ ��

/*
* - smart pointer�� �ϴ�  raw pointer�� wrapping �� ���̴�. 
* - �Ʒ� class MyInt�� �����ϰ� ����Ʈ�����Ͷ�� �� �� �ִ� ���� refcount�� ����.
* - �������� ������ new�� ������ object�� life time�� �����Ѵٰ� �� �� �ִ�. 
*	- �Ƹ����� MyInt Ÿ������ �ν��Ͻ̵� ��ü�� ���� delete�� ���� �ʰ� MyInt���� ���������� delete�� ���ش�.
*	- ���� �����Ͱ� �������ٸ� new - delete�� ����ٴ� ���� �Ұ���. ����� �ϴ� ���� �Ǽ��� ������ ����.!
*/
class MyInt
{
public:
	explicit MyInt(int* p = nullptr)
	{
		data = p;
	}

	~MyInt() { delete data; }

	int& operator * () { return *data; }
private:
	int* data{ nullptr };
};

class Foo
{
public:
	Foo(int x) : _x{x} {}
	int getX() { return _x; }
	~Foo() { std::cout << "~Foo" << std::endl; }
private:
	int _x = 0;
};

void fun(std::shared_ptr<Foo> sp)
{
	std::cout << "fun : " << sp.use_count() << std::endl;
}

int main()
{

	//GameServer server;

	////������ �ʱ�ȭ
	//server.Init(MAX_IO_WORKER_THREAD);

	////���ϰ� ���� �ּҸ� �����ϰ� ��� ��Ų��.
	//server.BindandListen(SERVER_PORT);

	//server.Run(MAX_CLIENT);
	//while (true)
	//{
	//	;
	//}
	//return 0;

	//int* p = new int(10);
	//MyInt myint = MyInt(p); // P�� POINTER�� PASSING TO MyInt ... p�����͸� MyInt�� �ѱ��. 
	//std::cout << *myint << std::endl;

	//shared_ptr<> p(new Foo()).. 
	// sp0, sp1, sp2... Foo��ü�� ����Ű�� ����Ʈ�����͵��� �����ϳ�.
	// -> � ��ü����, �� �ٸ� ���� control block ->,,, 

	//shared_ptr���� reference count�� control block�� thread safe������, � ��ü���� ����Ű�� T*�� thread safe
	// ���� �ʴ�. 

	// make_shared�� �̸� �޸� �Ҵ��� �� ��ü Ÿ���� ���ϴ� ���̶� new Ű���尡 ���� 
	// 
	std::shared_ptr<Foo> sp = std::make_shared<Foo>(200);
	std::cout << sp.get() << std::endl;
	std::cout << sp.use_count() << std::endl; // reference count, -> thread safe
	std::shared_ptr<Foo> sp1(new Foo(100));
	std::shared_ptr<Foo> sp2 = sp;
	std::cout << sp.use_count() << std::endl;


	std::thread t1(fun, sp), t2(fun, sp), t3(fun, sp);

	std::cout << "main : " << sp.use_count() << std::endl;
	t1.join(); t2.join(); t3.join();
	//�ָ��� ���� ~ deconstructor�� �ѹ��� ȣ��ȴ�. 
}

/*
* [�� �����]
*  -> �ܼ� �ش� ������ ������ ���� �ƴ϶� �ٸ� ���������׵� ���� ���� �˷�����Ѵ�. �� ������ broadcasting.
* 
* �ǹ��ڵ�� ����ڵ尡 �߿��ϴ�. 
* 
* -> io�� ���ϰ� ũ��, ���� ó�� ������ʹ� �и��ϴ� ���� ����
*/