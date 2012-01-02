#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include "def.hh"
#include "xml.hh"
#include "idx.hh"

int main(int argc, char *argv[])
{
	if (argc < 3) {
		std::cerr << "usage: " << argv[0] << " <xml> <idx>" << std::endl;
		return 1;
	}
	int rc(0);
	try {
		// compute regions
		std::vector<region> regions(regionize(argv[1], get_cpus()));
		// start threads
		std::vector<idx_thread *> threads;
		size_t thread_id(1);
		typedef std::vector<region>::const_iterator rcit;
		typedef std::vector<idx_thread *>::iterator thit;
		for (rcit it(regions.begin()); it != regions.end(); ++it) {
			std::cout << "region: " << it->begin << '-' << it->end << std::endl;
			std::ostringstream oss;
			oss << argv[2] << "." << thread_id++;
			idx_thread *t(new idx_thread(argv[1], *it, oss.str()));
			t->start();
			threads.push_back(t);
		}
		// wait for completion + calculate statistics
		for (size_t i(1); ; i++) {
			size_t finished_count(0), articles(0);
			for (thit it(threads.begin()); it != threads.end(); ++it) {
				if ((*it)->finished()) {
					finished_count++;
				}
				articles += (*it)->article_count();
			}
			const size_t aps(articles/i);
			std::cout << "indexed " << articles << " articles "
			          << "(~" << aps << "/s)"
			          << std::endl;
			if (finished_count >= threads.size()) {
				break;
			}
			sleep(1);
		}
		std::cout << "all indexing complete; finalizing" << std::endl;
		for (thit it(threads.begin()); it != threads.end(); ++it) {
			(*it)->join();
			delete *it;
		}
	} catch (const std::runtime_error& ex) {
		std::cerr << ex.what() << std::endl;
		rc = -1;
	}
	return rc;
}
