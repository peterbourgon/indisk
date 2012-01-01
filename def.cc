#include "def.hh"

extern "C" {
	#include <unistd.h>
	#include <stdlib.h>
	#include <sys/types.h>
	#include <sys/sysctl.h>
}

size_t get_cpus()
{
	// Optimize indexing based on available cores.
	// http://stackoverflow.com/questions/150355
	
	const char *threads_env(getenv("THREADS"));
	if (threads_env) {
		int threads(atoi(threads_env));
		if (threads > 0) {
			return threads;
		}
	}
	
#ifdef __linux__
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif
	
#ifdef __APPLE__
	size_t cores(1);
	int mib[4];
	size_t len(sizeof(cores));
	mib[0] = CTL_HW;
	mib[1] = HW_AVAILCPU;
	sysctl(mib, 2, &cores, &len, NULL, 0);
	if (cores < 1) {
		mib[1] = HW_NCPU;
		sysctl(mib, 2, &cores, &len, NULL, 0);
		if (cores < 1) {
			cores = 1;
		}
	}
	return cores;
#endif
	
	return 1;
}

