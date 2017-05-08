/**
 * This file declares a synchronous queue: one that provides an exchange point
 * between threads.
 *
 * Copyright 2017 University Corporation for Atmospheric Research
 * See file "Copying" in the top-level source-directory for terms and
 * conditions.
 *
 * SyncQueue.h
 *
 *  Created on: Apr 27, 2017
 *      Author: Steven R. Emmerson
 */

#ifndef MAIN_MISC_SYNCQUEUE_H_
#define MAIN_MISC_SYNCQUEUE_H_

#include <condition_variable>
#include <memory>
#include <mutex>

namespace hycast {

template<class T>
class SyncQueue final
{
	std::mutex              mutex;
	std::condition_variable cond;
	T                       obj;
	bool                    haveObj;

public:
	SyncQueue()
		: haveObj{false}
	{}

	SyncQueue(const SyncQueue& that) = delete;
	SyncQueue(const SyncQueue&& that) = delete;
	SyncQueue& operator =(const SyncQueue& rhs) = delete;
	SyncQueue& operator =(const SyncQueue&& rhs) = delete;

	void push(T obj)
	{
		std::unique_lock<decltype(mutex)> lock{mutex};
		while (haveObj)
			cond.wait(lock);
		this->obj = obj;
		haveObj = true;
		cond.notify_one();
	}

	T pop()
	{
		std::unique_lock<decltype(mutex)> lock{mutex};
		while (!haveObj)
			cond.wait(lock);
		haveObj = false;
		cond.notify_one();
		return obj;
	}
};

} // namespace

#endif /* MAIN_MISC_SYNCQUEUE_H_ */
