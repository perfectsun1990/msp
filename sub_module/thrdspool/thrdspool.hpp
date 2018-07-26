
/************************************************************************/
/*	线程池
 *	任务类型：直接支持全局，静态，类静态,Lamada表达式 以及Opteron()仿函数
 *  使用bind()绑定对象后可实现对类成员函数的间接支持,不限参数个数类型。
 *	工作模式：多线程单任务队列(其他：单线程单任务队列,多线程多任务队列)
 *  返回结果：通过future阻塞获取（pull方式）或回调函数通知触发（push方式）
/************************************************************************/

#pragma once

#include <vector>
#include <queue>
#include <stack>
#include <list>
#include <atomic>
#include <future>
#include <condition_variable>
#include <thread>
#include <functional>
#include <stdexcept>
#include <iostream>
#include "signals.h"

template<typename T> class SafeStack
{
public:
	// If use right ref,bellow must be defined!
	SafeStack() {}
	SafeStack(SafeStack const& other) {
		std::lock_guard<std::mutex> locker(other.m_lock);
		m_data(other.m_data);
	}
	SafeStack& operator=(SafeStack const& other) {
		std::lock_guard<std::mutex> locker(other.m_lock);
		m_data = other.m_data;
		return *this;
	}
	SafeStack(SafeStack&& other) {// cons by moving _Right
		m_data(std::move(other));
	}
	SafeStack& operator=(SafeStack&& other) {
		m_data = std::move(other);
		return *this;
	}
	uint32_t size() const {
		std::lock_guard<std::mutex> locker(m_lock);
		return m_data.size();
	}
	void push(T&& value) {
		std::lock_guard<std::mutex> locker(m_lock);
		m_data.emplace(std::move(value));
		m_cond.notify_one();
	}
	void push(T&  value) {
		std::lock_guard<std::mutex> locker(m_lock);
		m_data.emplace(value);
		m_cond.notify_one();
	}
	void peek(T&  value) {
		std::unique_lock<std::mutex> locker(m_lock);
		m_cond.wait(locker, [this] {
			return !(m_data.empty() && !m_wake); }
		);
		if (!m_data.empty()) {
			value = m_data.top();
		}
	}
	void popd(T&  value) {
		std::unique_lock<std::mutex> locker(m_lock);
		m_cond.wait(locker, [this] {
			return !(m_data.empty() && !m_wake); }
		);
		if (!m_data.empty()) {
			value = m_data.top(); m_data.pop();
		}
	}
	bool try_peek(T&  value) {
		std::lock_guard<std::mutex> locker(m_lock);
		if (m_data.empty())
			return false;
		value = m_data.top();
		return true;
	}
	bool try_popd(T&  value) {
		std::lock_guard<std::mutex> locker(m_lock);
		if (m_data.empty())
			return false;
		value = m_data.top();
		m_data.pop();
		return true;
	}
	bool empty() const {
		std::lock_guard<std::mutex> locker(m_lock);
		return m_data.empty();
	}
	void clear() {
		std::lock_guard<std::mutex> locker(m_lock);
		while (!m_data.empty())
			m_data.pop();
	}
	void awake(bool iswakeup = false) {
		m_wake = iswakeup;
		m_cond.notify_all();
	}
private:
	mutable std::mutex		m_lock;
	std::stack<T>			m_data;
	std::condition_variable m_cond;
	std::atomic<bool>		m_wake{ false };
};

template<typename T> class SafeQueue
{
public:
	// If use right ref,bellow must be defined!
	SafeQueue() {}
	SafeQueue(SafeQueue const& other) {
		std::lock_guard<std::mutex> locker(other.m_lock);
		m_data(other.m_data);
	}
	SafeQueue& operator=(SafeQueue const& other) {
		std::lock_guard<std::mutex> locker(other.m_lock);
		m_data = other.m_data;
		return *this;
	}
	SafeQueue(SafeQueue&& other) {// cons by moving _Right
		m_data(std::move(other));
	}
	SafeQueue& operator=(SafeQueue&& other) {
		m_data =std::move(other);
		return *this;
	}
	uint32_t size() const{
		std::lock_guard<std::mutex> locker(m_lock);
		return m_data.size();
	}
	void push(T&& value) {
		std::lock_guard<std::mutex> locker(m_lock);
		m_data.emplace(std::move(value));
		m_cond.notify_one();
	}
	void push(T&  value) {
		std::lock_guard<std::mutex> locker(m_lock);
		m_data.emplace(value);
		m_cond.notify_one();
	}
	void peek(T&  value) {
		std::unique_lock<std::mutex> locker(m_lock);
		m_cond.wait(locker, [this] {
			return !(m_data.empty() && !m_wake); }
		);
		if (!m_data.empty()) {
			value = m_data.front();
		}
	}
	void popd(T&  value) {
		std::unique_lock<std::mutex> locker(m_lock);
		m_cond.wait(locker, [this] {
			return !(m_data.empty() && !m_wake); }
		);
		if (!m_data.empty()) {
			value = m_data.front(); m_data.pop();
		}
	}
	bool try_peek(T&  value) {
		std::lock_guard<std::mutex> locker(m_lock);
		if (m_data.empty())
			return false;
		value = m_data.front();
		return true;
	}
	bool try_popd(T&  value) {
		std::lock_guard<std::mutex> locker(m_lock);
		if (m_data.empty())
			return false;
		value = m_data.front();
		m_data.pop();
		return true;
	}
	bool empty() const {
		std::lock_guard<std::mutex> locker(m_lock);
		return m_data.empty();
	}
	void clear() {
		std::lock_guard<std::mutex> locker(m_lock);
		while (!m_data.empty())
			m_data.pop();
	}
	void awake(bool iswakeup = false) {
		m_wake = iswakeup;
		m_cond.notify_all();
	}
private:
	mutable std::mutex		m_lock;
	std::queue<T>			m_data;
	std::condition_variable m_cond;
	std::atomic<bool>		m_wake{ false };
};

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

