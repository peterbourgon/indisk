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

uint32_t index_state::flush(
		std::ofstream& ofs_index,
		uint32_t tid,
		const id_vector& aids) const
{
	const uint32_t offset(ofs_index.tellp());
	ofs_index << tid;
	typedef id_vector::const_iterator idvcit;
	for (idvcit it(aids.begin()); it != aids.end(); ++it) {
		ofs_index << *it;
	}
	ofs_index << '\n';
	return offset;
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
	const uint32_t tid(termid(term)), aid(articleid(article));
	tid_aids_map::iterator tgt(inverted_index.find(tid));
	if (tgt != inverted_index.end()) {
		tgt->second.push_back(aid);
		if (tgt->second.size() >= FLUSH_LIMIT) {
			const uint32_t o(flush(ofs_index, tgt->first, tgt->second));
			register_tid_offset(tid, o);
			tgt->second.clear();
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
	typedef tid_aids_map::const_iterator xit;
	for (xit it(inverted_index.begin()); it != inverted_index.end(); ++it) {
		const uint32_t o(flush(ofs_index, it->first, it->second));
		register_tid_offset(tid, o);
	}
	finalized = true;
}
