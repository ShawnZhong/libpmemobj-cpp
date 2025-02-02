// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2021, Intel Corporation */

/**
 * @file
 * Persistent_ptr atomic allocation functions for objects. The typical usage
 * examples would be:
 * @snippet make_persistent/make_persistent.cpp make_atomic_example
 */

#ifndef LIBPMEMOBJ_CPP_MAKE_PERSISTENT_ATOMIC_HPP
#define LIBPMEMOBJ_CPP_MAKE_PERSISTENT_ATOMIC_HPP

#include <libpmemobj++/allocation_flag.hpp>
#include <libpmemobj++/detail/check_persistent_ptr_array.hpp>
#include <libpmemobj++/detail/common.hpp>
#include <libpmemobj++/detail/make_atomic_impl.hpp>
#include <libpmemobj++/detail/variadic.hpp>
#include <libpmemobj++/make_persistent_array_atomic.hpp>
#include <libpmemobj++/pexceptions.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj/atomic_base.h>

#include <tuple>

namespace pmem
{

namespace obj
{

/**
 * Atomically allocate and construct an object.
 *
 * Constructor parameters are passed through variadic parameters. Do *NOT* use
 * this inside transactions, as it might lead to undefined behavior in the
 * presence of transaction aborts.
 *
 * @param[in,out] pool the pool from which the object will be allocated.
 * @param[in,out] ptr the persistent pointer to which the allocation
 * will take place.
 * @param[in] flag affects behaviour of allocator
 * @param[in] args variadic function parameter containing all parameters
 * passed to the objects constructor.
 *
 * @throw std::bad_alloc on allocation failure.
 * @ingroup allocation
 */
template <typename T, typename... Args>
void
make_persistent_atomic(pool_base &pool,
		       typename detail::pp_if_not_array<T>::type &ptr,
		       allocation_flag_atomic flag, Args &&... args)
{
	auto arg_pack = std::forward_as_tuple(std::forward<Args>(args)...);
	auto ret = pmemobj_xalloc(
		pool.handle(), ptr.raw_ptr(), sizeof(T), detail::type_num<T>(),
		flag.value,
		&detail::obj_constructor<T, decltype(arg_pack), Args...>,
		static_cast<void *>(&arg_pack));

	if (ret != 0)
		throw std::bad_alloc();
}

/**
 * Atomically allocate and construct an object.
 *
 * Constructor parameters are passed through variadic parameters. Do *NOT* use
 * this inside transactions, as it might lead to undefined behavior in the
 * presence of transaction aborts.
 *
 * @param[in,out] pool the pool from which the object will be allocated.
 * @param[in,out] ptr the persistent pointer to which the allocation
 * will take place.
 * @param[in] args variadic function parameter containing all parameters
 * passed to the objects constructor.
 *
 * @throw std::bad_alloc on allocation failure.
 * @ingroup allocation
 */
template <typename T, typename... Args>
typename std::enable_if<!detail::is_first_arg_same<allocation_flag_atomic,
						   Args...>::value>::type
make_persistent_atomic(pool_base &pool,
		       typename detail::pp_if_not_array<T>::type &ptr,
		       Args &&... args)
{
	make_persistent_atomic<T>(pool, ptr, allocation_flag_atomic::none(),
				  std::forward<Args>(args)...);
}

/**
 * Atomically deallocate an object.
 *
 * There is no way to atomically destroy an object. Any object specific
 * cleanup must be performed elsewhere.  Do *NOT* use this inside transactions,
 * as it might lead to undefined behavior in the presence of transaction aborts.
 *
 * param[in,out] ptr the persistent_ptr whose pointee is to be
 * deallocated.
 * @ingroup allocation
 */
template <typename T>
void
delete_persistent_atomic(
	typename detail::pp_if_not_array<T>::type &ptr) noexcept
{
	if (ptr == nullptr)
		return;

	/* we CAN'T call the destructor */
	pmemobj_free(ptr.raw_ptr());
}

} /* namespace obj */

} /* namespace pmem */

#endif /* LIBPMEMOBJ_CPP_MAKE_PERSISTENT_ATOMIC_HPP */
