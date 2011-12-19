#include <iostream>
#include <fstream>
#include <stdexcept>
#include <ext/hash_map>
#include "xmlparse.hh"

namespace __gnu_cxx
{
	template<> struct hash<std::string>
	{
		size_t operator()(const std::string& s) const
		{
			return hash<const char *>()(s.c_str());
		}
	};
}

#define FLUSH_LIMIT 64
struct index_state
{
public:
	index_state()
	: finalized(false)
	, aid(1)
	, tid(1)
	{
		
	}
	
public:
	typedef std::vector<uint32_t> id_vector;
	typedef std::vector<uint32_t> offset_vector;
	typedef __gnu_cxx::hash_map<std::string, uint32_t> str_id_map;
	typedef __gnu_cxx::hash_map<uint32_t, id_vector> tid_aids_map;
	typedef __gnu_cxx::hash_map<uint32_t, offset_vector> tid_offsets_map;
	
	str_id_map articles;
	str_id_map terms;
	tid_aids_map inverted_index;
	tid_offsets_map tid_offsets;
	
	bool finalized;
	
private:
	uint32_t aid;
	uint32_t tid;
	
	uint32_t termid(const std::string& s)
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
	
	uint32_t articleid(const std::string& s)
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

	// Dump the term ID + a vector of article IDs to the current position
	// in the passed output file stream, and returns that position as an
	// offset.
	uint32_t flush(
			std::ofstream& ofs_index,
			uint32_t tid,
			const id_vector& aids)
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
	
	// Register an offset (from flush(), above) with a term ID, to be later
	// used while writing the index header.
	void register_tid_offset(uint32_t tid, uint32_t offset)
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
	
public:
	
	// Associate the given article to the given term.
	// If FLUSH_LIMIT articles have been associated with the term,
	// flush that term ID + article ID list to the given file stream,
	// and register the resultant offset with the term ID, for the header.
	void index(
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
	
	// Flush all remaining term ID + article ID vectors
	// to the given file stream.
	void finalize(std::ofstream& ofs_index)
	{
		typedef tid_aids_map::const_iterator xit;
		for (xit it(inverted_index.begin()); it != inverted_index.end(); ++it) {
			const uint32_t o(flush(ofs_index, it->first, it->second));
			register_tid_offset(tid, o);
		}
		finalized = true;
	}
};

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

void nothing(FILE *f, uint32_t offset, size_t len, void *arg) { }

void parse_raw(FILE *f, uint32_t offset, size_t len, void *arg)
{
	std::string *s(reinterpret_cast<std::string *>(arg));
	if (!s) {
		return;
	}
	if (fseek(f, offset, SEEK_SET) != 0) {
		throw std::runtime_error("parse_raw failed during fseek");
	}
	s->reserve(len);
	if (fread(const_cast<char *>(s->data()), 1, len, f) != len) {
		throw std::runtime_error("parse_raw read not enough bytes");
	}
}

void parse_contrib(FILE *f, uint32_t offset, size_t len, void *arg)
{
	parse_raw(f, offset, len, arg); // TODO get name if it exists
}

struct parse_text_context {
	parse_text_context(
			const std::string& article,
			index_state& is,
			std::ofstream& ofs_index)
	: article(article)
	, is(is)
	, ofs_index(ofs_index)
	{
		//
	}
	
	const std::string& article;
	index_state& is;
	std::ofstream& ofs_index;
};

void parse_text(FILE *f, uint32_t offset, size_t len, void *arg)
{
	parse_text_context *ctx(reinterpret_cast<parse_text_context *>(arg));
	if (!ctx) {
		return;
	}
	// TODO get every token
	const std::string term; // TODO
	ctx->is.index(term, ctx->article, ctx->ofs_index);
}

bool index_article(
		stream& s,
		index_state& is,
		std::ofstream& ofs_index)
{
	if (!s.read_until("<title>", nothing, NULL)) {
		return false;
	}
	std::string title;
	if (!s.read_until("<", parse_raw, &title)) {
		return false;
	}
	if (!s.read_until("<contributor>", nothing, NULL)) {
		return false;
	}
	std::string contrib;
	if (!s.read_until("</contributor>", parse_contrib, &contrib)) {
		return false;
	}
	if (!s.read_until("<text", nothing, NULL)) {
		return false;
	}
	if (!s.read_until(">", nothing, NULL)) {
		return false;
	}
	parse_text_context ctx(title, is, ofs_index);
	if (!s.read_until("</text", parse_text, &ctx)) {
		return false;
	}
	return true;
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
	return 0;
}
