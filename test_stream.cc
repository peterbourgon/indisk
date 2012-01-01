#include <iostream>
#include <stdexcept>
#include <sstream>
#include "xml.hh"

void ensure(bool condition, const std::string& file, int line)
{
	if (!condition) {
		std::ostringstream oss;
		oss << "ensure failed at " << file << ":" << line;
		throw std::runtime_error(oss.str());
	}
}

#define ENSURE(x) ensure(x, __FILE__, __LINE__)

void test_basic_reading()
{
	stream s("data/short.xml", region(0, 0));
	ENSURE(s.size() == 27737);
	ENSURE(s.read_until("<title>", true, NULL, NULL));
	ENSURE(s.read(5) == "April");
	ENSURE(s.read(5) == "April"); // read should reset the cursor
	ENSURE(s.read_until("<contributor>", true, NULL, NULL));
	ENSURE(s.read_until("<username>", true, NULL, NULL));
	ENSURE(s.read(3) == "Chu");
}

void test_regionize()
{
	std::vector<region> regions(regionize("data/short.xml", 2));
	ENSURE(regions.size() == 2);
	ENSURE(regions.at(0).begin == 0);
	ENSURE(regions.at(1).end == 27737);
	ENSURE(regions.at(0).end == regions.at(1).begin);
	std::cout << "test_regionize: "
	          << regions.at(0).begin << "-" << regions.at(0).end << ", "
	          << regions.at(1).begin << "-" << regions.at(1).end << std::endl;
}

void test_regionized_reading()
{
	std::vector<region> regions(regionize("data/short.xml", 2));
	ENSURE(regions.size() == 2);
	stream s1("data/short.xml", regions.at(0));
	stream s2("data/short.xml", regions.at(1));
	
	ENSURE(s1.tell() == regions.at(0).begin);
	ENSURE(s1.read_until("<title>", true, NULL, NULL));
	ENSURE(s1.read(5) == "April");
	ENSURE(s1.read_until("<title>", true, NULL, NULL));
	ENSURE(s1.read(6) == "August");
	ENSURE(s1.read_until("<title>", true, NULL, NULL));
	ENSURE(s1.read(3) == "Art");
	ENSURE(!s1.read_until("<title>", true, NULL, NULL));
	ENSURE(s1.tell() == regions.at(0).end);
	
	ENSURE(s2.tell() == regions.at(1).begin);
	ENSURE(s2.read_until("<title>", true, NULL, NULL));
	ENSURE(s2.read(9) == "A</title>");
	ENSURE(s2.read_until("<title>", true, NULL, NULL));
	ENSURE(s2.read(3) == "Air");
	ENSURE(!s2.read_until("<title>", true, NULL, NULL));
	ENSURE(s2.tell() == regions.at(1).end);
}

int main()
{
	int rc(0);
	try {
		test_basic_reading();
		test_regionize();
		test_regionized_reading();
		std::cout << "success" << std::endl;
	} catch (const std::runtime_error& ex) {
		std::cerr << ex.what() << std::endl;
		rc = -1;
	}
	return rc;
}

