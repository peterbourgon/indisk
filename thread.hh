#ifndef THREAD_HH_
#define THREAD_HH_

#include <pthread.h>

// from boost
class noncopyable
{
protected:
	noncopyable() {}
	~noncopyable() {}

private:
	// emphasize the following members are private
	noncopyable(const noncopyable&);
	const noncopyable& operator=(const noncopyable&);
};

class scoped_lock : private noncopyable
{
public:
	explicit scoped_lock(pthread_mutex_t& mx, bool initially_locked=true);
	~scoped_lock();
	
	void lock();
	void unlock();
	bool locked() const;

private:
	pthread_mutex_t& m_mutex;
	bool m_locked;
};

enum mutex_type {
	MUTEX_TYPE_NORMAL,
	MUTEX_TYPE_RECURSIVE
};

class monitor
{
protected:
	mutable pthread_mutex_t monitor_mutex;
	mutable pthread_cond_t monitor_condition;

public:
	monitor();
	monitor(mutex_type t);
	virtual ~monitor();
	void wait();
	bool timed_wait_ms(int i_duration_ms);
	void notify_one();
	void notify_all();
};

class threadbase
{
public:
	threadbase();
	virtual ~threadbase();
	void start();
	void join();
	virtual void run() = 0;
	
private:
	pthread_t *m_threadbase_ptr;
};

class synchronized_threadbase : public threadbase, public monitor
{
public:
	synchronized_threadbase();
	virtual ~synchronized_threadbase();
	virtual void stop();
	void safe_start();
	
protected:
	bool synchronized_thread_running;
};

#endif 
