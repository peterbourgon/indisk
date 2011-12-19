#include <stdexcept>
#include <cassert>
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
		const size_t len(end_offset - start_offset);
		f(m_f, start_offset, len, arg);
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

