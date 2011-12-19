#include <iostream>
#include <stdexcept>
#include "index_state.hh"

index_state::index_state()
: finalized(false)
, aid(1)
, tid(1)
{
	//
}
	
uint32_t index_state::termid(const std::string& s)
{
	str_id_map::const_iterator it(terms.find(s));
	if (it == terms.end()) {
		const uint32_t id(tid++);
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
	const uint32_t offset(ofs_index.tellp());
	write<uint32_t>(ofs_index, offset);
	typedef id_vector::const_iterator idvcit;
	for (idvcit it(aids.begin()); it != aids.end(); ++it) {
		write<uint32_t>(ofs_index, *it);
	}
	ofs_index << '\n';
	register_tid_offset(tid, offset);
	aids.clear();
}

void index_state::register_tid_offset(uint32_t tid, uint32_t offset)
{
	tid_offsets_map::iterator tgt(tid_offsets.find(tid));
	if (tgt != tid_offsets.end()) {
		tgt->second.push_back(offset);
	} else {
		offset_vector v;
		v.push_back(offset);
		tid_offsets.insert(std::make_pair(tid, v));
	}
}
	
void index_state::index(
		const std::string& term,
		const std::string& article,
		std::ofstream& ofs_index)
{
	if (finalized) {
		throw std::runtime_error("can't index after finalize");
	}
	std::cout << "index: " << term << " -> " << article << std::endl;
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

void index_state::finalize(std::ofstream& ofs_index)
{
	typedef tid_aids_map::iterator xit;
	for (xit it(inverted_index.begin()); it != inverted_index.end(); ++it) {
		flush(ofs_index, it->first, it->second);
	}
	finalized = true;
}

void index_state::write_header(std::ofstream& ofs_header)
{
	if (!finalized) {
		throw std::runtime_error("can't write index header without finalize");
	}
	
	// <uint32_t offset where header ends and inverted index begins> '\n'
	// <uint32_t number of articles> '\n'
	// <uint32_t article ID> <article name as text> '\n'
	//  . . .
	// <uint32_t number of terms>\n
	// <uint32_t term ID> <term as text> '|' <uint32_t offset 1> ... '\n'
	//  . . .
	
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
		write<uint32_t>(ofs_header, it->second);
		ofs_header << it->first << '\n';
	}
	
	// write term block
	write<uint32_t>(ofs_header, tsz);
	ofs_header << '\n';
	for (sidcit it(terms.begin()); it != terms.end(); ++it) {
		write<uint32_t>(ofs_header, it->second);
		ofs_header << it->first << '|';
		tocit tgt(tid_offsets.find(it->second));
		if (tgt == tid_offsets.end()) {
			std::cerr << "term " 
					<< it->second << " " << it->first 
					<< " not present" << std::endl;
			throw std::runtime_error("invalid state in index_state");
		}
		const index_state::offset_vector& offsets(tgt->second);
		for (ovcit it2(offsets.begin()); it2 != offsets.end(); ++it2) {
			write<uint32_t>(ofs_header, *it2);
		}
		ofs_header << '\n';
	}
	
	// back-fill the offset position
	offset = ofs_header.tellp();
	ofs_header.seekp(0);
	write<uint32_t>(ofs_header, offset);
	ofs_header.seekp(offset);
	std::cout << "wrote offset of " << offset << std::endl;
}
