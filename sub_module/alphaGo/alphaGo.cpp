
#include "alphaGo.hpp"
#include <vector>
/*************************************************************************
* @file   : EncodeDecode.c
* @author : sun
* @mail   : perfectsun1990@163.com
* @version: v1.0.0
* @date   : 2018年08月02日 星期四 10时26分12秒
* Copyright (C), 1990-2020, Tech.Co., Ltd. All rights reserved.
************************************************************************/

#include "encrypt.hpp"
#include "thrdspool.hpp"
#include <iostream>

int32_t
StrFun(std::string str, std::function<void(std::string&&)> callback)
{
	int32_t i = 0;
	for (i = 0; i < 3; i++) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		std::cout << "->StrFun exec:" << i << " str=" << str.c_str()
			<< " thr_id:" << std::this_thread::get_id() << std::endl;
	}
	if (callback) callback("StrFun is finished!");
	return i;
}
void func(void) { std::cout << "task func exec" << std::endl; }

void test_thrdspool()
{
	// 线程池的测试
	std::shared_ptr<ThrdsPool> thr_pool = ThrdsPool::create(16);
	std::future<int32_t> future_return1 = thr_pool->commit(StrFun, "StrFun is started!", [](std::string&& str)
	{
		std::cout << "=======>>>>>>>>>>>>>>  Test 1 callback string=" << str.c_str()
			<< " thr_id:" << std::this_thread::get_id() << std::endl;
	});
	std::cout << "=======  ThrdsPool test =========" << std::endl;
	std::cout << "StrFun retv=" << future_return1.get() << " thr_id:" << std::this_thread::get_id() << std::endl;
	std::cout << "=======  ThrdsPool over =========" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1));

	std::cout << "=======  TaskQueue test =========" << std::endl;
	Task task;

	// 任务队列测试
	std::shared_ptr<TaskQueue> pTaskQue = TaskQueue::create();
	pTaskQue->push(std::bind(func));
	pTaskQue->popd(task);
	task();
	std::cout << "=======  TaskQueue over =========" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1));
	
	// 线程代理测试
	std::cout << "=======  ThrdProxy test =========" << std::endl;
	std::shared_ptr<ThrdProxy> pThrds = ThrdProxy::create(task);
	pThrds->start(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	pThrds->stopd();
	std::cout << "=======  ThrdProxy over =========" << std::endl;
}

int32_t main(int32_t argc, char** argv)
{
	
	test_thrdspool();
	// DoEncodeDecode("E:\\av-test\\test.ccr", "E:\\av-test\\test-encode.ccr");
	//DoEncodeDecode("E:\\av-test\\test-encode.ccr", "E:\\av-test\\test-decode.ccr");
	system("pause");
}
