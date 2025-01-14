// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/**
 * @file
 * Pmem-resident condition variable.
 */

#ifndef LIBPMEMOBJ_CPP_CONDVARIABLE_HPP
#define LIBPMEMOBJ_CPP_CONDVARIABLE_HPP

#include <chrono>
#include <condition_variable>

#include <libpmemobj++/detail/conversions.hpp>
#include <libpmemobj++/mutex.hpp>
#include <libpmemobj/thread.h>

namespace pmem
{

namespace obj
{

/**
 * Persistent memory resident condition variable.
 *
 * This class is an implementation of a PMEM-resident condition
 * variable which mimics in behavior the C++11 std::condition_variable. The
 * typical usage example would be:
 * @snippet mutex/mutex.cpp cond_var_example
 * @ingroup synchronization
 */
class condition_variable {
	typedef std::chrono::system_clock clock_type;

public:
	/** The handle typedef to the underlying basic type. */
	typedef PMEMcond *native_handle_type;

	/**
	 * Default constructor.
	 *
	 * @throw lock_error when the condition_variable is not from persistent
	 * memory.
	 */
	condition_variable()
	{
		PMEMobjpool *pop;
		if ((pop = pmemobj_pool_by_ptr(&pcond)) == nullptr)
			throw pmem::lock_error(
				1, std::generic_category(),
				"Persistent condition variable not from persistent memory.");

		pmemobj_cond_zero(pop, &pcond);
	}

	/**
	 * Defaulted destructor.
	 */
	~condition_variable() = default;

	/**
	 * Notify and unblock one thread waiting on `*this` condition.
	 *
	 * Does nothing when no threads are waiting. It is unspecified
	 * which thread is selected for unblocking.
	 *
	 * @throw lock_error when the signal fails on the #pcond.
	 */
	void
	notify_one()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		if (int ret = pmemobj_cond_signal(pop, &this->pcond))
			throw detail::exception_with_errormsg<lock_error>(
				ret, std::system_category(),
				"Error notifying one on a condition variable.");
	}

