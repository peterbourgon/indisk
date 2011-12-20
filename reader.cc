#include <iostream>
#include <fstream>
#include <cassert>
#include <stdexcept>
#include "definitions.hh"

template<typename T>
void read(std::ifstream& ifs, T& t)
{
	ifs.read(reinterpret_cast<char *>(&t), sizeof(T));
}

struct index_repr {
	index_repr(const std::string& filename)
	: ifs_ptr(new std::ifstream(filename.c_str(), std::ios::binary))
	, index_offset(0)
	, articles(0)
	, terms(0)
	{
		if (!ifs_ptr->good()) {
			throw std::runtime_error("bad index file");
		}
	}

	~index_repr()
	{
		if (ifs_ptr) {
			ifs_ptr->close();
			delete ifs_ptr;
		}
	}
	
	std::ifstream *ifs_ptr;

	uint32_t index_offset;
	uint32_t articles;
	uint32_t terms;
	
	aid_title_map aid_title;
	term_hov_map term_hov;
	
	void parse()
	{
		char c;
		assert(ifs_ptr);
		std::ifstream& ifs(*ifs_ptr);

		// <uint32_t index_offset> '\n'
		read<uint32_t>(ifs, index_offset);
		read<char>(ifs, c);
		assert(c == '\n');
		
		// <uint32_t article count> '\n'
		// <uint32_t article ID> <article title> '\n'
		//  . . .
		read<uint32_t>(ifs, articles);
		read<char>(ifs, c);
		assert(c == '\n');
		for (size_t i(0); i < articles; ++i) {
			uint32_t articleid(0);
			read<uint32_t>(ifs, articleid);
			assert(articleid > 0);
			assert(aid_title.find(articleid) == aid_title.end());
			std::string title;
			std::getline(ifs, title);
			assert(!title.empty());
			aid_title[articleid] = title;
		}

		// <uint32_t term count> '\n'
		// <uint32_t term ID> <term> '|' <uint32_t offset> . . . 
		//    <uint32_t UINT32_MAX> '\n'
		//  . . .
		read<uint32_t>(ifs, terms);
		read<char>(ifs, c);
		assert(c == '\n');
		for (size_t i(0); i < terms; ++i) {
			uint32_t termid(0);
			read<uint32_t>(ifs, termid);
			assert(termid > 0);
			std::string term;
			std::getline(ifs, term, '|');
			assert(!term.empty());
			assert(term_hov.find(term) == term_hov.end());
			header_offset_vector hov;
			uint32_t header_offset(0);
			while (true) {
				read<uint32_t>(ifs, header_offset);
				if (header_offset == UINT32_MAX) {
					break;
				}
				header_offset += index_offset;
				assert(header_offset > 0);
				std::cout << termid << ": " << term << " offset @" << header_offset << std::endl;
				hov.push_back(header_offset);
			}
			assert(!hov.empty());
			term_hov[term] = hov;
			read<char>(ifs, c);
			assert(c == '\n');
		}
	}

	void search(const std::string& term, std::vector<std::string>& articles) const
	{
		assert(ifs_ptr);
		term_hov_map::const_iterator tgt(term_hov.find(term));
		if (tgt == term_hov.end()) {
			return;
		}
		std::ifstream& ifs(*ifs_ptr);
		const header_offset_vector& hov(tgt->second);
		std::cout << term << ": " << hov.size() << " offset(s)" << std::endl;
		typedef header_offset_vector::const_iterator hovcit;
		std::vector<uint32_t> articleids;
		char c(0);
		for (hovcit it(hov.begin()); it != hov.end(); ++it) {
			const uint32_t term_offset(*it);
			std::cout << term << ": offset " << term_offset << std::endl;
			// <uint32_t term ID> <uint32_t article ID> . . . <uint32_t UINT32_MAX> '\n'
			ifs.seekg(term_offset);
			uint32_t termid(0);
			read<uint32_t>(ifs, termid);
			assert(termid > 0);
			std::cout << term << " id " << termid << std::endl;
			uint32_t articleid(UINT32_MAX);
			while (true) {
				read<uint32_t>(ifs, articleid);
				if (articleid == UINT32_MAX) {
					break;
				}
				articleids.push_back(articleid);
			}
			read<char>(ifs, c);
			assert(c == '\n');
		}
		std::cout << term << " yielded " << articleids.size() << " articles" << std::endl;
		articles.reserve(articleids.size());
		typedef std::vector<uint32_t>::const_iterator uvcit;
		for (uvcit it(articleids.begin()); it != articleids.end(); ++it) {
			aid_title_map::const_iterator tgt2(aid_title.find(*it));
			assert(tgt2 != aid_title.end());
			std::cout << '\t' << tgt2->second << std::endl;
			articles.push_back(tgt2->second);
		}
	}
};

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "usage: " << argv[0] << " <index>" << std::endl;
		return 1;
	}
	index_repr idx(argv[1]);
	idx.parse();
	std::cout << "offset begins at " << idx.index_offset << std::endl;
	std::cout << idx.articles << " articles" << std::endl;
	std::cout << idx.terms << " terms" << std::endl;
	
	size_t link_count(0);
	typedef term_hov_map::const_iterator thcit;
	for (thcit it(idx.term_hov.begin()); it != idx.term_hov.end(); ++it) {
		link_count += it->second.size();
	}
	std::cout << link_count << " term-offset links" << std::endl;

	std::vector<std::string> search_results;
	idx.search("poetry", search_results);
	std::cout << "poetry: " << search_results.size() << " hits" << std::endl;
	
	return 0;
}
