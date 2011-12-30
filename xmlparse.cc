#include <stdexcept>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include "xmlparse.hh"

stream::stream(const std::string& filename, uint64_t from, uint64_t to)
: m_f(filename.c_str(), std::ios::in & std::ios::binary)
, m_from(from)
, m_to(to)
, m_finished(false)
{
	if (!m_f.good()) {
		throw std::runtime_error("bad input file");
	}
	if (from > 0) {
		m_f.seekg(from);
	}
	if (m_to <= 0) {
		m_to = size();
	}
	assert(m_f.good());
}

stream::~stream()
{
	m_f.close();
}

bool stream::read_until(const std::string& tok, bool consume, readfunc f, void *arg)
{
	const size_t tok_sz(tok.size());
	std::ifstream::pos_type start_pos(m_f.tellg()), end_pos(m_f.tellg());
	bool found(false);
	std::string line;
	while (!found && !m_finished) {
		std::getline(m_f, line);
		std::string::size_type loc(line.find(tok));
		if (loc != std::string::npos) {
			int backup( -(line.size() - loc + 1) );
			m_f.seekg(backup, m_f.cur);
			assert(read(tok_sz) == tok);
			found = true;
		}
		m_finished = static_cast<uint64_t>(m_f.tellg()) >= m_to;
	}
	if (!found) {
		return false;
	}
	if (consume) {
		m_f.seekg(tok_sz, m_f.cur);
	}
	end_pos = m_f.tellg();
	if (end_pos == start_pos) {
		// found immediately; no data to push anywhere
		return true;
	}
	if (f) {
		m_f.seekg(start_pos);
		assert(m_f.good());
		const size_t len(end_pos - start_pos);
		char *buf(reinterpret_cast<char *>(malloc(len)));
		m_f.read(buf, len);
		assert(m_f.good());
		assert(m_f.tellg() == end_pos);
		f(buf, len, arg);
		free(buf);
	}
	return true;
}

uint64_t stream::size()
{
	std::ifstream::pos_type start_pos(m_f.tellg());
	m_f.seekg(0, std::ios::end);
	std::ifstream::pos_type end_pos(m_f.tellg());
	m_f.seekg(start_pos);
	return end_pos;
}

static void read_until(const char *buf, size_t& i, size_t len, char c)
{
	for ( ; i < len && buf[i] != c; ++i);
}

static void read_until(
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
		read_until(buf, i, len, t0x);
		return true;
	}
	if (i+t1.size() <= len && strncmp(buf+i, t1.c_str(), t1.size()) == 0) {
		read_until(buf, i, len, t1x);
		return true;
	}
	return false;
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
	read_until(buf, i, len, USERNAME_BEGIN);
	if (i < len) {
		const size_t from(i);
		read_until(buf, i, len, USERNAME_END);
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

#define MAX_TEXT_SIZE (1024*1024*100) // 100 MB
#define TERM_RESERVE  64
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
			read_until(buf, i, len, ';');
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
				ctx->is.index(term, ctx->article);
			}
			term.clear();
			term_complete = false;
		}
	}
}
