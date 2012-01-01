#ifndef XML_HH_
#define XML_HH_

#include <fstream>
#include <vector>

// Regions define areas of an input file
// which are yielded by a stream.

typedef std::ifstream::pos_type stream_pos;

struct region
{
	region(stream_pos b, stream_pos e) : begin(b), end(e) { }
	
	stream_pos begin;
	stream_pos end;
};

std::vector<region> regionize(const std::string& filename, size_t count);

// A stream represents a region of a file, and
// provides the API we need to efficiently parse
// large XML.

typedef void (*rfunc)(char *, size_t, void *);

class xstream
{
public:
	xstream(const std::string& filename, const region& r);
	~xstream();
	
	bool read_until(const std::string& tok, bool consume, rfunc f, void *arg);
	
	bool seek(const stream_pos& pos);
	stream_pos tell();
	stream_pos size();
	
	// introspection methods for assertions and debug
	std::string read(size_t n);
	
private:
	std::ifstream *m_fptr;
	region m_region;
	bool m_finished;
};

#endif
