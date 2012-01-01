#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include "def.hh"
#include "xml.hh"
#include "idx.hh"

/*
static bool index_article(stream& s, index_state& is)
{
	if (!s.read_until("<title>", true, NULL, NULL)) {
		return false;
	}
	std::string title;
	if (!s.read_until("<", false, parse_title, &title)) {
		return false;
	}
	if (title.empty()) {
		return false;
	}
	if (title.find("Category:") == 0) {
		// Don't index category pages
		return true;
	}
	assert(title.find(END_DELIM) == std::string::npos);
	//std::cout << "parsed title: " << title << std::endl;
	if (!s.read_until("<contributor>", true, NULL, NULL)) {
		return false;
	}
	std::string contrib;
	if (!s.read_until("</contributor>", false, parse_contrib, &contrib)) {
		return false;
	}
	//std::cout << "parsed contributor: " << contrib << std::endl;
	if (!s.read_until("<text", true, NULL, NULL)) {
		return false;
	}
	if (!s.read_until(">", true, NULL, NULL)) {
		return false;
	}
	parse_text_context ctx(title, is);
	if (!s.read_until("</text", false, parse_text, &ctx)) {
		return false;
	}
	return true;
}

void merge(const std::string& header, const std::string& index)
{
	// :(
	{
		std::ostringstream oss;
		oss << "cat " << index << " >> " << header;
		system(oss.str().c_str());
	}
	{
		std::ostringstream oss;
		oss << "mv " << header << " " << index;
		system(oss.str().c_str());
	}
}

class index_thread : public synchronized_threadbase
{
public:
	index_thread(
			const std::string& xml_filename,
			const std::string& idx_filename,
			uint64_t start_pos,
			uint64_t end_pos)
	: m_stream(xml_filename, start_pos, end_pos)
	, m_is(new index_state(idx_filename))
	, m_finished(false)
	{
		//
	}
	
	~index_thread()
	{
		if (m_is) {
			delete m_is;
		}
	}
	
	virtual void run()
	{
		while (index_article(m_stream, *m_is));
		std::cout << "index thread finalizing" << std::endl;
		m_is->finalize();
		std::cout << "index thread merging" << std::endl;
		merge(m_is->header_file, m_is->index_file);
		scoped_lock sync(monitor_mutex);
		std::cout << "index thread finished" << std::endl;
		m_finished = true;
	}
	
	bool finished() const
	{
		scoped_lock sync(monitor_mutex);
		return m_finished;
	}
	
	index_state *idx_st()
	{
		return m_is;
	}
	
	uint64_t streampos() { return m_stream.tell(); }
	
private:
	stream m_stream;
	index_state *m_is;
	bool m_finished;
};

typedef std::pair<uint64_t, uint64_t> split_pair;
typedef std::vector<split_pair> split_pairs;

split_pairs get_split_positions(const std::string& xml_filename)
{
	split_pairs p;
	const size_t ncpu(get_cpus());
	std::cout << "indexing on " << ncpu
	          << " thread" << (ncpu == 1 ? "" : "s")
	          << std::endl;
	stream s(xml_filename, 0, 0);
	const uint64_t sz(s.size());
	std::ifstream::pos_type last_end(s.tell());
	std::cout << xml_filename << ": " << sz << " offsets" << std::endl;
	
	// split up the file into roughly equal sections
	// based on the <title> delimiter
	for (size_t i(0); i < ncpu-1; ++i) {
		// begin where we left off
		std::ifstream::pos_type begin_pos(last_end);
		s.seek(begin_pos);
		// look for the next <title>
		s.read_until("<title>", false, NULL, NULL);
		assert(s.read(7) == "<title>");
		assert(s.read(7) == "<title>");
		// we will make one partition from here
		begin_pos = s.tell();
		// move to where we think the next title may be
		std::ifstream::pos_type end_pos( (sz / ncpu) * (i+1) );
		s.seek(end_pos);
		// look for the next <title>
		s.read_until("<title>", false, NULL, NULL);
		assert(s.read(7) == "<title>");
		assert(s.tell() > 0);
		// we will make that partition end here
		end_pos = s.tell();
		// insert the partition
		p.push_back(std::make_pair(last_end, end_pos));
		// and mark where we begin the next search
		last_end = end_pos;
	}
	
	// make sure we get all the bytes
	p.push_back(std::make_pair(last_end, s.size()));
	return p;
}
*/

int main(int argc, char *argv[])
{
	if (argc < 3) {
		std::cerr << "usage: " << argv[0] << " <xml> <idx>" << std::endl;
		return 1;
	}
	int rc(0);
	try {
		// compute regions
		std::vector<region> regions(regionize(argv[1], get_cpus()));
		// start threads
		std::vector<idx_thread *> threads;
		size_t thread_id(1);
		typedef std::vector<region>::const_iterator rcit;
		typedef std::vector<idx_thread *>::iterator thit;
		for (rcit it(regions.begin()); it != regions.end(); ++it) {
			std::cout << "region: " << it->begin << '-' << it->end << std::endl;
			std::ostringstream oss;
			oss << argv[2] << "." << thread_id++;
			idx_thread *t(new idx_thread(argv[1], *it, oss.str()));
			t->start();
			threads.push_back(t);
		}
		// wait for completion + calculate statistics
		for (size_t i(1); ; i++) {
			size_t finished_count(0), articles(0);
			for (thit it(threads.begin()); it != threads.end(); ++it) {
				if ((*it)->finished()) {
					finished_count++;
				}
				articles += (*it)->article_count();
			}
			const size_t aps(articles/i);
			std::cout << "indexed " << articles << " articles "
			          << "(~" << aps << "/s)"
			          << std::endl;
			if (finished_count >= threads.size()) {
				break;
			}
			sleep(1);
		}
		std::cout << "all indexing complete; finalizing" << std::endl;
		for (thit it(threads.begin()); it != threads.end(); ++it) {
			(*it)->join();
			delete *it;
		}
	} catch (const std::runtime_error& ex) {
		std::cerr << ex.what() << std::endl;
		rc = -1;
	}
	return rc;
}
