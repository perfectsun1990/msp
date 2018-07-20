
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
#include <atomic>
#include <future>
#include <condition_variable>
#include <thread>
#include <functional>
#include <stdexcept>

#include "signals.h"

//线程池最大容量,应尽量设小一点
#define  THREADPOOL_MAX_NUM		16
#define  THREADPOOL_AUTO_GROW

template<typename _ResT, typename ... _ArgTs>
class CallBack;
template<typename _ResT, typename ... _ArgTs>
class CallBack<_ResT(_ArgTs...)>
{
public:
	template<typename _FuncT> CallBack(_FuncT&& func, _ArgTs&&... args)
	{
		sig = std::make_shared<vdk::signal<_ResT(_ArgTs...)>>();
		sig->connect(&func);
		tup =  std::make_shared<std::tuple<_ArgTs...>>(std::make_tuple<_ArgTs...>(std::forward<_ArgTs>(args)...));
	}
	void operator()() 
	{
		sig->emit(std::forward<_ArgTs...>(std::get<0>(*tp)));
	}
private:
	std::shared_ptr<vdk::signal<_ResT(_ArgTs...)>>	sig{ nullptr };
	std::shared_ptr<std::tuple<_ArgTs...>>			tup{ nullptr };
};

class ThrPool
{
public:
	// 定义任务函数类型.
	using Task = std::function<void(void)>;	
	ThrPool(int32_t size = 4) {
		m_running = true;
		appendThrToworkQue(size);//当前是逐个追加.
	}
	~ThrPool() {
		m_running = false;
		deleteThrFrworkQue(true);//当前是销毁所有.
	}

	// 内部运行状态获取.
	int32_t idleCount() {
		return m_idlnums;
	}
	int32_t taskCount() {
		std::lock_guard<std::mutex> locker(m_taskMux);
		return m_taskQue.size();
	}
	int32_t thrsCount() {
		return m_workQue.size();
	}

	// 提交一个异步任务.
	// 调用.get()阻塞获取函数返回值->pull模式.
	// 或者注册callback()等待被触发->push模式.
	template< typename Func, typename... Args>
	auto push(Func&& func, Args&&... args) ->std::future<decltype(func(args...))>
	{
		using return_t = decltype(func(args...));
		if (!m_running) 
			return std::promise<return_t>().get_future();
		// 		std::cout << typeid(func).name() << "return_t() is same with result_of ?: " << 
		// 			std::is_same<return_t(), typename std::result_of<F(Args...)>::type()>::value << std::endl;
		std::shared_ptr<std::packaged_task<return_t()>> pTask =
			std::make_shared<std::packaged_task<return_t()>>(
				std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<return_t> task_future = pTask->get_future();
		{
			std::lock_guard<std::mutex> locker(m_taskMux);
			m_taskQue.emplace([pTask]() {
				(*pTask)();
			});
		}
		// 唤醒一个线程执行
		m_taskCon.notify_one();
		return task_future;
	}

	// 提交一个异步任务.
	// 调用.get()阻塞获取函数返回值->pull模式.
	// 或者注册callback()等待被触发->push模式.
	void post(std::function<void(void)> func, std::function<void(void)> callback = nullptr)
	{
		{
			std::lock_guard<std::mutex> locker(m_taskMux);
			m_taskQue.emplace([func, callback]() {
				func();
				std::thread([callback]() {
					if (callback) callback();
					printf("----->>>>>>>>>>>>finish ok!\n");
				}).detach();	
				printf("##################finish!\n");
			});
		}
		// 唤醒一个线程执行
		m_taskCon.notify_one();
		return;
	}

private:
	void appendThrToworkQue(int32_t size)
	{
		for (int32_t i = 0; i < size; ++i)
		{
			m_workQue.emplace_back(std::thread([this]()
			{
				while (m_running)
				{
					Task task;
					{
						std::unique_lock<std::mutex> locker(m_taskMux);
						m_taskCon.wait(locker, [&](){
							return (!m_running || !m_taskQue.empty());
						});
						if (!m_taskQue.empty())
						{// 按先进先出从队列取一个 task
							task = std::move(m_taskQue.front());
							m_taskQue.pop();
						}
					}
					m_idlnums--;
					task();//执行任务
					m_idlnums++;
				}
			}));
			m_idlnums++;
		}
	}
	void deleteThrFrworkQue(bool join)
	{
		m_taskCon.notify_all();
		for (auto &item : m_workQue)
		{
			if (!join) {
				item.detach();
			}else {
				if (item.joinable())
					item.join();
			}
		}
	}
private:
	std::atomic<int32_t>		m_idlnums{ 0 };		 //空闲数量
	std::atomic<bool> 		 	m_running{ true };	 //运行状态
	std::vector<std::thread> 	m_workQue;			 //线程队列
	std::queue<Task> 			m_taskQue;			 //任务队列
	std::mutex 					m_taskMux;			 //互斥原语
	std::condition_variable		m_taskCon;			 //同步原语
};

class TaskQue
{
public:
	template< typename Func, typename... Args>
	auto commit(Func&& func, Args&&... args) ->std::future<decltype(func(args...))>{
		return m_thrpool->commit(std::forward<Func>(func), std::forward<Args>(args)...);
	}
	int32_t taskCount() {
		return m_thrpool->taskCount();
	}
private:
	std::shared_ptr<ThrPool> m_thrpool{ std::make_shared<ThrPool>(1) };
};
