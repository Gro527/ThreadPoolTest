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

	//������У��˴��������̵߳Ĳ�����ͬ����һ���Ǻ������������Ǻ����Ĳ���
	//�����޷�ȷ�����庯�����������ͣ����Դ˴�ʹ�÷���
	template<class F, class... Args> 
	auto enqueue(F&& f, Args&&... args)	
		->std::future<typename std::result_of<F(Args...)>::type>;
	//β�÷�������
	//future�ɹ������첽�������
	//std::result_of �����ڱ����ʱ���Ƶ���һ���������ʽ�ķ���ֵ���ͣ��̶��÷���

	~ThreadPool();

private:
	//׷���̣߳���������join�߳�
	//join���ڵ�ǰ�߳��У��ȴ�Ŀ���߳����н������������߳�
	std::vector<std::thread> workers;

	//������У����ڴ��û�д���������ṩ�������
	std::queue<std::function<void()> > tasks;
	//��������������ס�������
	std::mutex queue_mutex;		

	//�߳�ͬ��
	//��������
	std::condition_variable condition;		
	bool stop;

};

// ���캯���д�����һ�������Ĺ����߳�
inline ThreadPool::ThreadPool(size_t threads) : stop(false)
{
	for (size_t i = 0; i < threads; ++i)
		//�̴߳��λ�ã�����һ���߳�
		workers.emplace_back(
			//�����Ǹ�lambda���ʽ��һ����������
			[this]
			{
				for (;;)
				{
					// taskΪ���̰߳󶨵ĺ���
					std::function<void()> task;
					{
						// ʹ��mutex��unique_lock����
						// unique_lock��������飨�����ţ��������Զ��ͷ���
						std::unique_lock<std::mutex> lock(this->queue_mutex);
						// condition_variable��wait������ȴ���ֱ���ڶ���������������
						// �ķ���ֵΪTrueΪֹ��
						// �˴���ȴ�ֱ��stopΪ���������в�Ϊ��
						this->condition.wait(lock,
							[this] { return this->stop || !this->tasks.empty();});
						// ��stopΪ�棬���������Ϊ�գ���ֱ�ӷ��أ����߳�û�а��κ�����
						if (this->stop && this->tasks.empty())
							return;
						// ���򣬽������������ǰ�����������̰߳�
						// std::move �ƶ���Դ
						task = std::move(this->tasks.front());
						this->tasks.pop();

					}

					task(); //���к���
				}
			}
			);
}

//���̳߳��м���������
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
	//�����񷵻�ֵ����ͳ��Ϊreturn_type
	using return_type = typename std::result_of<F(Args...)>::type;

	//make_shared����һ������ָ�룬ָ�򹹽��ĺ���,���Ͳ���Ϊ�������ͣ�������Ϊ��������
	//packaged_task������һ���������������������ݸ�std::future��������������ʱ�������쳣
	//std::bind ����һ���ɵ��ö����һϵ�в���������һ���µĿɵ��ö���
	//std::bind �˴����ڴ��ݺ������й���
	auto task = std::make_shared<std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);
	
	std::future<return_type> res = task->get_future();


	{
		//����������У���ֹ�����ڽ����е�ͬʱ���߳�������
		std::unique_lock<std::mutex> lock(queue_mutex);
		//���̳߳���ֹͣ���У�����������������������
		if (stop)
			throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace([task]() {(*task)();}); //��������������
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
	condition.notify_all();  //֪ͨ����wait״̬���߳̾�������Ŀ���Ȩ�����������߳�ִ��
	for (std::thread& worker : workers)
		worker.join(); //��Ϊ�̶߳���ʼ�����ˣ�����һ����ִ���꣬join�ɵȴ��߳�ִ����
}


