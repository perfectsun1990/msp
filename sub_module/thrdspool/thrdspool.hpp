
/************************************************************************/
/*	线程池
 *	任务类型：直接支持全局，静态，类静态,Lamada表达式 以及Opteron()仿函数
 *  使用bind()绑定对象后可实现对类成员函数的间接支持,不限参数个数类型。
 *	工作模式：多线程单任务队列(其他：单线程单任务队列,多线程多任务队列)
 *  返回结果：通过future阻塞获取（pull方式）或回调函数通知触发（push方式）
/************************************************************************/

#pragma once
#if defined (WIN32) && defined (DLL_EXPORT)
# define THRDSPOOL_API __declspec(dllexport)
#elif defined (__GNUC__) && (__GNUC__ >= 4)
# define THRDSPOOL_API __attribute__((visibility("default")))
#else
# define THRDSPOOL_API
#endif

#include <memory>
#include <functional>
#include <atomic>
#include <future>

/**
*模板函数/类不能导出到动态链接库（DLL）
*模板函数在声明的时候，其实并不存在，函数地址也就无从谈起了，而导出到动态链接库的函数都需要有地址，就是说函数模板不具备导出的基本条件。
*函数模板在调用时后，有了具体的实现，这个时候才有了地址。如果要导出，必须将参数类型列表具体化或者模板定义出现在导出类内。
*/
using Task = std::function<void(void)>;

class THRDSPOOL_API TaskQueue
{
public:
	static  std::shared_ptr<TaskQueue> create();
	virtual std::shared_ptr<TaskQueue> clone() = 0;
	virtual void push(Task&  v) = 0;
	virtual void push(Task&& v) = 0;
	virtual void peek(Task&  v) = 0;
	virtual bool try_peek(Task&  v) = 0;
	virtual void popd(Task&  v) = 0;
	virtual bool try_popd(Task&  v) = 0;
	/* Q.inner status */
	virtual uint32_t size() const = 0;
	virtual void clear() = 0;
	virtual bool empty() const = 0;
	virtual void awake(bool wake = false) = 0;
protected:
	virtual ~TaskQueue() = default;
	TaskQueue() = default;
	TaskQueue(const TaskQueue&  other) = delete;
	TaskQueue& operator=(const TaskQueue& other) = delete;
};

class THRDSPOOL_API ThrdProxy
{
public:
	static  std::shared_ptr<ThrdProxy> create(Task&  task);
	static  std::shared_ptr<ThrdProxy> create(Task&& task);
	virtual std::shared_ptr<ThrdProxy> clone() = 0;
	virtual bool state() = 0;//running?
	virtual void start(bool loop = false) = 0;
	virtual void stopd(bool sepa = false) = 0;
protected:
	virtual ~ThrdProxy() = default;
	ThrdProxy() = default;
	ThrdProxy(const ThrdProxy& other) = delete;
	ThrdProxy& operator=(const ThrdProxy& other) = delete;
};

class THRDSPOOL_API ThrdsPool
{
public:
	static  std::shared_ptr<ThrdsPool> create(int32_t size);
	virtual int32_t taskCount() = 0;
	virtual int32_t thrsCount() = 0;
	virtual int32_t idleCount() = 0;
	virtual void post(Task&  task) = 0;
	virtual void post(Task&& task) = 0;
	virtual void insertThreads(int32_t size = 8) = 0;
	virtual void removeThreads(int32_t size = 1) = 0;
	template< typename _FuncT, typename... _ArgTs>
	auto commit(_FuncT&& func, _ArgTs&&... args) ->std::future<decltype(func(args...))> {
		using retv_t = typename std::result_of<_FuncT(_ArgTs...)>::type;
		std::shared_ptr<std::packaged_task<retv_t()>> pTask =
			std::make_shared<std::packaged_task<retv_t()>>(
				std::bind(std::forward<_FuncT>(func), std::forward<_ArgTs>(args)...));
		post([pTask]() {(*pTask)(); });
		std::future<retv_t> task_future = pTask->get_future();
		return task_future;
	}
protected:
	virtual ~ThrdsPool() = default;
	ThrdsPool() = default;
	ThrdsPool(const ThrdsPool&  other) = delete;
	ThrdsPool& operator=(const ThrdsPool& other) = delete;
};

