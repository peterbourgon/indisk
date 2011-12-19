#ifndef XMLPARSE_HH_
#define XMLPARSE_HH_

#include <string>
#include <cstdio>
#include <fstream>
#include "index_state.hh"

typedef void (*readfunc)(FILE *, uint32_t, size_t, void *);

class stream
{
public:
	stream(const std::string& filename, size_t bufsz=32768);
	~stream();
	bool read_until(const std::string& tok, readfunc f, void *arg);
	char peek();

private:
	FILE *m_f;
	const size_t m_bufsz;
	bool m_finished;
};

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

void parse_raw(FILE *f, uint32_t offset, size_t len, void *arg);
void parse_contrib(FILE *f, uint32_t offset, size_t len, void *arg);
void parse_text(FILE *f, uint32_t offset, size_t len, void *arg);

#endif
