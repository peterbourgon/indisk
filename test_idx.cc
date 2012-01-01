#include <iostream>
#include <stdexcept>
#include <sstream>
#include "idx.hh"

void ensure(bool condition, const std::string& file, int line)
{
	if (!condition) {
		std::ostringstream oss;
		oss << "ensure failed at " << file << ":" << line;
		throw std::runtime_error(oss.str());
	}
}

#define ENSURE(x) ensure(x, __FILE__, __LINE__)

int main()
{
	int rc(0);
	try {
		std::cout << "success" << std::endl;
	} catch (const std::runtime_error& ex) {
		std::cerr << ex.what() << std::endl;
		rc = -1;
	}
	return rc;
}

