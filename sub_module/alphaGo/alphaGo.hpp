
/************************************************************************/
/*	线程池,可以提交变参函数或拉姆达表达式的匿名函数执行,可以获取执行返回值
*	不直接支持类成员函数, 支持类静态成员函数或全局函数,Opteron()函数等                                                                   */
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

//线程池最大容量,应尽量设小一点
#define  THREADPOOL_MAX_NUM		16
#define  THREADPOOL_AUTO_GROW

class thrpool
{
public:
	using Task = std::function<void(void)>;	//任务仿函数类型
	thrpool(int32_t size = 4) {
		m_running = true;
		appendThrToworkQue(size);
	}
	~thrpool() {
		m_running = false;
		deleteThrFrworkQue(true);//当前是销毁所有.
	}
	int32_t idleCount() {
		return m_idlnums;
	}
	int32_t thrsCount() {
		return m_workQue.size();
	}
	// 提交一个任务
	// 调用.get()获取返回值会等待任务执行完,获取返回值
	template< typename Func, typename... Args>
	auto commit(Func&& func, Args&&... args) ->std::future<decltype(func(args...))>
	{
		using return_t = decltype(func(args...));
// 		std::cout << "return_t() is same with result_of ?: " <<
// 			std::is_same<return_t(), typename std::result_of<F(Args...)>::type()>::value << std::endl;
		if (!m_running) //Error! 
			return std::promise<return_t>().get_future();
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
	void deleteThrFrworkQue(bool detach)
	{
		m_taskCon.notify_all();
		for (auto &item : m_workQue)
		{
			if (detach) {
				item.detach();
			}
			else {
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