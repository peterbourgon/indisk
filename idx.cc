#include <cassert>
#include <sstream>
#include <stdexcept>
#include "idx.hh"

template<typename T>
static void write(std::ofstream& ofs, const T& t)
{
	ofs.write(
		const_cast<char *>(
			reinterpret_cast<const char *>(&t)
		),
		sizeof(T)
	);
}

static void merge(const std::string& idx_fn, const std::string& hdr_fn)
{
	// :(
	{
		std::ostringstream oss;
		oss << "cat " << idx_fn << " >> " << hdr_fn;
		system(oss.str().c_str());
	}
	{
		std::ostringstream oss;
		oss << "mv " << hdr_fn << " " << idx_fn;
		system(oss.str().c_str());
	}
}

index_st::index_st(const std::string& basename)
: m_basename(basename)
, m_flush_count(0)
, m_aid(1)
, m_tid(1)
, m_ofs_idx(NULL)
, m_ofs_hdr(NULL)
{
	recreate_output_files();
	assert(m_ofs_idx && m_ofs_hdr);
	assert(m_ofs_idx->good());
	assert(m_ofs_hdr->good());
}

void index_st::index(const std::string& term, const std::string& article)
{
	// TODO optimize for one lock = one article + all terms
	scoped_lock sync(monitor_mutex);
	assert(!term.empty() && !article.empty());
	const uint32_t tid(term_id(term)), aid(article_id(article));
	tid_aids_map::iterator tgt(m_inverted_index.find(tid));
	if (tgt != m_inverted_index.end()) {
		tgt->second.push_back(aid);
		if (tgt->second.size() >= PARTIAL_FLUSH_LIMIT) {
			partial_flush(tgt->first, tgt->second);
		}
	} else {
		id_vector v;
		v.reserve(PARTIAL_FLUSH_LIMIT);
		v.push_back(aid);
		m_inverted_index.insert(std::make_pair(tid, v));
	}
}

void index_st::flush()
{
	scoped_lock sync(monitor_mutex);
	assert(m_ofs_idx->good());
	assert(m_ofs_hdr->good());
	// first, partial_flush any remaining mappings to the index file
	typedef tid_aids_map::iterator tait;
	for (tait it(m_inverted_index.begin()); it != m_inverted_index.end(); ++it) {
		partial_flush(it->first, it->second);
	}
	// then, dump everything out to disk, and reset our state
	write_header();
	m_ofs_idx->close();
	m_ofs_hdr->close();
	merge(idx_filename(), hdr_filename());
	reset_state();
	m_flush_count++;
	recreate_output_files();
}

size_t index_st::article_count() const
{
	scoped_lock sync(monitor_mutex);
	return m_articles.size();
}

static uint32_t generic_id(const std::string& s, str_id_map& m, uint32_t& id)
{
	str_id_map::const_iterator it(m.find(s));
	if (it == m.end()) {
		const uint32_t my_id(id++);
		if (my_id == UINT32_MAX) {
			abort();
		}
		m.insert(std::make_pair(s, my_id));
		return my_id;
	} else {
		return it->second;
	}
}

uint32_t index_st::article_id(const std::string& s)
{
	return generic_id(s, m_articles, m_aid);
}

uint32_t index_st::term_id(const std::string& s)
{
	return generic_id(s, m_terms, m_tid);
}

void index_st::partial_flush(uint32_t tid, id_vector& aids)
{
	// Format of the index line is
	// <uint32 term ID> <uint32 article ID> . . . <uint32 MAX> '\n'
	assert(m_ofs_idx && m_ofs_idx->good());
	std::ofstream& idx(*m_ofs_idx);
	typedef std::ofstream::pos_type stream_pos;
	stream_pos pos(idx.tellp());
	write<uint32_t>(idx, tid);
	typedef id_vector::const_iterator idvcit;
	for (idvcit it(aids.begin()); it != aids.end(); ++it) {
		assert(*it > 0 && *it < UINT32_MAX);
		write<uint32_t>(idx, *it);
	}
	const uint32_t max(UINT32_MAX);
	write<uint32_t>(idx, max);
	write<char>(idx, '\n');
	register_tid_offset(tid, pos);
	aids.clear();
}

