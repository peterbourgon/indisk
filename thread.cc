#include <stdexcept>
#include "thread.hh"

extern "C" {
	#include <sys/time.h>
}

scoped_lock::scoped_lock(pthread_mutex_t& mx, bool initially_locked)
: m_mutex(mx), m_locked(false)
{
	if (initially_locked) {
		lock();
	}
}

scoped_lock::~scoped_lock()
{
	if (m_locked) {
		unlock();
	}
}

void scoped_lock::lock()
{
	if (m_locked) {
		throw std::runtime_error("double-lock");
	}
	pthread_mutex_lock(&m_mutex);
	m_locked = true;
}

void scoped_lock::unlock()
{
	if (!m_locked) {
		throw std::runtime_error("double-unlock");
	}
	pthread_mutex_unlock(&m_mutex);
	m_locked = false;
}

bool scoped_lock::locked() const 
{
	return m_locked;
}

monitor::monitor()
{
	if (pthread_mutex_init(&monitor_mutex, NULL) != 0) {
		throw std::runtime_error("couldn't initialize Monitor mutex");
	}
	if (pthread_cond_init(&monitor_condition, NULL) != 0) {
		throw std::runtime_error("couldn't initialize Monitor condition");
	}
}

monitor::monitor(mutex_type t)
{
	switch (t) {
	case MUTEX_TYPE_RECURSIVE:
		pthread_mutexattr_t mta;
		if (pthread_mutexattr_init(&mta) != 0) {
			throw std::runtime_error("couldn't initialize monitor mutex attr");
		}
		if (pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE) != 0) {
			throw std::runtime_error("couldn't settype monitor mutex attr");
		}
		if (pthread_mutex_init(&monitor_mutex, &mta) != 0) {
			throw std::runtime_error("couldn't initialize monitor mutex");
		}
		if (pthread_cond_init(&monitor_condition, NULL) != 0) {
			throw std::runtime_error("couldn't initialize monitor mutex");
		}
		break;
	default:
		if (pthread_mutex_init(&monitor_mutex, NULL) != 0) {
			throw std::runtime_error("couldn't initialize Monitor mutex");
		}
		if (pthread_cond_init(&monitor_condition, NULL) != 0) {
			throw std::runtime_error("couldn't initialize Monitor condition");
		}
		break;
	}
}

monitor::~monitor()
{
	//
}

void monitor::wait()
{
	pthread_cond_wait(&monitor_condition, &monitor_mutex);
}

#define NSEC_IN_MSEC 1000000
#define NSEC_IN_SEC 1000000000
#define MSEC_IN_SEC 1000
bool monitor::timed_wait_ms(int i_duration_ms)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	struct timespec delay;
	delay.tv_sec  = now.tv_sec + i_duration_ms / MSEC_IN_SEC;
	delay.tv_nsec = now.tv_usec*1000 + (i_duration_ms % MSEC_IN_SEC) * NSEC_IN_MSEC;
	if (delay.tv_nsec >= NSEC_IN_SEC) {
		delay.tv_nsec -= NSEC_IN_SEC;
		delay.tv_sec++;
	}
	int rc(pthread_cond_timedwait(&monitor_condition, &monitor_mutex, &delay));
	return rc == 0;
}

void monitor::notify_one()
{
	pthread_cond_signal(&monitor_condition);
}

void monitor::notify_all()
{
	pthread_cond_broadcast(&monitor_condition);
}

static void * threadbase_dispatcher(void *arg)
{
	threadbase *tb(static_cast<threadbase *>(arg));
	tb->run();
	return NULL;
}

threadbase::threadbase()
: m_threadbase_ptr(NULL)
{
	//
}

threadbase::~threadbase()
{
	//
}

void threadbase::start()
{
	if (!m_threadbase_ptr) {
		m_threadbase_ptr = new pthread_t;
		int rc(pthread_create(m_threadbase_ptr, NULL, threadbase_dispatcher, this));
		if (rc != 0) {
			throw std::runtime_error("threadbase pthread_create failed");
		}
	} else {
		throw std::runtime_error("multiple threadbase start");
	}
}

void threadbase::join()
{
	if (m_threadbase_ptr) {
		int rc(pthread_join(*m_threadbase_ptr, NULL));
		if (rc != 0) {
			throw std::runtime_error("threadbase pthread_join failed");
		}
		delete m_threadbase_ptr;
		m_threadbase_ptr = NULL;
	}
}

synchronized_threadbase::synchronized_threadbase()
: synchronized_thread_running(false)
{
	//
}

synchronized_threadbase::~synchronized_threadbase()
{
	//
}

void synchronized_threadbase::stop()
{
	scoped_lock sync(monitor_mutex);
	synchronized_thread_running = false;
	notify_all();
}

void synchronized_threadbase::safe_start()
{
	threadbase::start();
	scoped_lock sync(monitor_mutex);
	while (!synchronized_thread_running) {
		timed_wait_ms(50);
	}
}

