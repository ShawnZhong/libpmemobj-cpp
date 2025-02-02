// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/**
 * @file
 * Pmem-resident shared mutex.
 */

#ifndef LIBPMEMOBJ_CPP_SHARED_MUTEX_HPP
#define LIBPMEMOBJ_CPP_SHARED_MUTEX_HPP

#include <libpmemobj/thread.h>
#include <libpmemobj/tx_base.h>

namespace pmem
{

namespace obj
{

/**
 * Persistent memory resident shared_mutex implementation.
 *
 * This class is an implementation of a PMEM-resident shared_mutex
 * which mimics in behavior the C++11 std::mutex. This class
 * satisfies all requirements of the SharedMutex and StandardLayoutType
 * concepts. The typical usage would be:
 * @snippet mutex/mutex.cpp shared_mutex_example
 * @ingroup synchronization
 */
class shared_mutex {
public:
	/** Implementation defined handle to the native type. */
	typedef PMEMrwlock *native_handle_type;

	/**
	 * Default constructor.
	 *
	 * @throw lock_error when the shared_mutex is not from persistent
	 * memory.
	 */
	shared_mutex()
	{
		PMEMobjpool *pop;
		if ((pop = pmemobj_pool_by_ptr(&plock)) == nullptr)
			throw pmem::lock_error(
				1, std::generic_category(),
				"Persistent shared mutex not from persistent memory.");

		pmemobj_rwlock_zero(pop, &plock);
	}

	/**
	 * Defaulted destructor.
	 */
	~shared_mutex() = default;

	/**
	 * Lock the mutex for exclusive access.
	 *
	 * If a different thread already locked this mutex, the calling
	 * thread will block. If the same thread tries to lock a mutex
	 * it already owns, either in exclusive or shared mode,
	 * the behavior is undefined.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	void
	lock()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		if (int ret = pmemobj_rwlock_wrlock(pop, &this->plock))
			throw detail::exception_with_errormsg<lock_error>(
				ret, std::system_category(),
				"Failed to lock a shared mutex.");
	}

	/**
	 * Lock the mutex for shared access.
	 *
	 * If a different thread already locked this mutex for exclusive
	 * access, the calling thread will block. If it was locked for
	 * shared access by a different thread, the lock will succeed.
	 *
	 * The mutex can be locked for shared access multiple times
	 * by the same thread. If so, the same number of unlocks must be
	 * made to unlock the mutex.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	void
	lock_shared()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		if (int ret = pmemobj_rwlock_rdlock(pop, &this->plock))
			throw detail::exception_with_errormsg<lock_error>(
				ret, std::system_category(),
				"Failed to shared lock a shared mutex.");
	}

	/**
	 * Try to lock the mutex for exclusive access, returns
	 * regardless if the lock succeeds.
	 *
	 * If the same thread tries to lock a mutex it already owns
	 * either in exclusive or shared mode, the behavior is undefined.
	 *
	 * @return `true` on successful lock acquisition, `false`
	 * otherwise.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	bool
	try_lock()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		int ret = pmemobj_rwlock_trywrlock(pop, &this->plock);

		if (ret == 0)
			return true;
		else if (ret == EBUSY)
			return false;
		else
			throw detail::exception_with_errormsg<lock_error>(
				ret, std::system_category(),
				"Failed to lock a shared mutex.");
	}

	/**
	 * Try to lock the mutex for shared access, returns
	 * regardless if the lock succeeds.
	 *
	 * The mutex can be locked for shared access multiple times
	 * by the same thread. If so, the same number of unlocks must be
	 * made to unlock the mutex. If the calling thread already owns
	 * the mutex in any mode, the behavior is undefined.
	 *
	 * @return `false` if a different thread already locked the
	 * mutex for exclusive access, `true` otherwise.
	 *
	 * @throw lock_error when an error occurs, this includes all
	 * system related errors with the underlying implementation of
	 * the mutex.
	 */
	bool
	try_lock_shared()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		int ret = pmemobj_rwlock_tryrdlock(pop, &this->plock);

		if (ret == 0)
			return true;
		else if (ret == EBUSY)
			return false;
		else
			throw detail::exception_with_errormsg<lock_error>(
				ret, std::system_category(),
				"Failed to lock a shared mutex.");
	}

	/**
	 * Unlocks the mutex.
	 *
	 * The mutex must be locked for exclusive access by the calling
	 * thread, otherwise results in undefined behavior.
	 */
	void
	unlock()
	{
		PMEMobjpool *pop = pmemobj_pool_by_ptr(this);
		int ret = pmemobj_rwlock_unlock(pop, &this->plock);
		if (ret)
			throw detail::exception_with_errormsg<lock_error>(
				ret, std::system_category(),
				"Failed to unlock a shared mutex.");
	}

	/**
	 * Unlocks the mutex.
	 *
	 * The mutex must be locked for shared access by the calling
	 * thread, otherwise results in undefined behavior.
	 */
	void
	unlock_shared()
	{
		this->unlock();
	}

	/**
	 * Access a native handle to this shared mutex.
	 *
	 * @return a pointer to PMEMmutex.
	 */
	native_handle_type
	native_handle() noexcept
	{
		return &this->plock;
	}

	/**
	 * The type of lock needed for the transaction API.
	 *
	 * @return TX_PARAM_RWLOCK
	 */
	enum pobj_tx_param
	lock_type() const noexcept
	{
		return TX_PARAM_RWLOCK;
	}

	/**
	 * Deleted assignment operator.
	 */
	shared_mutex &operator=(const shared_mutex &) = delete;

	/**
	 * Deleted copy constructor.
	 */
	shared_mutex(const shared_mutex &) = delete;

private:
	/** A POSIX style PMEM-resident shared_mutex.*/
	PMEMrwlock plock;
};

} /* namespace obj */

} /* namespace pmem */

#endif /* LIBPMEMOBJ_CPP_SHARED_MUTEX_HPP */
