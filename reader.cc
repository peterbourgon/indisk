#include <iostream>
#include <fstream>
#include <cassert>
#include <stdexcept>
#include <map>
#include "definitions.hh"

template<typename T>
void read(std::ifstream& ifs, T& t)
{
	ifs.read(reinterpret_cast<char *>(&t), sizeof(T));
}

#define MAX_SEARCH_RESULTS 25

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
		
		// header section:
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
		// <uint32_t term ID> <term> END_DELIM
		//    <uint32_t offset> . . . <uint32_t UINT32_MAX> '\n'
		//  . . .
		read<uint32_t>(ifs, terms);
		read<char>(ifs, c);
		assert(c == '\n');
		for (size_t i(0); i < terms; ++i) {
			uint32_t termid(0);
			read<uint32_t>(ifs, termid);
			assert(termid > 0);
			std::string term;
			std::getline(ifs, term, END_DELIM);
			assert(!term.empty());
			header_offset_vector hov;
			uint32_t header_offset(0);
			while (true) {
				read<uint32_t>(ifs, header_offset);
				if (header_offset == UINT32_MAX) {
					break;
				}
				header_offset += index_offset;
				assert(header_offset > 0);
				hov.push_back(header_offset);
			}
			assert(!hov.empty());
			term_hov_map::iterator tgt(term_hov.find(term));
			if (tgt != term_hov.end()) {
				tgt->second.insert(tgt->second.end(), hov.begin(), hov.end());
			} else {
				term_hov[term] = hov;
			}
			read<char>(ifs, c);
			assert(c == '\n');
		}
	}
	
	void search(const std::string& term, search_results& results) const
	{
		// make sure the term exists in our in-memory term index
		term_hov_map::const_iterator tgt(term_hov.find(term));
		if (tgt == term_hov.end()) {
			return;
		}
		
		// set up
		assert(ifs_ptr);
		std::ifstream& ifs(*ifs_ptr);
		const header_offset_vector& hov(tgt->second);
		std::cout << term << ": " << hov.size() << " offset(s)" << std::endl;
		typedef header_offset_vector::const_iterator hovcit;
		std::vector<uint32_t> articleids;
		char c(0);
		
		// collect all the articles which contain this term
		// (each article may be represented multiple times)
		for (hovcit it(hov.begin()); it != hov.end(); ++it) {
			
			// each entry represents an offset in the file
			// which begins a sequence of article IDs
			// terminated by UINT32_MAX
			const uint32_t term_offset(*it);
			
			// <uint32_t term ID> <uint32_t article ID> . . . 
			//   <uint32_t UINT32_MAX> '\n'
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
		std::cout << term 
				  << " yielded " << articleids.size() << " articles" 
				  << " including duplicates" << std::endl;
		
		// now aggregate those article IDs into a map of ID to count
		typedef std::map<uint32_t, size_t> aid_count_map;
		aid_count_map acm;
		typedef std::vector<uint32_t>::const_iterator uvcit;
		for (uvcit it(articleids.begin()); it != articleids.end(); ++it) {
			const uint32_t& articleid(*it);
			aid_count_map::iterator tgt(acm.find(articleid));
			if (tgt != acm.end()) {
				tgt->second++;
			} else {
				acm[articleid] = 1;
			}
		}
		
		// and convert that map into search_results
		results.total = acm.size();
		size_t count(0);
		typedef aid_count_map::const_iterator acmcit;
		for (acmcit it(acm.begin());
				it != acm.end() && ++count <= MAX_SEARCH_RESULTS;
					++it) {
			aid_title_map::const_iterator tgt(aid_title.find(it->first));
			assert(tgt != aid_title.end());
			results.top.insert(search_result(tgt->second, it->second));
		}
	}
};

void print_results(const index_repr& idx, const std::string& term)
{
	search_results results;
	idx.search(term, results);
	std::cout << term << ": " << results.total << " hits" << std::endl;
	typedef std::set<search_result>::const_iterator srit;
	for (srit it(results.top.begin()); it != results.top.end(); ++it) {
		std::cout << it->article << " (" << it->weight << ")" << std::endl;
	}
}

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
	print_results(idx, "poetry");
	print_results(idx, "red");
	return 0;
}
