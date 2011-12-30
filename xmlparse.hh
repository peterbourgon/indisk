#ifndef XMLPARSE_HH_
#define XMLPARSE_HH_

#include <string>
#include <cstdio>
#include <fstream>
#include "index_state.hh"

typedef void (*readfunc)(std::ifstream&, uint64_t, uint64_t, void *);

class stream
{
public:
	stream(
			const std::string& filename,
			uint64_t from,
			uint64_t to);
	~stream();
	bool read_until(const std::string& tok, readfunc f, void *arg);
	uint64_t size();
	
	std::ifstream::pos_type tell() { return m_f.tellg(); }
	void seek(const std::ifstream::pos_type& pos) { m_f.seekg(pos); }
	
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

void parse_title(std::ifstream& f, uint64_t offset, uint64_t len, void *arg);
void parse_contrib(std::ifstream& f, uint64_t offset, uint64_t len, void *arg);
void parse_text(std::ifstream& f, uint64_t offset, uint64_t len, void *arg);

#endif
