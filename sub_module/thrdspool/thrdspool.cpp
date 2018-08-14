
#include "thrdspool.hpp"
#include <queue>
#include <list>
#include <condition_variable>
#include <thread>
#include <iostream>
#include "signals.h"

//当类内部使用了STL模板类的时候，会出现C4251警告,可以直接忽视.
//因为, 客户使用dll时，他那里也有一套STL标准模板的定义(标准的)
//或者，在每个模板类前加：template class THRDSPOOL_API(麻烦的)
//只是测试：实际中请使用接口类导出或者纯C接口导出。
#pragma warning( disable: 4251 )
#pragma warning( disable: 4477 )

template<typename _ResT, typename ... _ArgTs>
class BindFunc;
template<typename _ResT, typename ... _ArgTs>
class BindFunc<_ResT(_ArgTs...)>
{
public:
	template<typename _FuncT> BindFunc(_FuncT&& func, _ArgTs&&... args)
	{
		sig = std::make_shared<vdk::signal<_ResT(_ArgTs...)>>();
		sig->connect(&func);
		tup = std::make_shared<std::tuple<_ArgTs...>>(std::make_tuple<_ArgTs...>(std::forward<_ArgTs>(args)...));
	}
	void operator()()
	{
		sig->emit(std::forward<_ArgTs...>(std::get<0>(*tup)));
	}
private:
	std::shared_ptr<vdk::signal<_ResT(_ArgTs...)>>	sig{ nullptr };
	std::shared_ptr<std::tuple<_ArgTs...>>			tup{ nullptr };
};

class TaskQueueImpl : public TaskQueue
{
public:
	// If use right ref,bellow must be defined!
	TaskQueueImpl() {}
	TaskQueueImpl(TaskQueueImpl const& other) {
		std::lock_guard<std::mutex> locker(other.m_lock);
		m_data = other.m_data;
	}
	TaskQueueImpl& operator=(TaskQueueImpl const& other) {
		std::lock_guard<std::mutex> locker(other.m_lock);
		m_data = other.m_data;
		return *this;
	}
	TaskQueueImpl(TaskQueueImpl&& other) {// cons by moving _Right
		std::lock_guard<std::mutex> locker(other.m_lock);
		m_data = std::move(other.m_data);
	}
	TaskQueueImpl& operator=(TaskQueueImpl&& other) {
		std::lock_guard<std::mutex> locker(other.m_lock);
		m_data = std::move(other.m_data);
		return *this;
	}
	std::shared_ptr<TaskQueue> clone(){//this will copy tasks.
		return std::make_shared<TaskQueueImpl>(*this);
	}
	void push(Task&& value)  override {
		std::lock_guard<std::mutex> locker(m_lock);
		m_data.emplace(std::move(value));
		m_cond.notify_one();
	}
	void push(Task&  value)  override {
		std::lock_guard<std::mutex> locker(m_lock);
		m_data.emplace(value);
		m_cond.notify_one();
	}
	void peek(Task&  value) override {
		std::unique_lock<std::mutex> locker(m_lock);
		m_cond.wait(locker, [this] {
			return !(m_data.empty() && !m_wake); }
		);
		if (!m_data.empty()) {
			value = m_data.front();
		}
	}
	bool try_peek(Task&  value) override {
		std::lock_guard<std::mutex> locker(m_lock);
		if (m_data.empty())
			return false;
		value = m_data.front();
		return true;
	}
	void popd(Task&  value) override {
		std::unique_lock<std::mutex> locker(m_lock);
		m_cond.wait(locker, [this] {
			return !(m_data.empty() && !m_wake); }
		);
		if (!m_data.empty()) {
			value = m_data.front(); m_data.pop();
		}
	}
	bool try_popd(Task&  value) override {
		std::lock_guard<std::mutex> locker(m_lock);
		if (m_data.empty())
			return false;
		value = m_data.front();
		m_data.pop();
		return true;
	}
	uint32_t size() const override {
		std::lock_guard<std::mutex> locker(m_lock);
		return m_data.size();
	}
	bool empty() const  override {
		std::lock_guard<std::mutex> locker(m_lock);
		return m_data.empty();
	}
	void clear()  override {
		std::lock_guard<std::mutex> locker(m_lock);
		while (!m_data.empty())
			m_data.pop();
	}
	void awake(bool iswakeup = false) override {
		m_wake = iswakeup;
		m_cond.notify_all();
	}
private:
	mutable std::mutex		m_lock;
	std::queue<Task>		m_data;
	std::condition_variable m_cond;
	std::atomic<bool>		m_wake{ false };
};

std::shared_ptr<TaskQueue> TaskQueue::create(){
	return std::make_shared<TaskQueueImpl>();
}

