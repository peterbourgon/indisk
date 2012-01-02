#include <iostream>
#include <fstream>
#include <cassert>
#include <stdexcept>
#include <map>
#include "def.hh"
#include "search.hh"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "usage: " << argv[0] << " <idx> [<idx> ...]" << std::endl;
		return 1;
	}
	std::vector<std::string> filenames;
	for (int i(1); i < argc; i++) {
		filenames.push_back(argv[i]);
	}
	size_t indices(init_indices(filenames));
	std::cout << "searching " << indices << " index files" << std::endl;
	int rc(0);
	try {
		while (true) {
			std::string input;
			std::cout << "> ";
			std::getline(std::cin, input);
			std::transform(input.begin(), input.end(), input.begin(), tolower);
			if (input == "quit") {
				break;
			}
			search_results r(search_indices(input));
			std::cout << input << ": " << r.total << " hits" << std::endl;
			typedef std::vector<search_result>::const_iterator srit;
			for (srit it(r.top.begin()); it != r.top.end(); ++it) {
				std::cout << it->article << " (" << it->weight << ")" << std::endl;
			}
		}
	} catch (const std::runtime_error& ex) {
		std::cerr << ex.what() << std::endl;
		rc = -1;
	}
	return rc;
}