	/**
	 * Notify and unblock all threads waiting on `*this` condition.
	 *
	 * Does nothing when no threads are waiting.
	 */
	void
	notify_all()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		if (int ret = pmemobj_cond_broadcast(pop, &this->pcond))
			throw detail::exception_with_errormsg<lock_error>(
				ret, std::system_category(),
				"Error notifying all on a condition variable.");
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified or it is woken up by some other measure.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a PMEM-resident obj::mutex.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	void
	wait(mutex &lock)
	{
		this->wait_impl(lock);
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified or it is woken up by some other measure.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a Lock object which meets the
	 * BasicLockableConcept. Needs to be based on a PMEM-resident
	 * obj::mutex.
	 *
	 * @throw lock_error when unlocking the lock or waiting
	 * on #pcond fails.
	 */
	template <typename Lock>
	void
	wait(Lock &lock)
	{
		this->wait_impl(*lock.mutex());
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 * This version is immune to spurious wake ups due to the
	 * provided predicate.
	 *
	 * @param[in,out] lock a PMEM-resident obj::mutex.
	 * @param[in] pred predicate which returns `false` if waiting is
	 * to be continued.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Predicate>
	void
	wait(mutex &lock, Predicate pred)
	{
		this->wait_impl(lock, std::move(pred));
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 * This version is immune to spurious wake ups due to the
	 * provided predicate.
	 *
	 * @param[in,out] lock a Lock object which meets the
	 * BasicLockableConcept. Needs to be based on a PMEM-resident
	 * obj::mutex.
	 * @param[in] pred predicate which returns `false` if waiting is
	 * to be continued.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Lock, typename Predicate>
	void
	wait(Lock &lock, Predicate pred)
	{
		this->wait_impl(*lock.mutex(), std::move(pred));
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified, a specific time is reached or it is woken up by
	 * some other measure.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a PMEM-resident obj::mutex.
	 * @param[in] timeout a specific point in time, which when
	 * reached unblocks the thread.
	 *
	 * @return std::cv_status::timeout on timeout,
	 * std::cv_status::no_timeout otherwise.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Clock, typename Duration>
	std::cv_status
	wait_until(mutex &lock,
		   const std::chrono::time_point<Clock, Duration> &timeout)
	{
		return this->wait_until_impl(lock, timeout);
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified, a specific time is reached or it is woken up by
	 * some other measure.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a Lock object which meets the
	 * BasicLockableConcept. Needs to be based on a PMEM-resident
	 * obj::mutex.
	 * @param[in] timeout a specific point in time, which when
	 * reached unblocks the thread.
	 *
	 * @return std::cv_status::timeout on timeout,
	 * std::cv_status::no_timeout otherwise.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Lock, typename Clock, typename Duration>
	std::cv_status
	wait_until(Lock &lock,
		   const std::chrono::time_point<Clock, Duration> &timeout)
	{
		return this->wait_until_impl(*lock.mutex(), timeout);
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified or a specific time is reached.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a PMEM-resident obj::mutex.
	 * @param[in] timeout a specific point in time, which when
	 * reached unblocks the thread.
	 * @param[in] pred predicate which returns `false` if waiting is
	 * to be continued.
	 *
	 * @return `false` if pred evaluates to `false` after timeout
	 * expired, otherwise `true`.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Clock, typename Duration, typename Predicate>
	bool
	wait_until(mutex &lock,
		   const std::chrono::time_point<Clock, Duration> &timeout,
		   Predicate pred)
	{
		return this->wait_until_impl(lock, timeout, std::move(pred));
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified or a specific time is reached.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a Lock object which meets the
	 * BasicLockableConcept. Needs to be based on a PMEM-resident
	 * obj::mutex.
	 * @param[in] timeout a specific point in time, which when
	 * reached unblocks the thread.
	 * @param[in] pred predicate which returns `false` if waiting is
	 * to be continued.
	 *
	 * @return `false` if pred evaluates to `false` after timeout
	 * expired, otherwise `true`.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Lock, typename Clock, typename Duration,
		  typename Predicate>
	bool
	wait_until(Lock &lock,
		   const std::chrono::time_point<Clock, Duration> &timeout,
		   Predicate pred)
	{
		return this->wait_until_impl(*lock.mutex(), timeout,
					     std::move(pred));
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified, the specified amount of time passes or it is
	 * woken up by some other measure.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a Lock object which meets the
	 * BasicLockableConcept. Needs to be based on a PMEM-resident
	 * obj::mutex.
	 * @param[in] rel_time a specific duration, which when
	 * expired unblocks the thread.
	 *
	 * @return std::cv_status::timeout on timeout,
	 * std::cv_status::no_timeout otherwise.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Lock, typename Rep, typename Period>
	std::cv_status
	wait_for(Lock &lock, const std::chrono::duration<Rep, Period> &rel_time)
	{
		return this->wait_until_impl(*lock.mutex(),
					     clock_type::now() + rel_time);
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified or the specified amount of time passes.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a Lock object which meets the
	 * BasicLockableConcept. Needs to be based on a PMEM-resident
	 * obj::mutex.
	 * @param[in] rel_time a specific duration, which when
	 * expired unblocks the thread.
	 * @param[in] pred predicate which returns `false` if waiting is
	 * to be continued.
	 *
	 * @return `false` if pred evaluates to `false` after timeout
	 * expired, otherwise `true`.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Lock, typename Rep, typename Period,
		  typename Predicate>
	bool
	wait_for(Lock &lock, const std::chrono::duration<Rep, Period> &rel_time,
		 Predicate pred)
	{
		return this->wait_until_impl(*lock.mutex(),
					     clock_type::now() + rel_time,
					     std::move(pred));
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified, the specified amount of time passes or it is
	 * woken up by some other measure.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a PMEM-resident obj::mutex.
	 * @param[in] rel_time a specific duration, which when
	 * expired unblocks the thread.
	 *
	 * @return std::cv_status::timeout on timeout,
	 * std::cv_status::no_timeout otherwise.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Rep, typename Period>
	std::cv_status
	wait_for(mutex &lock,
		 const std::chrono::duration<Rep, Period> &rel_time)
	{
		return this->wait_until_impl(lock,
					     clock_type::now() + rel_time);
	}

	/**
	 * Makes the current thread block until the condition variable
	 * is notified or the specified amount of time passes.
	 *
	 * This releases the lock, blocks the current thread and adds
	 * it to the list of threads waiting on `*this` condition
	 * variable. The lock needs to be acquired and owned by the
	 * calling thread. The lock is automatically reacquired after
	 * the call to wait.
	 *
	 * @param[in,out] lock a PMEM-resident obj::mutex.
	 * @param[in] rel_time a specific duration, which when
	 * expired unblocks the thread.
	 * @param[in] pred predicate which returns `false` if waiting is
	 * to be continued.
	 *
	 * @return `false` if pred evaluates to `false` after timeout
	 * expired, otherwise `true`.
	 *
	 * @throw lock_error when unlocking the lock or waiting on
	 * #pcond fails.
	 */
	template <typename Rep, typename Period, typename Predicate>
	bool
	wait_for(mutex &lock,
		 const std::chrono::duration<Rep, Period> &rel_time,
		 Predicate pred)
	{
		return this->wait_until_impl(lock, clock_type::now() + rel_time,
					     std::move(pred));
	}

	/**
	 * Access a native handle to this condition variable.
	 *
	 * @return a pointer to PMEMcond.
	 */
	native_handle_type
	native_handle() noexcept
	{
		return &this->pcond;
	}

	/**
	 * Deleted assignment operator.
	 */
	condition_variable &operator=(const condition_variable &) = delete;

	/**
	 * Deleted copy constructor.
	 */
	condition_variable(const condition_variable &) = delete;

private:
	/**
	 * Internal implementation of the wait call.
	 */
	void
	wait_impl(mutex &lock)
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		if (int ret = pmemobj_cond_wait(pop, &this->pcond,
						lock.native_handle()))
			throw detail::exception_with_errormsg<lock_error>(
				ret, std::system_category(),
				"Error waiting on a condition variable.");
	}

	/**
	 * Internal implementation of the wait call.
	 */
	template <typename Predicate>
	void
	wait_impl(mutex &lock, Predicate pred)
	{
		while (!pred())
			this->wait(lock);
	}

	/**
	 * Internal implementation of the wait_until call.
	 */
	template <typename Clock, typename Duration>
	std::cv_status
	wait_until_impl(
		mutex &lock,
		const std::chrono::time_point<Clock, Duration> &abs_timeout)
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);

		/* convert to my clock */
		const typename Clock::time_point their_now = Clock::now();
		const clock_type::time_point my_now = clock_type::now();
		const auto delta = abs_timeout - their_now;
		const auto my_rel = my_now + delta;

		struct timespec ts = detail::timepoint_to_timespec(my_rel);

		auto ret = pmemobj_cond_timedwait(pop, &this->pcond,
						  lock.native_handle(), &ts);

		if (ret == 0)
			return std::cv_status::no_timeout;
		else if (ret == ETIMEDOUT)
			return std::cv_status::timeout;
		else
			throw detail::exception_with_errormsg<lock_error>(
				ret, std::system_category(),
				"Error waiting on a condition variable.");
	}

	/**
	 * Internal implementation of the wait_until call.
	 */
	template <typename Clock, typename Duration, typename Predicate>
	bool
	wait_until_impl(
		mutex &lock,
		const std::chrono::time_point<Clock, Duration> &abs_timeout,
		Predicate pred)
	{
		while (!pred())
			if (this->wait_until_impl(lock, abs_timeout) ==
			    std::cv_status::timeout)
				return pred();
		return true;
	}

	/** A POSIX style PMEM-resident condition variable.*/
	PMEMcond pcond;
};

} /* namespace obj */

} /* namespace pmem */

#endif /* LIBPMEMOBJ_CPP_CONDVARIABLE_HPP */