void index_st::register_tid_offset(uint32_t tid, uint32_t offset)
{
	assert(tid > 0); // offset can be 0
	tid_offsets_map::iterator tgt(m_tid_offsets.find(tid));
	if (tgt != m_tid_offsets.end()) {
		tgt->second.push_back(offset);
	} else {
		offset_vector v;
		v.push_back(offset);
		m_tid_offsets.insert(std::make_pair(tid, v));
	}
}

void index_st::recreate_output_files()
{
	if (m_ofs_idx) {
		delete m_ofs_idx;
	}
	m_ofs_idx = new std::ofstream(idx_filename().c_str(), std::ios::out & std::ios::binary);
	if (!m_ofs_idx || !m_ofs_idx->good()) {
		throw std::runtime_error("failed to create output index file");
	}
	if (m_ofs_hdr) {
		delete m_ofs_hdr;
	}
	m_ofs_hdr = new std::ofstream(hdr_filename().c_str(), std::ios::out & std::ios::binary);
	if (!m_ofs_hdr || !m_ofs_hdr->good()) {
		throw std::runtime_error("failed to create output index header file");
	}
}

std::string index_st::idx_filename()
{
	std::ostringstream oss;
	oss << m_basename << '.' << m_flush_count;
	return oss.str();
}

std::string index_st::hdr_filename()
{
	return std::string(idx_filename() + ".hdr");
}

void index_st::write_header()
{
	// <uint32 offset where header ends and inverted index begins> '\n'
	// <uint32 number of articles> '\n'
	// <uint32 article ID> <article name as text> '\n'
	//  . . .
	// <uint32 number of terms> '\n'
	// <uint32 term ID> <term as text> END_DELIM <uint32 offset 1> ... 
	//    <uint32 UINT32_MAX> '\n'
	//  . . .
	
	// The term offsets in the header is a complete list of all offsets within
	// the "inverted index" portion of the index file which represent a
	// sequence of uint32_t article IDs (terminated by a '\n') in which that
	// term appears. Each such offset begins with a uint32_t representing the
	// term ID for all uint32_t article IDs that follow.
	
	typedef str_id_map::const_iterator sidcit;
	typedef offset_vector::const_iterator ovcit;
	typedef tid_offsets_map::const_iterator tocit;
	uint32_t asz(m_articles.size()), tsz(m_terms.size()), offset(0);
	assert(m_ofs_hdr && m_ofs_hdr->good());
	std::ofstream& hdr(*m_ofs_hdr);
	
	// write initial offset position (will put correct value later)
	write<uint32_t>(hdr, offset);
	write<char>(hdr, '\n');
	
	// write article block
	write<uint32_t>(hdr, asz);
	write<char>(hdr, '\n');
	for (sidcit it(m_articles.begin()); it != m_articles.end(); ++it) {
		assert(it->second > 0);
		write<uint32_t>(hdr, it->second);
		assert(!it->first.empty());
		hdr << it->first << '\n';
	}
	
	// write term block
	write<uint32_t>(hdr, tsz);
	write<char>(hdr, '\n');
	uint32_t max(UINT32_MAX);
	for (sidcit it(m_terms.begin()); it != m_terms.end(); ++it) {
		assert(it->second > 0);
		write<uint32_t>(hdr, it->second);
		assert(!it->first.empty());
		assert(it->first.find(END_DELIM) == std::string::npos);
		hdr << it->first << END_DELIM;
		tocit tgt(m_tid_offsets.find(it->second));
		assert(tgt != m_tid_offsets.end());
		const offset_vector& offsets(tgt->second);
		assert(!offsets.empty());
		for (ovcit it2(offsets.begin()); it2 != offsets.end(); ++it2) {
			// this offset can be 0
			assert(*it2 < max);
			write<uint32_t>(hdr, *it2);
		}
		write<uint32_t>(hdr, max);
		write<char>(hdr, '\n');
	}
	
	// back-fill the offset position
	offset = hdr.tellp();
	assert(offset > 0);
	hdr.seekp(0);
	write<uint32_t>(hdr, offset);
	hdr.seekp(offset);
}

void index_st::reset_state()
{
	m_aid = 1;
	m_tid = 1;
	m_articles.clear();
	m_terms.clear();
	m_inverted_index.clear();
	m_tid_offsets.clear();
}

