/**
 *  @file test.cpp
 *  @date May 5, 2015
 *  @author Markus Klein
 */

#include "boost/synchronized.hpp"
#define BOOST_TEST_MODULE synchronized test
#include <boost/test/unit_test.hpp>

#include <boost/range/algorithm/sort.hpp> //only required for usage example

BOOST_AUTO_TEST_CASE( usage )
{
	using boost::synchronized;

	//! [Construction]
	synchronized<std::vector<int>> synchronized_vector {std::vector<int>(5, 42)};
	//! [Construction]

	//! [Member_access]
	auto result = synchronized_vector.lock()->at(3);
	//! [Member_access]
	BOOST_CHECK(42 == result); //Mainly suppresses the unused variable warning

	//! [Multiple_operations]
	{
		auto ptr = synchronized_vector.lock();
		ptr->at(3) = 4;
		boost::range::sort(*ptr); //You can pass a reference to other algorithms
	} //once ptr leaves scope, the lock is freed
	//! [Multiple_operations]

	struct parent{};
	struct child : parent{};

	//! [Wrap_base_class]
	synchronized<child> child_instance;
	synchronized<parent &> parent_instance = child_instance;
	//! [Wrap_base_class]
	parent_instance.lock(); //suppress warning about unused instance
}

BOOST_AUTO_TEST_CASE( dereference )
{
	boost::synchronized<int> synchronized {42};
	auto guarded_ptr = synchronized.lock();
	BOOST_CHECK(42 == *guarded_ptr);
}

BOOST_AUTO_TEST_CASE(synchronized_access)
{
	struct mock_mutex{
		bool is_locked = false;
		void lock() { is_locked = true; }
		void unlock() { is_locked = false; }
	};

	mock_mutex mutex;

	boost::synchronized<int, mock_mutex&> instance {42, mutex};

	BOOST_CHECK(not mutex.is_locked);

	{
		auto ptr = instance.lock();
		BOOST_CHECK(mutex.is_locked);
	} //Destructor frees mutex

	BOOST_CHECK(not mutex.is_locked);
}

BOOST_AUTO_TEST_CASE(maintain_const_correctness)
{
	struct test{
		bool is_const ()       { return false; }
		bool is_const () const { return true; }
	};

	boost::synchronized<test> changable;
	BOOST_CHECK(not (*changable.lock()).is_const());
	BOOST_CHECK(not changable.lock()->is_const());

	auto const & immutable = changable;
	BOOST_CHECK((*immutable.lock()).is_const());
	BOOST_CHECK(immutable.lock()->is_const());
}

BOOST_AUTO_TEST_CASE(guard_base_class)
{
	struct parent {
	};
	struct child : parent {
	};

	child instance;
	std::mutex mutex;

	boost::synchronized<parent &> reference { instance, mutex };
	BOOST_CHECK(&instance == &*reference.lock());
}

BOOST_AUTO_TEST_CASE(conversion)
{
	struct parent {
		virtual bool is_const() = 0;
		virtual bool is_const() const = 0;
	};
	struct child : parent {

		bool is_const() override { return false; }
		bool is_const() const override { return true; }
	};

	boost::synchronized<child> child_instance;
	boost::synchronized<parent &> base_reference( child_instance );
	BOOST_CHECK( not base_reference.lock()->is_const() );

	auto const & const_child = child_instance;
	boost::synchronized<parent const &> base_const_reference (const_child);
	BOOST_CHECK( base_const_reference.lock()->is_const() );
}

BOOST_AUTO_TEST_CASE(move_ptr)
{
	struct mock_mutex{
		int count_lock = 0;
		int count_unlock = 0;

		void lock(){ ++count_lock; }
		void unlock(){ ++count_unlock; }
	};

	mock_mutex mutex;

	boost::synchronized<int, mock_mutex &> sync_int( 42, mutex );
	{
		auto ptr = sync_int.lock();
		auto copy = std::move(ptr);
		auto copy_2 = std::move(ptr);
	}
	BOOST_CHECK(1 == mutex.count_lock);
	BOOST_CHECK(1 == mutex.count_unlock);
}

BOOST_AUTO_TEST_CASE(nullptr_does_not_lock)
{
	std::mutex m;
	boost::locked_ptr<char, std::mutex> ptr{nullptr, m};

	BOOST_CHECK(m.try_lock());
	m.unlock();
}

BOOST_AUTO_TEST_CASE(avoid_undefined_behaviour_if_unlock_throws)
{
	struct mock_mutex{
		void lock() {}
		void unlock() { throw std::runtime_error("out of cheese exception"); }
	} m;

	int answer = 42;

	{
		boost::locked_ptr<int, mock_mutex> ptr {&answer, m};
	} // ptr looses scope and would throw an exception if we did not prevent it
}
