#include <stdexcept>
#include <cassert>
#include <iostream>
#include "xmlparse.hh"

stream::stream(const std::string& filename, size_t bufsz)
: m_f(fopen(filename.c_str(), "rb"))
, m_bufsz(bufsz)
, m_finished(false)
{
	if (m_bufsz <= 0 || m_bufsz > (1024*1024)) {
		throw std::runtime_error("bad buffer size");
	}
	if (m_f == NULL) {
		throw std::runtime_error("bad file");
	}
}

stream::~stream()
{
	if (m_f) {
		fclose(m_f);
	}
}

bool stream::read_until(const std::string& tok, readfunc f, void *arg)
{
	char *buf(reinterpret_cast<char *>(malloc(m_bufsz)));
	size_t total_readbytes(0), total_seekbytes(0);
	size_t t(0), tmax(tok.size());
	uint32_t start_offset(ftell(m_f));
	
	bool found(false), last_read(false);
	while (!found && !last_read) {
		const size_t seekbytes(fread(buf, 1, m_bufsz, m_f));
		last_read = seekbytes < m_bufsz;
		size_t i(0);
		for ( ; i < seekbytes && !found; ++i) {
			if (buf[i] == tok[t]) {
				++t;
			} else {
				t = 0;
			}
			if (t == tmax) {
				found = true;
			}
		}
		total_readbytes += i;
		total_seekbytes += seekbytes;
	}
	free(buf);
	if (!found) {
		m_finished = last_read;
		return false;
	}
	
	const long int rewind_bytes(total_readbytes - total_seekbytes);
	assert(rewind_bytes <= 0);
	fseek(m_f, rewind_bytes, SEEK_CUR);

	if (f) {
		const uint32_t end_offset(ftell(m_f));
		const size_t len(end_offset - start_offset - tmax);
		f(m_f, start_offset, len, arg);
		fseek(m_f, tmax, SEEK_CUR);
		assert(ftell(m_f) == end_offset);
	}
	
	return true;
}

char stream::peek()
{
	if (m_finished) {
		return 0;
	}
	const char c(fgetc(m_f));
	fseek(m_f, -1, SEEK_CUR);
	return c;
}

void parse_raw(FILE *f, uint32_t offset, size_t len, void *arg)
{
	std::string *s(reinterpret_cast<std::string *>(arg));
	if (!s) {
		return;
	}
	if (fseek(f, offset, SEEK_SET) != 0) {
		throw std::runtime_error("parse_raw failed during fseek");
	}
	char *buf(static_cast<char *>(malloc(len)));
	if (fread(buf, 1, len, f) != len) {
		throw std::runtime_error("parse_raw read not enough bytes");
	}
	*s = std::string(buf, len);
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

#define MAX_CONTRIB_SIZE (1024 * 1024) // 1MB
void parse_contrib(FILE *f, uint32_t offset, size_t len, void *arg)
{
	std::string *s(reinterpret_cast<std::string *>(arg));
	if (!s) {
		return;
	}
	if (len > MAX_CONTRIB_SIZE) {
		throw std::runtime_error("parse_contrib buffer too big");
	}
	if (fseek(f, offset, SEEK_SET) != 0) {
		throw std::runtime_error("parse_text seek failed");
	}
	char *buf(static_cast<char *>(malloc(len)));
	if (fread(buf, 1, len, f) != len) {
		throw std::runtime_error("parse_text read too short");
	}
	size_t i(0);
	read_until(buf, i, len, "<username>");
	if (i < len) {
		const size_t from(i);
		read_until(buf, i, len, "</username>");
		if (i < len) {
			*s = std::string(buf+from, i-from-11);
		}
	}
	free(buf);
}

static bool add_to(char c, std::string& term)
{
	switch (c) {
	// simply elide these characters totally
	case ',': case ';': case '"': case '=': case '\'':
	case '%': case '!': case '(': case ')': case '*':
	case '^': case '$': case '~': case '`':
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
void parse_text(FILE *f, uint32_t offset, size_t len, void *arg)
{
	parse_text_context *ctx(reinterpret_cast<parse_text_context *>(arg));
	if (!ctx) {
		return;
	}
	if (len > MAX_TEXT_SIZE) {
		throw std::runtime_error("parse_text buffer too big");
	}
	if (fseek(f, offset, SEEK_SET) != 0) {
		throw std::runtime_error("parse_text seek failed");
	}
	char *buf(static_cast<char *>(malloc(len)));
	if (fread(buf, 1, len, f) != len) {
		throw std::runtime_error("parse_text read too short");
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
				if (c == '|' || c == ' ' || c == ':' || c == '.') {
					term.clear();
					break;
				}
			}
			term_complete = add_to(c, term);
			break;
		}
		if (term_complete) {
			if (term_passes(term)) {
				ctx->is.index(term, ctx->article, ctx->ofs_index);
			}
			term.clear();
			term_complete = false;
		}
	}
	free(buf);
}
