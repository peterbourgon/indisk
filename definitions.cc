#include "definitions.hh"

extern "C" {
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/sysctl.h>
}

size_t cpus()
{
	// Optimize indexing based on available cores.
	// http://stackoverflow.com/questions/150355

#ifdef __linux__
	return sysconf(_SC_NPROCESSORS_ONLN);
#endif

#ifdef __APPLE__ & __MACH__
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

