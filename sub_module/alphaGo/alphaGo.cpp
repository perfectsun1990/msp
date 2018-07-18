
#include "alphaGo.hpp"
#include <vector>

struct OBJ_1
{
	OBJ_1() {
		std::cout << "OBJ_1()" << std::endl;
	}
	OBJ_1(int a) :value(a) {
		std::cout << "OBJ_1(int a)" << std::endl;
	}
	int value = 0;
};

struct OBJ_2
{
	int a;
	int b;
};
int main()
{
	MemPool<OBJ_2> mpool;// ok

	std::vector<OBJ_1*> obj_table;
	MemPool<OBJ_1> *pool = new MemPool<OBJ_1>(2);

#if 1
#define TEST_OBJ_NUMS 8
	OBJ_1* obj_tmp = nullptr;
	for (int i = 0; i < TEST_OBJ_NUMS; i++) {
		obj_tmp = pool->New(i);
		std::cout << "create : " << obj_tmp << std::endl;
		obj_table.push_back(obj_tmp);
	}
	std::cout << "===============================================" << std::endl;
	for (int i = 0; i < TEST_OBJ_NUMS / 2; i++) {
		obj_tmp = obj_table.back();
		std::cout << "destroy : " << obj_tmp << std::endl;
		pool->Delete(obj_tmp);
		obj_table.pop_back();
	}
	std::cout << "===============================================" << std::endl;
	for (int i = 0; i < TEST_OBJ_NUMS / 2; i++) {
		std::cout << "CREAT SUCCESSFUL" << std::endl;
		obj_tmp = pool->New();
		std::cout << "creat : " << obj_tmp << std::endl;
		obj_table.push_back(obj_tmp);
	}
	std::cout << "===============================================" << std::endl;
	std::cout << obj_table.size() << std::endl;
	delete pool;
#endif

	system("pause");
	return 0;
}
