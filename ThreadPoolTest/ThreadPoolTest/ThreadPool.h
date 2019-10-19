#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>

class ThreadPool {
public:
	ThreadPool(size_t);

	//任务队列，此处参数与线程的参数相同，第一个是函数名，后面是函数的参数
	//由于无法确定具体函数参数的类型，所以此处使用泛型
	template<class F, class... Args> 
	auto enqueue(F&& f, Args&&... args)	
		->std::future<typename std::result_of<F(Args...)>::type>;
	//尾置返回类型
	//future可供访问异步操作结果
	//std::result_of 可以在编译的时候推导出一个函数表达式的返回值类型（固定用法）

	~ThreadPool();

private:
	//追踪线程，这样才能join线程
	//join：在当前线程中，等待目标线程运行结束，并销毁线程
	std::vector<std::thread> workers;

	//任务队列，用于存放没有处理的任务。提供缓冲机制
	std::queue<std::function<void()> > tasks;
	//互斥锁，用于锁住任务队列
	std::mutex queue_mutex;		

	//线程同步
	//条件变量
	std::condition_variable condition;		
	bool stop;

};

// 构造函数中创建了一定数量的工作线程
inline ThreadPool::ThreadPool(size_t threads) : stop(false)
{
	for (size_t i = 0; i < threads; ++i)
		//线程存放位置，构造一个线程
		workers.emplace_back(
			//这里是个lambda表达式，一个匿名函数
			[this]
			{
				for (;;)
				{
					// task为与线程绑定的函数
					std::function<void()> task;
					{
						// 使用mutex的unique_lock加锁
						// unique_lock会在这个块（大括号）结束后自动释放锁
						std::unique_lock<std::mutex> lock(this->queue_mutex);
						// condition_variable的wait函数会等待，直到第二个参数（函数）
						// 的返回值为True为止。
						// 此处会等待直到stop为真或任务队列不为空
						this->condition.wait(lock,
							[this] { return this->stop || !this->tasks.empty();});
						// 若stop为真，且任务队列为空，则直接返回，该线程没有绑定任何任务。
						if (this->stop && this->tasks.empty())
							return;
						// 否则，将任务队列中最前面的任务与该线程绑定
						// std::move 移动资源
						task = std::move(this->tasks.front());
						this->tasks.pop();

					}

					task(); //运行函数
				}
			}
			);
}

//向线程池中加入新任务
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
	//将任务返回值类型统称为return_type
	using return_type = typename std::result_of<F(Args...)>::type;

	//make_shared创建一个智能指针，指向构建的函数,泛型部分为函数类型，括号内为函数内容
	//packaged_task允许传入一个函数，并将计算结果传递给std::future，包括函数运行时产生的异常
	//std::bind 接收一个可调用对象和一系列参数，返回一个新的可调用对象。
	//std::bind 此处用于传递函数进行构造
	auto task = std::make_shared<std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);
	
	std::future<return_type> res = task->get_future();


	{
		//保护任务队列，防止任务在进队列的同时被线程争抢。
		std::unique_lock<std::mutex> lock(queue_mutex);
		//若线程池已停止运行，不允许新任务进入任务队列
		if (stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace([task]() {(*task)();}); //进入任务进入队列
	}
	condition.notify_one();

	return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop = true;
	}
	condition.notify_all();  //通知所有wait状态的线程竞争对象的控制权，唤醒所有线程执行
	for (std::thread& worker : workers)
		worker.join(); //因为线程都开始竞争了，所以一定会执行完，join可等待线程执行完
}


