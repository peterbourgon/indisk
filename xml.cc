#include <stdexcept>
#include <cassert>
#include <cstdlib>
#include "xml.hh"

#define MAX_REGIONS 64
static const std::string REGION_TOKEN("<title>");
std::vector<region> regionize(const std::string& filename, size_t count)
{
	if (count == 0 || count > MAX_REGIONS) {
		throw std::runtime_error("regionize count out of range");
	}
	std::vector<region> regions;
	region full_region(0, 0);
	xstream s(filename, full_region);
	stream_pos sz(s.size());
	stream_pos last_end(s.tell());
	for (size_t i(0); i < count-1; ++i) {
		// look for the next region token
		if (!s.read_until(REGION_TOKEN, false, NULL, NULL)) {
			throw std::runtime_error("too many regions for file");
		}
		assert(s.read(REGION_TOKEN.size()) == REGION_TOKEN);
		// we will make one region from here
		// move to where we think the next region token may be
		stream_pos end_pos( (sz/count) * (i+1) );
		s.seek(end_pos);
		// look for the next region token
		if (!s.read_until(REGION_TOKEN, false, NULL, NULL)) {
			throw std::runtime_error("too many regions for file");
		}
		assert(s.read(REGION_TOKEN.size()) == REGION_TOKEN);
		// we will make the region end here
		end_pos = s.tell();
		regions.push_back(region(last_end, end_pos));
		// ready for next run
		last_end = end_pos;
	}
	// make sure we get all the offsets
	regions.push_back(region(last_end, s.size()));
	return regions;
}

xstream::xstream(const std::string& filename, const region& r)
: m_fptr(new std::ifstream(filename.c_str(), std::ios::in & std::ios::binary))
, m_region(r)
, m_finished(false)
{
	if (!m_fptr->good()) {
		throw std::runtime_error("bad input file");
	}
	if (m_region.begin > 0) {
		seek(m_region.begin);
	}
	if (m_region.end == 0) {
		m_region.end = size();
	}
}

xstream::~xstream()
{
	if (m_fptr) {
		m_fptr->close();
		delete m_fptr;
	}
}

bool xstream::read_until(const std::string& tok, bool consume, rfunc rf, void *arg)
{
	assert(m_fptr);
	std::ifstream& f(*m_fptr);
	const size_t tok_sz(tok.size());
	stream_pos start_pos(tell()), end_pos(tell());
	bool found(false);
	std::string line;
	while (!found && !m_finished) {
		std::getline(f, line);
		std::string::size_type loc(line.find(tok));
		if (loc != std::string::npos) {
			int backup( -(line.size() - loc + 1) );
			f.seekg(backup, std::ifstream::cur);
			assert(read(tok_sz) == tok);
			if (tell() < m_region.end) {
				found = true;
			}
		}
		m_finished = tell() >= m_region.end;
	}
	if (m_finished && tell() > m_region.end) {
		int backup( m_region.end - tell() );
		f.seekg(backup, std::ifstream::cur);
	}
	if (!found) {
		return false;
	}
	if (consume) {
		f.seekg(tok_sz, std::ifstream::cur);
	}
	end_pos = f.tellg();
	if (end_pos == start_pos) {
		// found immediately; no data to push anywhere
		return true;
	}
	if (rf) {
		f.seekg(start_pos);
		assert(f.good());
		const size_t len(end_pos - start_pos);
		char *buf(reinterpret_cast<char *>(malloc(len)));
		f.read(buf, len);
		assert(f.good());
		assert(f.tellg() == end_pos);
		rf(buf, len, arg);
		free(buf);
	}
	return true;
}

bool xstream::seek(const stream_pos& pos)
{
	assert(m_fptr);
	if (pos < m_region.begin || pos > m_region.end) {
		return false;
	}
	m_fptr->seekg(pos);
	return m_fptr->good();
}

stream_pos xstream::tell()
{
	assert(m_fptr);
	return m_fptr->tellg();
}

stream_pos xstream::size()
{
	assert(m_fptr);
	stream_pos start_pos(tell());
	m_fptr->seekg(0, std::ifstream::end);
	stream_pos end_pos(tell());
	m_fptr->seekg(start_pos);
	return end_pos;
}

std::string xstream::read(size_t n)
{
	assert(m_fptr);
	stream_pos start_pos(tell());
	char *buf(reinterpret_cast<char *>(malloc(n)));
	m_fptr->read(buf, n);
	std::string s(buf, n);
	free(buf);
	seek(start_pos);
	return s;
}