class TaskQueue final
	: public SafeQueue<std::function<void(void)>>
{
public:
	// 提交一个异步任务.
	// 调用.get()阻塞获取函数返回值->pull模式.
	// 或者注册callback()等待被触发->push模式.
	template< typename _FuncT, typename... _ArgTs>
	auto post(_FuncT&& func, _ArgTs&&... args) ->std::future<decltype(func(args...))>
	{
		using retv_t = typename std::result_of<_FuncT(_ArgTs...)>::type;
		std::shared_ptr<std::packaged_task<retv_t()>> pTask =
			std::make_shared<std::packaged_task<retv_t()>>(
				std::bind(std::forward<_FuncT>(func), std::forward<_ArgTs>(args)...));
		push([pTask]() {(*pTask)(); });
		std::future<retv_t> task_future = pTask->get_future();
		return task_future;
	}
};

class ThrWorker
{
public:
	ThrWorker(std::function<void()> task){
		m_taskPtr = std::make_shared<std::function<void()>>(task);
	}
	~ThrWorker() {
		stopd();
	}
	bool state(){
		return m_running;
	}
	void start( bool loop = false ) {
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
	void stopd( bool sepa = false) {
		if (m_running)
		{
			m_running = false;
			if (!sepa) {
				if (m_workThr.joinable())
					m_workThr.join();
			}else {
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

//线程数量推荐配置：
//计算密集型：cpu cores+1；IO密集型：限制条件内越多越好。
//#define  THREADS_AUTOGROW
//#define  MAX_THREADS_NUMS		16
class ThrPool final
{
public:
	// 定义任务函数类型.
	using Task = std::function<void(void)>;	
	ThrPool(int32_t size = 8) {
		m_running = true;
		insertThreads(size);//当前是逐个追加.
	}
	~ThrPool() {
		m_running = false;
		removeThreads();//当前是销毁所有.
	}
	// 内部运行状态获取.
	int32_t idleCount() {
		return m_idlnums;
	}
	int32_t taskCount() {
		return m_taskQue.size();
	}
	int32_t thrsCount() {
		return m_workQue.size();
	}
	// 提交线程异步任务.
	template< typename _FuncT, typename... _ArgTs>
	auto post(_FuncT&& func, _ArgTs&&... args) ->std::future<decltype(func(args...))>  {
		return m_taskQue.post(std::forward<_FuncT>(func), std::forward<_ArgTs>(args)...);
	}
private:
	void insertThreads(int32_t size = 8)
	{
		for (int32_t i=0; i<size; ++i) {
			m_workQue.emplace_back(std::make_shared<ThrWorker>([this]()
			{
				Task task = nullptr;
				m_taskQue.popd(task);//阻塞队列,移除时需要唤醒.
				if (m_running)
				{
					m_idlnums--;
					if (nullptr != task)
						task();//执行任务
					m_idlnums++;
				}
			}));
			m_idlnums++;
		}
		for (auto &item : m_workQue) {
			item->start(true);
		}
	}
	void removeThreads(int32_t size = 0)
	{
		m_taskQue.awake(true);
		for (int32_t i = 0; i<size; ++i){
			for (auto &item : m_workQue)
				item->stopd();
		}
		if( 0 ==  size ) {
			for (auto iter = m_workQue.begin(); iter != m_workQue.end();) {
				(*iter)->stopd();
				m_workQue.remove(*iter++);
			}
			m_taskQue.clear();
		}		
		m_taskQue.awake(false);
	}
	
private:
	std::atomic<int32_t>							m_idlnums{ 0 };			//空闲数量
	std::atomic<bool> 		 						m_running{ true };		//运行状态
	std::list<std::shared_ptr<ThrWorker>>			m_workQue;				//线程队列
	//std::list<std::shared_ptr<ThrWorker>>			m_workQue1;				//线程队列
	TaskQueue 										m_taskQue;				//任务队列
};


