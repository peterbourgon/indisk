#include <iostream>
#include <limits>
#include <stdexcept>
#include <cassert>
#include <cstdlib>
#include "index_state.hh"

static const std::string HEADER_SUFFIX(".hdr");

index_state::index_state(const std::string index_filename)
: index_file(index_filename)
, header_file(index_file + HEADER_SUFFIX)
, ofs_header(header_file.c_str(), std::ios::binary)
, ofs_index(index_file.c_str(), std::ios::binary)
, finalized(false)
, aid(1)
, tid(1)
{
	if (!ofs_header.good()) {
		throw std::runtime_error("index header: bad file");
	}
	if (!ofs_index.good()) {
		throw std::runtime_error("bad index file");
	}
	std::cout << "indexing to " << index_file << " + " << header_file << std::endl;
}

uint32_t index_state::termid(const std::string& s)
{
	str_id_map::const_iterator it(terms.find(s));
	if (it == terms.end()) {
		const uint32_t id(tid++);
		if (tid == UINT32_MAX) {
			abort();
		}
		terms.insert(std::make_pair(s, id));
		return id;
	} else {
		return it->second;
	}
}

uint32_t index_state::articleid(const std::string& s)
{
	str_id_map::const_iterator it(articles.find(s));
	if (it == articles.end()) {
		const uint32_t id(aid++);
		if (aid == UINT32_MAX) {
			abort();
		}
		articles.insert(std::make_pair(s, id));
		return id;
	} else {
		return it->second;
	}
}

template<typename T>
static void write(std::ofstream& ofs, const T& t)
{
	ofs.write(const_cast<char *>(reinterpret_cast<const char *>(&t)),
				sizeof(T));
}

void index_state::flush(
		std::ofstream& ofs_index,
		uint32_t tid,
		id_vector& aids)
{
	// <uint32_t term ID> <uint32_t article ID> . . . <uint32_t UINT32_MAX> '\n'
	const uint32_t offset(ofs_index.tellp());
	write<uint32_t>(ofs_index, tid);
	typedef id_vector::const_iterator idvcit;
	for (idvcit it(aids.begin()); it != aids.end(); ++it) {
		assert(*it > 0);
		write<uint32_t>(ofs_index, *it);
	}
	const uint32_t max(UINT32_MAX);
	write<uint32_t>(ofs_index, max);
	ofs_index << '\n';
	register_tid_offset(tid, offset);
	aids.clear();
}

void index_state::register_tid_offset(uint32_t tid, uint32_t offset)
{
	assert(tid > 0); // offset can be 0
	tid_offsets_map::iterator tgt(tid_offsets.find(tid));
	if (tgt != tid_offsets.end()) {
		tgt->second.push_back(offset);
	} else {
		offset_vector v;
		v.push_back(offset);
		tid_offsets.insert(std::make_pair(tid, v));
	}
}
	
void index_state::index(const std::string& term, const std::string& article)
{
	scoped_lock sync(monitor_mutex);
	assert(!term.empty());
	assert(!article.empty());
	if (finalized) {
		throw std::runtime_error("can't index after finalize");
	}
	//std::cout << "index: " << term << " -> " << article << std::endl;
	const uint32_t tid(termid(term)), aid(articleid(article));
	tid_aids_map::iterator tgt(inverted_index.find(tid));
	if (tgt != inverted_index.end()) {
		tgt->second.push_back(aid);
		if (tgt->second.size() >= FLUSH_LIMIT) {
			flush(ofs_index, tgt->first, tgt->second);
		}
	} else {
		id_vector v;
		v.reserve(FLUSH_LIMIT);
		v.push_back(aid);
		inverted_index.insert(std::make_pair(tid, v));
	}
}

void index_state::finalize()
{
	scoped_lock sync(monitor_mutex);
	typedef tid_aids_map::iterator xit;
	for (xit it(inverted_index.begin()); it != inverted_index.end(); ++it) {
		flush(ofs_index, it->first, it->second);
	}
	finalized = true;
	write_header();
	ofs_header.close();
	ofs_index.close();
}

void index_state::write_header()
{
	if (!finalized) {
		throw std::runtime_error("can't write index header without finalize");
	}
	
	// <uint32_t offset where header ends and inverted index begins> '\n'
	// <uint32_t number of articles> '\n'
	// <uint32_t article ID> <article name as text> '\n'
	//  . . .
	// <uint32_t number of terms>\n
	// <uint32_t term ID> <term as text> END_DELIM <uint32_t offset 1> ... 
	//    <uint32_t UINT32_MAX> '\n'
	//  . . .
	
	// The term offsets in the header is a complete list of all offsets within
	// the "inverted index" portion of the index file which represent a
	// sequence of uint32_t article IDs (terminated by a '\n') in which that
	// term appears. Each such offset begins with a uint32_t representing the
	// term ID for all uint32_t article IDs that follow.

	typedef str_id_map::const_iterator sidcit;
	typedef offset_vector::const_iterator ovcit;
	typedef tid_offsets_map::const_iterator tocit;
	uint32_t asz(articles.size()), tsz(terms.size()), offset(0);
	
	// write initial offset position (will put correct value later)
	write<uint32_t>(ofs_header, offset);
	ofs_header << '\n';
	
	// write article block
	write<uint32_t>(ofs_header, asz);
	ofs_header << '\n';
	for (sidcit it(articles.begin()); it != articles.end(); ++it) {
		assert(it->second > 0);
		write<uint32_t>(ofs_header, it->second);
		assert(!it->first.empty());
		ofs_header << it->first << '\n';
	}
	
	// write term block
	write<uint32_t>(ofs_header, tsz);
	ofs_header << '\n';
	uint32_t max(UINT32_MAX);
	for (sidcit it(terms.begin()); it != terms.end(); ++it) {
		assert(it->second > 0);
		write<uint32_t>(ofs_header, it->second);
		assert(!it->first.empty());
		assert(it->first.find(END_DELIM) == std::string::npos);
		ofs_header << it->first << END_DELIM;
		tocit tgt(tid_offsets.find(it->second));
		assert(tgt != tid_offsets.end());
		const offset_vector& offsets(tgt->second);
		assert(!offsets.empty());
		for (ovcit it2(offsets.begin()); it2 != offsets.end(); ++it2) {
			// this offset can be 0
			assert(*it2 < max);
			write<uint32_t>(ofs_header, *it2);
		}
		write<uint32_t>(ofs_header, max);
		ofs_header << '\n';
	}
	
	// back-fill the offset position
	offset = ofs_header.tellp();
	assert(offset > 0);
	ofs_header.seekp(0);
	write<uint32_t>(ofs_header, offset);
	ofs_header.seekp(offset);
}

size_t index_state::article_count() const
{
	scoped_lock sync(monitor_mutex);
	return articles.size();
}

size_t index_state::term_count() const
{
	scoped_lock sync(monitor_mutex);
	const size_t t1(terms.size());
	const size_t t2(inverted_index.size());
	assert(t1 == t2);
	return t1;
}
