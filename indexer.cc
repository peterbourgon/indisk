#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <ext/hash_map>
#include "xmlparse.hh"
#include "index_state.hh"

bool index_article(
		stream& s,
		index_state& is,
		std::ofstream& ofs_index)
{
	if (!s.read_until("<title>", parse_nil, NULL)) {
		return false;
	}
	std::string title;
	if (!s.read_until("<", parse_raw, &title)) {
		return false;
	}
	if (!s.read_until("<contributor>", parse_nil, NULL)) {
		return false;
	}
	std::string contrib;
	if (!s.read_until("</contributor>", parse_contrib, &contrib)) {
		return false;
	}
	if (!s.read_until("<text", parse_nil, NULL)) {
		return false;
	}
	if (!s.read_until(">", parse_nil, NULL)) {
		return false;
	}
	parse_text_context ctx(title, is, ofs_index);
	if (!s.read_until("</text", parse_text, &ctx)) {
		return false;
	}
	return true;
}

void merge(const std::string& header, const std::string& index)
{
	// :(
	std::ostringstream oss;
	oss << "cat " << index << " >> " << header;
	system(oss.str().c_str());
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		std::cerr << "usage: " << argv[0] << " <xml> <idx>" << std::endl;
		return 1;
	}
	stream s(argv[1]);
	std::ofstream ofs_header("index.header");
	if (!ofs_header.good()) {
		std::cerr << "index.header: bad file" << std::endl;
		return 2;
	}
	std::ofstream ofs_index(argv[2]);
	if (!ofs_index.good()) {
		std::cerr << argv[2] << ": bad file" << std::endl;
		return 3;
	}
	index_state is;
	size_t articles(0);
	while (index_article(s, is, ofs_index)) {
		if (++articles % 10000 == 0) {
			std::cout << articles << " articles indexed" << std::endl;
		}
	}
	is.finalize(ofs_index);
	is.write_header(ofs_header);
	ofs_index.close();
	ofs_header.close();
	merge("index.header", argv[2]);
	std::cout << articles << " articles indexed" << std::endl;
	return 0;
}
