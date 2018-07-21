
#include "thrdspool.hpp"
#include <iostream>
#include <queue>

int gfunBlock(int slp)
{
	//printf("%s execute!  thrid=%d\n",__FUNCTION__, std::this_thread::get_id());
	std::this_thread::sleep_for(std::chrono::seconds(3));
	return  slp;
}

int gfun(int slp)
{
	printf("%s execute! value=%d thrid=%d\n", __FUNCTION__, slp, std::this_thread::get_id());
	return  slp;
}
struct gfunClass 
{
	int operator()(int n) {
		printf("%s execute!  thrid=%d\n", __FUNCTION__, std::this_thread::get_id());
		return 42;
	}
};

class A {    //函数必须是 static 的才能使用线程池
public:
	static int Afun(int n = 0) {
		printf("%s execute!  thrid=%d, n=%d\n", __FUNCTION__, std::this_thread::get_id(), n);
		return n;
	}
	static std::string Bfun(int n, std::string str, char c) {
		printf("%s execute!  thrid=%d, n=%d str=%s c=%c\n", __FUNCTION__, std::this_thread::get_id(), n, str.c_str(),c);
		return str;
	}
	int Cfun(int n = 0) {
		printf("%s execute!  thrid=%d, n=%d\n", __FUNCTION__, std::this_thread::get_id(), n);
		std::this_thread::sleep_for(std::chrono::seconds(1));
		return n;
	}

	int32_t StrFun(std::string str, std::function<void(std::string&&)> callback)
	{
		int32_t i = 0;
		for (i = 0; i< 3; i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			std::cout << "->StrFun exec:" << i << " str=" << str.c_str()
				<< " thr_id:" << std::this_thread::get_id() << std::endl;
		}
		if (callback) callback("StrFun is finished!");
		return i;
	}

};

int32_t 
StrFun(std::string str, std::function<void(std::string&&)> callback)
{	
	int32_t i = 0;
	for (i = 0; i< 3; i++){		
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		std::cout << "->StrFun exec:" << i << " str=" << str.c_str()
			<< " thr_id:" << std::this_thread::get_id() << std::endl;
	}
	if (callback) callback("StrFun is finished!");
	return i;
}

#define  MAX_THREADS_NUM   16

int main()
{
	std::thread([&]() {
		ThrPool thr_pool(MAX_THREADS_NUM);
	}).detach();

#if 1
	ThrPool thr_pool(MAX_THREADS_NUM);
	// Test 1  使用全局函数
	std::future<int32_t> future_return1 = thr_pool.post(StrFun, "StrFun is started!", [](std::string&& str)
	{
		std::cout << "=======>>>>>>>>>>>>>>  Test 1 callback string=" << str.c_str()
			<< " thr_id:" << std::this_thread::get_id() << std::endl;
	});
	std::cout << "=======  Test 1 post =========" << std::endl;
	std::cout << "StrFun retv=" << future_return1.get() << " thr_id:" << std::this_thread::get_id() << std::endl;
	std::cout << "=======  Test 1 over =========" << std::endl;

	std::this_thread::sleep_for(std::chrono::seconds(2));

	// Test 2  使用成员函数
	A impl;
	std::future<int32_t> future_return2 = thr_pool.post(std::bind(&A::StrFun, &impl, "StrFun is started!", [](std::string&& str)
	{
		std::cout << "=======>>>>>>>>>>>>>>  Test 2 callback string=" << str.c_str()
			<< " thr_id:" << std::this_thread::get_id() << std::endl;
	}));
	std::cout << "=======  Test 2 post =========" << std::endl;
	std::cout << "StrFun retv=" << future_return2.get() << " thr_id:" << std::this_thread::get_id() << std::endl;
	std::cout << "=======  Test 2 over =========" << std::endl;

	std::this_thread::sleep_for(std::chrono::seconds(2));

	std::cout << "=======  start all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool.idleCount() << std::endl;

	// 调用全局函数，仿函数，Lamada表达式.
	std::future<int> ff = thr_pool.post(gfun, 0);
	std::future<int> fg = thr_pool.post(gfunClass(), 0);
	std::future<std::string> fh = thr_pool.post([&]()->std::string 
	{
		printf("%s execute!  thrid=%d\n", __FUNCTION__, std::this_thread::get_id());
		return "fh test string";
	});

	// 调用类成员函数
	A a;
	std::future<int> gg = thr_pool.post(std::bind(&A::Cfun, &a, 9999)); 	
	std::future<std::string> gh = thr_pool.post(A::Bfun, 6666, "gh test string", 'B');
	std::future<int> gi = thr_pool.post(std::bind(&A::Afun, 1000)); 
	std::future<int> gj = thr_pool.post(std::bind(a.Afun, 400)); //IDE提示错误,但可以编译运行

	// 获取返回值，
	// 注意阻塞的函数，因为调用.get()获取返回值会等待线程执行完,此时调用get()会导致主程序阻塞。
	auto ret = gg.get();
	std::cout << "returm gg=" << ret << std::endl;
	std::cout << "returm gh=" << gh.get().c_str() << std::endl;
	std::cout << "returm fh=" << fh.get().c_str() << std::endl;

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	std::cout << "\n=======  beg post all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool.idleCount() << std::endl;
	std::vector< std::future<int> > results;
	for (int i = 0; i < MAX_THREADS_NUM+8; i++) 
	{
		results.emplace_back(std::move(thr_pool.post(gfunBlock, i)));
		int idle = thr_pool.idleCount();
		std::cout << "# thrsCount=" << thr_pool.thrsCount() << " idleCount=" << idle;
		if (idle == 0) std::cout << "-->Task link up: No idle threads!";
		std::cout << std::endl;
	}
	std::cout << " =======  end post all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool.idleCount() << std::endl;
	for (auto && result : results) std::cout << result.get() << ' ';
	std::cout << std::endl;

	std::cout << "=======  stopd all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool.idleCount() << std::endl;
#endif
	system("pause");
	return 0;
}
