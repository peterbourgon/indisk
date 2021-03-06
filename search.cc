#include <string>
#include <fstream>
#include <stdexcept>
#include <cassert>
#include <sstream>
#include <map>
#include "search.hh"

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
		assert(ifs.good());
		
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
	
	search_results search(const std::string& term) const
	{
		// make sure the term exists in our in-memory term index
		term_hov_map::const_iterator tgt(term_hov.find(term));
		if (tgt == term_hov.end()) {
			return search_results();
		}
		
		// set up
		assert(ifs_ptr);
		std::ifstream& ifs(*ifs_ptr);
		assert(ifs.good());
		const header_offset_vector& hov(tgt->second);
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
		search_results results;
		results.total = acm.size();
		size_t count(0);
		typedef aid_count_map::const_iterator acmcit;
		for (acmcit it(acm.begin());
				it != acm.end() && ++count <= MAX_SEARCH_RESULTS;
					++it) {
			aid_title_map::const_iterator tgt(aid_title.find(it->first));
			assert(tgt != aid_title.end());
			results.top.push_back(search_result(tgt->second, it->second));
		}
		return results;
	}
};

static void merge(search_results& dst, const search_results& src)
{
	// naive algorithm
	typedef std::vector<search_result>::const_iterator srcit;
	typedef std::vector<search_result>::iterator srit;
	dst.total += src.total;
	for (srcit it(src.top.begin()); it != src.top.end(); ++it) {
		bool found(false);
		for (srit it2(dst.top.begin()); it2 != dst.top.end(); ++it2) {
			if (it2->article == it->article) {
				it2->weight += it->weight;
				found = true;
				break;
			}
		}
		if (!found) {
			dst.top.push_back(*it);
		}
	}
}

static search_results search(
		const std::vector<index_repr *>& indices,
		const std::string& term)
{
	// get
	std::vector<search_results> intermediate;
	typedef std::vector<index_repr *>::const_iterator ircit;
	for (ircit it(indices.begin()); it != indices.end(); ++it) {
		intermediate.push_back((*it)->search(term));
	}
	// merge
	search_results final;
	typedef std::vector<search_results>::const_iterator srscit;
	for (srscit it(intermediate.begin()); it != intermediate.end(); ++it) {
		merge(final, *it);
	}
	// sort and cut
	final.sort();
	if (final.top.size() > MAX_SEARCH_RESULTS) {
		final.top.erase(final.top.begin() + MAX_SEARCH_RESULTS, final.top.end());
	}
	return final;
}

std::vector<index_repr *> INDICES;

size_t init_indices(const std::vector<std::string>& filenames)
{
	typedef std::vector<index_repr *>::iterator irit;
	for (irit it(INDICES.begin()); it != INDICES.end(); ++it) {
		delete *it;
	}
	INDICES.clear();
	size_t count(0);
	typedef std::vector<std::string>::const_iterator svcit;
	for (svcit it(filenames.begin()); it != filenames.end(); ++it) {
		try {
			index_repr *idx(new index_repr(*it));
			idx->parse();
			INDICES.push_back(idx);
			count++;
		} catch (const std::runtime_error& ex) {
			continue;
		}
	}
	return count;
}

search_results search_indices(const std::string& term)
{
	return search(INDICES, term);
}

