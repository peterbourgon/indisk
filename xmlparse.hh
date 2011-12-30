#ifndef XMLPARSE_HH_
#define XMLPARSE_HH_

#include <string>
#include <cstdio>
#include <fstream>
#include "index_state.hh"

typedef void (*readfunc)(char *buf, size_t len, void *arg);

class stream
{
public:
	stream(const std::string& filename, uint64_t from, uint64_t to);
	~stream();
	bool read_until(const std::string& tok, bool consume, readfunc f, void *arg);
	uint64_t size();
	
	std::ifstream::pos_type tell() { return m_f.tellg(); }
	void seek(const std::ifstream::pos_type& pos) { m_f.seekg(pos); }
	std::string read(size_t n)
	{
		const std::ifstream::pos_type start_pos(tell());
		char *buf(reinterpret_cast<char *>(malloc(n)));
		m_f.read(buf, n);
		std::string s(buf, n);
		free(buf);
		seek(start_pos);
		return s;
	}
	
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

void parse_title(char *buf, size_t len, void *arg);
void parse_contrib(char *buf, size_t len, void *arg);
void parse_text(char *buf, size_t len, void *arg);

#endif
