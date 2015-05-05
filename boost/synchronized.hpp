#pragma once
/**
 *  @file synchronized.hpp
 *  @date May 5, 2015
 *  @author Markus Klein
 */

#include <mutex>
#include <tuple>
#include <utility>

namespace boost {

	/**
	 * @brief As long as an instance lives, you own the lock to the resource it points to
	 * @sa boost::synchronized
	 */
	template <typename Resource, typename Mutex>
	class locked_ptr{

	public:

		locked_ptr( locked_ptr const &) = delete;
		locked_ptr& operator =( locked_ptr const &) = delete;

		/**
		 * @brief Constructor acquires lock to mutex
		 * @param ptr Pointer to resource this instance will point to
		 * @param mutex Reference to mutex those lock will be acquired
		 */
		locked_ptr(Resource * ptr, Mutex & mutex)
			: ptr_{ptr}, mutex_(mutex)
		{
			if(ptr != nullptr){
				mutex_.lock();
			}
		}

		locked_ptr( locked_ptr && other) noexcept
			: ptr_(other.ptr_), mutex_(other.mutex_)
		{
			other.ptr_ = nullptr;
		}

		/**
		 * @brief Dereferences pointer
		 */
		Resource & operator* () const noexcept
		{
			return *ptr_;
		}

		/**
		 * @brief Allows direct access to members of resource pointed to
		 */
		Resource * operator-> () const noexcept
		{
			return ptr_;
		}

		/**
		 * @brief Unlocks the mutex
		 */
		~locked_ptr() noexcept
		{
			if (ptr_ != nullptr){
				try{mutex_.unlock();} catch (...){};
			}
		}

	private:
		Resource * ptr_;
		Mutex & mutex_;
	};

	/**
	 * @brief Wraps an instance for synchronized access
	 * @tparam Resource Type of the resource to be mapped. You may choose a reference type to indicate that the wrapper does not have ownership.
	 * @tparam Mutex defaults to std::mutex or std::mutex reference in case Resource is an lvalue reference.
	 *
	 * Multi-threaded applications are ofter required to share resources between threads.
	 * Sometimes this is done by creating a thread-aware class which internally holds
	 * a mutex. Whenever its methods require exclusive access to some protected member,
	 * these methods acquire a lock of the class mutex. This pattern has a few disadvantages:
	 *
	 * \li Any time you write a class, you must think of whether it will be used in a
	 * multi-threaded environment.
	 * \li You litter your class with locks and mutexes, making them difficult to test
	 * for correctness
	 * \li When you want to use a class whose source code you do not control, you always
	 * need to write a wrapper class for exclusion management.
	 *
	 * The synchronized class template provides an alternative means of protecting resources.
	 * Instead of managing locks directly in the resource class, responsibility of locking
	 * is moved to where the resource is used.
	 *
	 * The most straightforward use of the synchronized pattern is to wrap a
	 * resource and an object by value:
	 *
	 * \snippet test.cpp Construction
	 *
	 * Above line would create <tt>synchronized_vector</tt> as a frontend for a newly created
	 * vector instance. To access the inner vector, you must obtain a lock:
	 *
	 * \snippet test.cpp Member_access
	 *
	 * You can acquire a lock once for multiple operations and release in when you are done completely:
	 *
	 * \snippet test.cpp Multiple_operations
	 *
	 * In scenarios where polymorphism is required, the following conversion may come in handy:
	 *
	 * \snippet test.cpp Wrap_base_class
	 * Please note the <tt>&amp;</tt> in the type declaration. It indicates that <tt>parent_instance</tt> uses the
	 * same data and mutex as <tt>child_instance</tt>. Whenever you acquire a lock via <tt>parent_instance</tt>,
	 * you cannot acquire a lock via <tt>child_instance</tt> (and vice versa) at the same time.
	 */
	template<
		typename Resource,
		typename Mutex = typename std::conditional<std::is_lvalue_reference<Resource>::value, std::mutex&, std::mutex>::type
	>
	class synchronized{

	public:

		/**
		 * @brief Resource Type
		 */
		using resource_type = typename std::remove_reference<Resource>::type;

		/**
		 * @brief Mutex Type
		 */
		using mutex_type = typename std::remove_reference<Mutex>::type;

		/**
		 * @brief Pointer type returned by calls to lock on a mutable instance
		 */
		using locked_mutable_ptr = locked_ptr<resource_type, mutex_type>;

		/**
		 * @brief Pointer type returned by calls to lock on a constant instance
		 */
		using locked_const_ptr = locked_ptr<resource_type const, mutex_type>;

		//friend declaration required to support casts
		template <typename R, typename M> friend class synchronized;

		/**
		 * @brief Default Construction, owns mutex and default constructed resource
		 */
		synchronized() = default;

		synchronized( synchronized const & ) = default;
		synchronized& operator= ( synchronized const & ) = default;

		/**
		 * @brief Constructs a an instance from a Resource and a mutex
		 * @param resource Resource to be guarded by mutex
		 * @param mutex Mutex to guard resource
		 */
		template<typename Resource_Arg, typename Mutex_Arg>
		synchronized(Resource_Arg && resource, Mutex_Arg && mutex)
			: mutex_(std::forward<Mutex_Arg>(mutex))
			, resource_(std::forward<Resource_Arg>(resource))
		{}

		/**
		 * @brief Constructs an instance from a Resource
		 * @param resource Resource to be guarded by mutex
		 *
		 * Mutex will be default constructed
		 */
		template <typename Resource_Arg>
		explicit synchronized(Resource_Arg && resource):
			resource_(std::forward<Resource_Arg>(resource))
		{}

		/**
		 * @brief Implicit Conversion from mutable instance
		 */
		template<typename Other_resource, typename Other_mutex>
		synchronized(synchronized<Other_resource, Other_mutex> & other):
			synchronized(other.resource_, other.mutex_)
		{}

		/**
		 * @brief Implicit Conversion from constant instance
		 */
		template<typename Other_resource, typename Other_mutex>
		synchronized(synchronized<Other_resource, Other_mutex> const & other):
			synchronized(other.resource_, other.mutex_)
		{}

		/**
		 * @brief Provides access to the guarded resource
		 * @return A mutable locked pointer
		 */
		locked_mutable_ptr lock()
		{
			return locked_mutable_ptr(std::addressof(resource_), mutex_);
		}

		/**
		 * @brief Provides access to the guarded resource
		 * @return An immutable locked pointer
		 */
		locked_const_ptr lock() const
		{
			return locked_const_ptr(std::addressof(resource_), mutex_);
		}

	private:

		mutable Mutex mutex_;
		Resource resource_;
};
}