class ThrdProxyImpl : public ThrdProxy
{
public:
	ThrdProxyImpl(std::function<void()>&  task)  {
		m_taskPtr = std::make_shared<Task>(std::forward<Task>(task));
	}
	ThrdProxyImpl(std::function<void()>&& task) {
		m_taskPtr = std::make_shared<Task>(std::forward<Task>(task));
	}
	~ThrdProxyImpl() {
		stopd();
	}
	std::shared_ptr<ThrdProxy> clone() override{
		return std::make_shared<ThrdProxyImpl>(*m_taskPtr);
	}
	bool state() override {
		return m_running;
	}
	void start(bool loop = false) override {
		if (!m_running)
		{
			m_running = true;
			m_looping = loop;
			m_workThr = std::thread([this]() {
				while (m_running) {
					(*m_taskPtr)();
					if (!m_looping)
						break;
				}
			});
		}
	}
	void stopd(bool sepa = false) override {
		if (m_running)
		{
			m_running = false;
			if (!sepa) {
				if (m_workThr.joinable())
					m_workThr.join();
			}
			else {
				m_workThr.detach();
			}
		}
	}
private:
	std::atomic<bool> 		 						m_running{ false };	 //运行状态
	std::atomic<bool> 		 						m_looping{ false };	 //是否循环
	std::thread 									m_workThr;			 //工作线程
	std::shared_ptr<std::function<void()>>			m_taskPtr{ nullptr };
};
std::shared_ptr<ThrdProxy> ThrdProxy::create(Task&  task) {
	return std::make_shared<ThrdProxyImpl>(std::forward<Task>(task));
}
std::shared_ptr<ThrdProxy> ThrdProxy::create(Task&& task) {
	return std::make_shared<ThrdProxyImpl>(std::forward<Task>(task));
}

//线程数量推荐配置：
//计算密集型：cpu cores+1；IO密集型：限制条件内越多越好。
class ThrdsPoolImpl : public ThrdsPool
{
public:
	// 定义任务函数类型.
	using Task = std::function<void(void)>;
	ThrdsPoolImpl(int32_t size = 8) {
		m_taskQue = TaskQueue::create();
		m_running = true;
		insertThreads(size);//当前是逐个追加.
	}
	~ThrdsPoolImpl() {
		m_running = false;
		removeThreads(0);	//当前是销毁所有.
		m_taskQue->clear();
	}
	// 内部运行状态获取.
	int32_t idleCount() override {
		return m_idlnums.load();
	}
	int32_t taskCount() override {
		return m_taskQue->size();
	}
	int32_t thrsCount() override {
		return m_workQue.size();
	}
	void post(Task&  task) {
		m_taskQue->push(std::forward<Task>(task));
	}
	void post(Task&& task) {
		m_taskQue->push(std::forward<Task>(task));
	}
	void insertThreads(int32_t size = 8) override {
		for (int32_t i=0; i<size; ++i) {
			auto item = ThrdProxy::create([this]()
			{
				Task task = nullptr;
				m_taskQue->popd(task);//阻塞队列,移除时需要唤醒.
				if (m_running)
				{
					m_idlnums--;
					if (nullptr != task)
						task();//执行任务
					m_idlnums++;
				}
			});
			item->start(true);
			std::lock_guard<std::mutex> locker(m_workMux);
			m_workQue.emplace_back(item);
			m_idlnums++;
		}
	}
	void removeThreads(int32_t size = 1) override {
		m_taskQue->awake(true);
		int32_t iNums = size;
		for (auto item = m_workQue.begin(); item != m_workQue.end();) {
			if (iNums-- <= 0 && 0 != size)	break;
			(*item)->stopd();
			std::lock_guard<std::mutex> locker(m_workMux);
			m_workQue.remove(*item++);
			m_idlnums--;
		}
		m_taskQue->awake(false);
	}
private:
	std::atomic<int32_t>								m_idlnums{ 0 };			//空闲数量
	std::atomic<bool> 		 							m_running{ true };		//运行状态
	std::list<std::shared_ptr<ThrdProxy>>				m_workQue;				//线程队列
	std::mutex											m_workMux;				//互斥变量
	std::shared_ptr<TaskQueue>							m_taskQue;				//任务队列
};
std::shared_ptr<ThrdsPool> ThrdsPool::create(int32_t size)
{
	return std::make_shared<ThrdsPoolImpl>(size);
}

#ifdef UNITS_TEST

