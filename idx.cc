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

void index_st::flush(bool last_flush)
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
	if (!last_flush) {
		recreate_output_files();
	}
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
	oss << m_basename << '.' << m_flush_count+1;
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
	// the "inverted index" portion of the index file, which represent a
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

idx_thread::idx_thread(
		const std::string& xml_filename,
		const region& r,
		const std::string& idx_filename)
: m_s(xml_filename, r)
, m_idx_st(idx_filename)
, m_article_count(0)
{
	//
}

idx_thread::~idx_thread()
{
	//
}

void idx_thread::run()
{
	scoped_lock sync(monitor_mutex);
	size_t unflushed_article_count(0);
	synchronized_thread_running = true;
	while (synchronized_thread_running) {
		sync.unlock();
		index_result r(index_article(m_s, m_idx_st));
		if (r == END_OF_REGION) {
			m_idx_st.flush(true);
			sync.lock();
			break;
		} else if (r == INDEX_GOOD) {
			++m_article_count; // TODO atomic
			if (++unflushed_article_count >= ARTICLE_FLUSH_LIMIT) {
				m_idx_st.flush();
				unflushed_article_count = 0;
			}
		}
		sync.lock();
	}
	synchronized_thread_running = false;
}

bool idx_thread::finished() const
{
	scoped_lock sync(monitor_mutex);
	return !synchronized_thread_running;
}

size_t idx_thread::article_count() const
{
	return m_article_count;
}

//
//
//

static void buf_read_until(const char *buf, size_t& i, size_t len, char c)
{
	for ( ; i < len && buf[i] != c; ++i);
}

static void buf_read_until(
		const char *buf,
		size_t& i,
		size_t len,
		const std::string& tok)
{
	size_t t(0), tmax(tok.size());
	bool found(false);
	while (!found && i < len) {
		if (buf[i++] == tok[t]) {
			++t;
		} else {
			t = 0;
		}
		if (t == tmax) {
			found = true;
		}
	}
}

#define MAX_TITLE_SIZE (1024) // 1KB
void parse_title(char *buf, size_t len, void *arg)
{
	std::string *s(reinterpret_cast<std::string *>(arg));
	if (!s) {
		return;
	}
	if (len > MAX_TITLE_SIZE) {
		throw std::runtime_error("parse_title buffer too big");
	}
	std::string title;
	title.reserve(len);
	for (size_t i(0); i < len; i++) {
		const char& c(buf[i]);
		switch (c) {
		case END_DELIM:
			continue;
		}
		title += c;
	}
	title.swap(*s);
}

#define MAX_CONTRIB_SIZE (1024 * 1024) // 1MB
void parse_contrib(char *buf, size_t len, void *arg)
{
	std::string *s(reinterpret_cast<std::string *>(arg));
	if (!s) {
		return;
	}
	if (len > MAX_CONTRIB_SIZE) {
		throw std::runtime_error("parse_contrib buffer too big");
	}
	static const std::string USERNAME_BEGIN("<username>");
	static const std::string USERNAME_END("</username>");
	static const size_t USERNAME_END_SZ(USERNAME_END.size());
	size_t i(0);
	buf_read_until(buf, i, len, USERNAME_BEGIN);
	if (i < len) {
		const size_t from(i);
		buf_read_until(buf, i, len, USERNAME_END);
		if (i < len) {
			*s = std::string(buf+from, i-from-USERNAME_END_SZ);
		}
	}
}

static bool add_to(char c, std::string& term)
{
	switch (c) {
	// simply elide these characters totally
	case END_DELIM:
	case ',': case ';': case '"': case '=': case '\'':
	case '%': case '!': case '(': case ')': case '*':
	case '^': case '$': case '~': case '`': case '#':
		return false;
	
	// these are end-of-term signifiers
	case ' ': case '\t': case '\r': case '\n':
		return true;
	
	// these get converted to spaces and are consequently end-of-term
	case ':': case '.':
		return true;
	
	// otherwise, just add to the term
	default:
		term += tolower(c);
		return false;
	}
}

static bool term_passes(const std::string& term)
{
	if (term.empty()) {
		return false;
	}
	if (term.size() <= 2) {
		return false;
	}
	// http://scottbryce.com/cryptograms/stats.htm
	if (term.size() == 3 && (
			term == "the" || term == "and" || term == "for" ||
			term == "are" || term == "but" || term == "not" ||
			term == "you" || term == "all" || term == "any" ||
			term == "can" || term == "had" || term == "her" ||
			term == "was" || term == "one" || term == "our" ||
			term == "out" || term == "day" || term == "get" ||
			term == "has" || term == "him" || term == "his" ||
			term == "how" || term == "man" || term == "new" ||
			term == "now" || term == "old" || term == "see" ||
			term == "two" || term == "way" || term == "who" ||
			term == "boy" || term == "did" || term == "its" ||
			term == "let" || term == "put" || term == "say" ||
			term == "she" || term == "too" || term == "use"
		)) {
		return false;
	}
	return true;
}

