#include <iostream>
#include <fstream>
#include <cassert>

template<typename T>
void read(std::ifstream& ifs, T& t)
{
	ifs.read(reinterpret_cast<char *>(&t), sizeof(T));
}

struct index_repr {
	index_repr() : index_offset(0), articles(0), terms(0) { }
	
	uint32_t index_offset;
	uint32_t articles;
	uint32_t terms;
	
	void parse(std::ifstream& ifs)
	{
		char c;
		read<uint32_t>(ifs, index_offset);
		read<char>(ifs, c);
		assert(c == '\n');
		read<uint32_t>(ifs, articles);
		read<char>(ifs, c);
		assert(c == '\n');
		std::string line;
		for (size_t i(0); i < articles; i++, std::getline(ifs, line));
		read<uint32_t>(ifs, terms);
		read<char>(ifs, c);
		assert(c == '\n');
	}
};

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "usage: " << argv[0] << " <index>" << std::endl;
		return 1;
	}
	std::ifstream ifs(argv[1], std::ios::binary);
	if (!ifs.good()) {
		std::cerr << argv[1] << ": bad file" << std::endl;
		return 2;
	}
	index_repr idx;
	idx.parse(ifs);
	std::cout << "offset begins at " << idx.index_offset << std::endl;
	std::cout << idx.articles << " articles" << std::endl;
	std::cout << idx.terms << " terms" << std::endl;
	return 0;
}