#define  MAX_THREADS_NUM   16
int gfunBlock(int slp)
{
	//printf("%s execute!  thrid=%d\n",__FUNCTION__, std::this_thread::get_id());
	std::this_thread::sleep_for(std::chrono::seconds(1));
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
	int32_t StrFun(std::string str, std::function<void(std::string&&)> callback){
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
void func(void) { std::cout << "task func exec" << std::endl; }

void test_thrdspool()
{
	Task task, task_tmp;
	// 任务队列测试
	std::cout << "=======  ThrdProxy begn =========" << std::endl;
	std::shared_ptr<TaskQueue> pTaskQue = TaskQueue::create();
	pTaskQue->push(std::bind(func));
	pTaskQue->peek(task);
	task();
	std::shared_ptr<TaskQueue> pTaskQueNew = pTaskQue->clone();
	//pTaskQueNew->push(std::bind(func));
	pTaskQueNew->popd(task_tmp);//会clone原来队列中的数据。
	task_tmp();
	std::cout << "=======  TaskQueue over =========" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(1));

	// 线程代理测试
	std::cout << "=======  ThrdProxy begn =========" << std::endl;
	std::shared_ptr<ThrdProxy> pThrds = ThrdProxy::create(std::bind(func));
	pThrds->start();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	pThrds->stopd();
	std::shared_ptr<ThrdProxy> pThrdsNew = pThrds->clone();
	pThrdsNew->start();//会clone原来线程中的任务。
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	pThrdsNew->stopd();
	std::cout << "=======  ThrdProxy over =========" << std::endl;
#if 0
	// 线程池的测试
	std::shared_ptr<ThrdsPool> thr_pool = ThrdsPool::create(MAX_THREADS_NUM);
	// Test 1  使用全局函数
	std::future<int32_t> future_return1 = thr_pool->commit(StrFun, "StrFun is started!", [](std::string&& str)
	{
		std::cout << "=======>>>>>>>>>>>>>>  Test 1 callback string=" << str.c_str()
			<< " thr_id:" << std::this_thread::get_id() << std::endl;
	});
	std::cout << "=======  Test 1 post =========" << std::endl;
	std::cout << "StrFun retv=" << future_return1.get() << " thr_id:" << std::this_thread::get_id() << std::endl;
	std::cout << "=======  Test 1 over =========" << std::endl;

	std::this_thread::sleep_for(std::chrono::seconds(1));

	// Test 2  使用成员函数
	A impl;
	std::future<int32_t> future_return2 = thr_pool->commit(std::bind(&A::StrFun, &impl, "StrFun is started!", [](std::string&& str)
	{
		std::cout << "=======>>>>>>>>>>>>>>  Test 2 callback string=" << str.c_str()
			<< " thr_id:" << std::this_thread::get_id() << std::endl;
	}));
	std::cout << "=======  Test 2 post =========" << std::endl;
	std::cout << "StrFun retv=" << future_return2.get() << " thr_id:" << std::this_thread::get_id() << std::endl;
	std::cout << "=======  Test 2 over =========" << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(3));

	std::cout << "=======  start all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool->idleCount() << std::endl;
	// 调用全局函数，仿函数，Lamada表达式.
	std::future<int> ff = thr_pool->commit(gfun, 0);
	std::future<int> fg = thr_pool->commit(gfunClass(), 0);
	std::future<std::string> fh = thr_pool->commit([&]()->std::string {
		printf("%s execute!  thrid=%d\n", __FUNCTION__, std::this_thread::get_id());
		return "fh test string";
	});
	std::cout << "returm fh=" << fh.get().c_str() << std::endl;

	// 调用类成员函数
	A a;
	std::future<int> gg = thr_pool->commit(std::bind(&A::Cfun, &a, 9999));
	std::future<std::string> gh = thr_pool->commit(A::Bfun, 6666, "gh test string", 'B');
	std::future<int> gi = thr_pool->commit(std::bind(&A::Afun, 1000));
	std::future<int> gj = thr_pool->commit(std::bind(a.Afun, 400)); //IDE提示错误,但可以编译运行
																	// 获取返回值，
																	// 注意阻塞的函数，因为调用.get()获取返回值会等待线程执行完,此时调用get()会导致主程序阻塞。
	auto ret = gg.get();
	std::cout << "returm gg=" << ret << std::endl;
	std::cout << "returm gh=" << gh.get().c_str() << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	// 测试内部状态.
	std::cout << "\n=======  beg post all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool->idleCount() << std::endl;
	std::vector< std::future<int> > results;
	for (int i = 0; i < MAX_THREADS_NUM + 8; i++)
	{
		results.emplace_back(std::move(thr_pool->commit(gfunBlock, i)));
		std::this_thread::sleep_for(std::chrono::milliseconds(10));//防止任务还没有起来。
		int idle = thr_pool->idleCount();
		std::cout << "# thrsCount=" << thr_pool->thrsCount() << " idleCount=" << idle;
		if (idle == 0) std::cout << "-->Task link up: No idle threads!";
		std::cout << std::endl;
	}
	std::cout << " =======  end post all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool->idleCount() << std::endl;
	for (auto && result : results) std::cout << result.get() << ' ';
	std::cout << std::endl;

	std::cout << "=======  stopd all ========= " << std::this_thread::get_id() << " idlsize=" << thr_pool->idleCount() << std::endl;
#endif
}

int main()
{
	test_thrdspool();
#if 0
	std::shared_ptr<ThrdsPool> thr_pool = ThrdsPool::create(MAX_THREADS_NUM);
	std::thread([thr_pool]() {
		for (;;) {
			thr_pool->insertThreads(1);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::cout << "insertThreads" << thr_pool->idleCount() << std::endl;
		}
	}).detach();
	std::thread([thr_pool]() {
		for (;;) {
			thr_pool->removeThreads(1);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::cout << "removeThreads" << thr_pool->idleCount() << std::endl;
		}
	}).detach();
#endif
	system("pause");
	return 0;
}
#endif


