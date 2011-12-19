#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <ext/hash_map>
#include "xmlparse.hh"
#include "index_state.hh"

void write_index_header(const index_state& is, std::ostream& ofs_header)
{
	if (!is.finalized) {
		throw std::runtime_error("can't write index header without finalize");
	}
	
	// <uint32_t offset where header ends and inverted index begins> '\n'
	// <uint32_t number of articles> '\n'
	// <uint32_t article ID> <article name as text> '\n'
	//  . . .
	// <uint32_t number of terms>\n
	// <uint32_t term ID> <term as text> '|' <uint32_t offset 1> ... '\n'
	//  . . .
	
	typedef index_state::str_id_map::const_iterator sidcit;
	typedef index_state::offset_vector::const_iterator ovcit;
	typedef index_state::tid_offsets_map::const_iterator tocit;
	uint32_t asz(is.articles.size()), tsz(is.terms.size()), offset(0);
	
	// write initial offset position (will put correct value later)
	ofs_header << offset << '\n';
	
	// write article block
	ofs_header << asz << '\n';
	for (sidcit it(is.articles.begin()); it != is.articles.end(); ++it) {
		ofs_header << it->second << it->first << '\n';
	}
	
	// write term block
	ofs_header << tsz << '\n';
	for (sidcit it(is.terms.begin()); it != is.terms.end(); ++it) {
		ofs_header << it->second << it->first << '|';
		tocit tgt(is.tid_offsets.find(it->second));
		if (tgt == is.tid_offsets.end()) {
			throw std::runtime_error("invalid state in index_state");
		}
		const index_state::offset_vector& offsets(tgt->second);
		for (ovcit it2(offsets.begin()); it2 != offsets.end(); ++it2) {
			ofs_header << *it2;
		}
		ofs_header << '\n';
	}
	
	// back-fill the offset position
	offset = ofs_header.tellp();
	ofs_header.seekp(0);
	ofs_header << offset;
	ofs_header.seekp(offset);
}

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
	write_index_header(is, ofs_header);
	ofs_index.close();
	ofs_header.close();
	merge("index.header", argv[2]);
	return 0;
}
