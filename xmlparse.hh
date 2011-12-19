#ifndef XMLPARSE_HH_
#define XMLPARSE_HH_

#include <string>
#include <cstdio>

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

#endif
