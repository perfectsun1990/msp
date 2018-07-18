
#include "thrdspool.hpp"
#include <iostream>


int gfunBlock(int slp)
{
	//printf("%s execute!  thrid=%d\n",__FUNCTION__, std::this_thread::get_id());
	std::this_thread::sleep_for(std::chrono::seconds(5));
	return  slp;
}

int gfun(int slp)
{
	printf("%s execute!  thrid=%d\n", __FUNCTION__, std::this_thread::get_id());
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
		return n;
	}
};

int main()
{
#define  MAX_THREADS_NUM   16

	// 创建线程池，指定最大线程数.
	thrpool thr_pool{ MAX_THREADS_NUM };
	
	std::cout << " =======  begin all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool.idleCount() << std::endl;

	// 调用全局函数，仿函数，Lamada表达式.
	std::future<int> ff = thr_pool.commit(gfun, 0);
	std::future<int> fg = thr_pool.commit(gfunClass(), 0);
	std::future<std::string> fh = thr_pool.commit([&]()->std::string 
	{
		printf("%s execute!  thrid=%d\n", __FUNCTION__, std::this_thread::get_id());
		return "fh test string";
	});

	// 调用类成员函数
	A a;
	std::future<int> gg = thr_pool.commit(std::bind(&A::Cfun, &a, 9999)); 	
	std::future<std::string> gh = thr_pool.commit(A::Bfun, 6666, "gh test string", 'B');
	std::future<int> gi = thr_pool.commit(std::bind(&A::Afun, 1000)); 
	std::future<int> gj = thr_pool.commit(std::bind(a.Afun, 400)); //IDE提示错误,但可以编译运行

	// 获取返回值，
	// 注意阻塞的函数，因为调用.get()获取返回值会等待线程执行完,此时调用get()会导致主程序阻塞。
	auto ret = gg.get();
	std::cout << "returm gg=" << ret << std::endl;
	std::cout << "returm gh=" << gh.get().c_str() << std::endl;
	std::cout << "returm fh=" << fh.get().c_str() << std::endl;

	thr_pool.commit(gfun, 8888).get();    //调用.get()获取返回值会等待线程执行完,

	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	std::cout << "\n=======  beg commit all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool.idleCount() << std::endl;
	std::vector< std::future<int> > results;
	for (int i = 0; i < MAX_THREADS_NUM+8; i++) 
	{
		results.emplace_back(std::move(thr_pool.commit(gfunBlock, i)));
		int idle = thr_pool.idleCount();
		std::cout << "# thrsCount=" << thr_pool.thrsCount() << " idleCount=" << idle;
		if (idle == 0) std::cout << "-->Task link up: No idle threads!";
		std::cout << std::endl;
	}
	std::cout << " =======  end commit all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool.idleCount() << std::endl;
#if 1// 获取返回值 将会阻塞当前线程直到指定等待的线程结束。.
	for (auto && result : results)
	std::cout << result.get() << ' ';
	std::cout << std::endl;
#endif
	std::cout << " =======  finish all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool.idleCount() << std::endl;
	system("pause");
	return 0;
}