static void skip_interior(
		const char *buf,
		size_t& i,
		size_t len,
		char begin,
		char end)
{
	if (buf[i] != begin) {
		return;
	}
	int stack(0);
	for ( ; i < len; ++i) {
		const char& c(buf[i]);
		if (c == begin) {
			stack++;
		} else if (c == end) {
			stack--;
		}
		if (stack <= 0) {
			if (i < len) {
				i++;
			}
			break;
		}
	}
}

static bool lookahead(const char *buf, size_t& i, size_t len)
{
	// special cases
	static const std::string t0("&lt;ref"), t0x("&gt;");
	static const std::string t1("&lt;/"), t1x("&gt;");
	if (i+t0.size() <= len && strncmp(buf+i, t0.c_str(), t0.size()) == 0) {
		buf_read_until(buf, i, len, t0x);
		return true;
	}
	if (i+t1.size() <= len && strncmp(buf+i, t1.c_str(), t1.size()) == 0) {
		buf_read_until(buf, i, len, t1x);
		return true;
	}
	return false;
}

struct parse_text_context {
	parse_text_context(
			const std::string& article,
			const std::string& contrib,
			index_st& idx_st)
	: article(article)
	, contrib(contrib)
	, idx_st(idx_st)
	{
		//
	}
	
	const std::string& article;
	const std::string& contrib;
	index_st& idx_st;
};

#define MAX_TEXT_SIZE (1024*1024*100) // 100 MB
#define TERM_RESERVE 64 // a guess at the average term size
void parse_text(char *buf, size_t len, void *arg)
{
	parse_text_context *ctx(reinterpret_cast<parse_text_context *>(arg));
	if (!ctx) {
		return;
	}
	assert(!ctx->article.empty());
	if (len > MAX_TEXT_SIZE) {
		throw std::runtime_error("parse_text buffer too big");
	}
	if (!ctx->contrib.empty()) {
		ctx->idx_st.index(ctx->contrib, ctx->article);
	}
	std::string term;
	term.reserve(TERM_RESERVE);
	int square_stack(0);
	bool term_complete(false);
	for (size_t i(0); i < len; ++i) {
		if (lookahead(buf, i, len)) {
			continue;
		}
		const char& c(buf[i]);
		switch (c) {
		case '{':
			skip_interior(buf, i, len, '{', '}');
			break;
		case '<':
			skip_interior(buf, i, len, '<', '>');
		case '[':
			square_stack++;
			break;
		case ']':
			square_stack--;
			if (square_stack <= 0) {
				square_stack = 0;
			}
			break;
		case '&':
			buf_read_until(buf, i, len, ';');
			break;
		default:
			if (square_stack > 0) {
				// [[abc]]          => abc
				// [[abc|def]]      => def
				// [http://xyz foo] => foo
				// [[abc:def]]      => def
				if (
						c == '|' ||
						c == ' ' ||
						(square_stack > 1 && c == ':') // [[abc:def]]
						) {
					term.clear();
					break;
				}
			}
			term_complete = add_to(c, term) && square_stack <= 0;
			break;
		}
		if (term_complete) {
			if (term_passes(term)) {
				ctx->idx_st.index(term, ctx->article);
			}
			term.clear();
			term_complete = false;
		}
	}
}

index_result index_article(xstream& s, index_st& idx_st)
{
	if (!s.read_until("<title>", true, NULL, NULL)) {
		return END_OF_REGION;
	}
	std::string title;
	if (!s.read_until("<", false, parse_title, &title)) {
		return NO_INDEX_BUT_CONTINUE;
	}
	if (title.empty()) {
		return NO_INDEX_BUT_CONTINUE;
	}
	if (
			title.find("Category:") == 0 ||
			title.find("Wikipedia:") == 0 ||
			title.find("Special:") == 0) {
		// Don't index special pages
		return NO_INDEX_BUT_CONTINUE;
	}
	assert(title.find(END_DELIM) == std::string::npos);
	if (!s.read_until("<contributor>", true, NULL, NULL)) {
		return NO_INDEX_BUT_CONTINUE;
	}
	std::string contrib;
	if (!s.read_until("</contributor>", false, parse_contrib, &contrib)) {
		return NO_INDEX_BUT_CONTINUE;
	}
	assert(contrib.find(END_DELIM) == std::string::npos);
	if (!s.read_until("<text", true, NULL, NULL)) {
		return NO_INDEX_BUT_CONTINUE;
	}
	if (!s.read_until(">", true, NULL, NULL)) {
		return NO_INDEX_BUT_CONTINUE;
	}
	parse_text_context ctx(title, contrib, idx_st);
	if (!s.read_until("</text", false, parse_text, &ctx)) {
		return NO_INDEX_BUT_CONTINUE;
	}
	return INDEX_GOOD;
}
