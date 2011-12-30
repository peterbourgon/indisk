#ifndef XMLPARSE_HH_
#define XMLPARSE_HH_

#include <string>
#include <cstdio>
#include <fstream>
#include "index_state.hh"

#define DEFAULT_READ_BUFFER_SIZE (256)

typedef void (*readfunc)(FILE *, uint32_t, size_t, void *);
typedef void (*readfunc2)(std::ifstream&, uint64_t, uint64_t, void *);

class stream
{
public:
	stream(
			const std::string& filename,
			size_t from_pos=0,
			size_t to_pos=0,
			size_t bufsz=DEFAULT_READ_BUFFER_SIZE);
	~stream();
	bool read_until(const std::string& tok, readfunc f, void *arg);
	char peek();
	size_t size();
	void seek(uint32_t pos);
	size_t tell();

private:
	FILE *m_f;
	const size_t m_from_pos;
	const size_t m_to_pos;
	const size_t m_bufsz;
	bool m_finished;
};

class stream2
{
public:
	stream2(
			const std::string& filename,
			uint64_t from,
			uint64_t to);
	~stream2();
	bool read_until(const std::string& tok, readfunc2 f, void *arg);
	
private:
	std::ifstream m_f;
	uint64_t m_from;
	uint64_t m_to;
	bool m_finished;
};

struct parse_text_context {
	parse_text_context(
			const std::string& article,
			index_state& is)
	: article(article)
	, is(is)
	{
		//
	}
	
	const std::string& article;
	index_state& is;
};

void parse_title(FILE *f, uint32_t offset, size_t len, void *arg);
void parse_contrib(FILE *f, uint32_t offset, size_t len, void *arg);
void parse_text(FILE *f, uint32_t offset, size_t len, void *arg);

#endif
