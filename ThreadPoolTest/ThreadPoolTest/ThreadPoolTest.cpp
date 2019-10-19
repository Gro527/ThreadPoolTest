// ThreadPoolTest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <chrono>
#include <vector>
#include "ThreadPool.h"

std::mutex mutex_a;
int apples = 66;

int main()
{
	ThreadPool pool(4);
	std::vector<std::future<int>> results;

	for (int i = 0; i < 8; i++)
	{
		results.emplace_back(
			pool.enqueue([i] {
				std::unique_lock<std::mutex> lock(mutex_a);
				std::cout << "apples:" << --apples << std::endl;
				std::cout << "hello " << i << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
				std::cout << "world " << i << std::endl;
				return i * i;
				})
		);
	}

	for (auto&& results : results)
		std::cout << results.get() << ' ';
	std::cout << std::endl;
	std::cout << apples << std::endl;

	return EXIT_SUCCESS;
}
