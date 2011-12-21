#include <iostream>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include "xmlparse.hh"
#include "index_state.hh"
#include "definitions.hh"

bool index_article(stream& s, index_state& is)
{
	if (!s.read_until("<title>", NULL, NULL)) {
		return false;
	}
	std::string title;
	if (!s.read_until("<", parse_title, &title)) {
		return false;
	}
	if (title.empty()) {
		return false;
	}
	assert(title.find(END_DELIM) == std::string::npos);
	//std::cout << "parsed title: " << title << std::endl;
	if (!s.read_until("<contributor>", NULL, NULL)) {
		return false;
	}
	std::string contrib;
	if (!s.read_until("</contributor>", parse_contrib, &contrib)) {
		return false;
	}
	//std::cout << "parsed contributor: " << contrib << std::endl;
	if (!s.read_until("<text", NULL, NULL)) {
		return false;
	}
	if (!s.read_until(">", NULL, NULL)) {
		return false;
	}
	parse_text_context ctx(title, is);
	if (!s.read_until("</text", parse_text, &ctx)) {
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
			const std::string& filename,
			size_t start_pos,
			size_t end_pos,
			index_state& is)
	: m_stream(filename, start_pos, end_pos)
	, m_is(is)
	, m_finished(false)
	{
		//
	}
	
	virtual void run()
	{
		while (index_article(m_stream, m_is));
		scoped_lock sync(monitor_mutex);
		m_finished = true;
	}
	
	bool finished() const
	{
		scoped_lock sync(monitor_mutex);
		return m_finished;
	}
	
private:
	stream m_stream;
	index_state& m_is;
	bool m_finished;
};

typedef std::pair<size_t, size_t> split_pair;
typedef std::vector<split_pair> split_pairs;

split_pairs get_split_positions(const std::string& xml_filename)
{
	split_pairs p;
	const size_t ncpu(get_cpus());
	std::cout << "indexing on " << ncpu << " threads" << std::endl;
	stream s(xml_filename);
	const size_t sz(s.size());
	size_t last_end(0);
	
	// split up the file into roughly equal sections
	// based on the <title> delimiter
	for (size_t i(0); i < ncpu-1; ++i) {
		// begin where we left off
		size_t begin_pos(last_end);
		s.seek(begin_pos);
		// look for the next <title>
		s.read_until("<title>", NULL, NULL);
		// we will make one partition from here
		begin_pos = s.tell();
		// move to where we think the next title may be
		size_t end_pos( (sz / ncpu) * (i+1) );
		s.seek(end_pos);
		// look for the next <title>
		s.read_until("<title>", NULL, NULL);
		assert(s.tell() > 0);
		// we will make that partition end here
		end_pos = s.tell();
		// insert the partition
		p.push_back(std::make_pair(begin_pos, end_pos));
		// and mark where we begin the next search
		last_end = end_pos;
	}
	
	// make sure we get all the bytes
	p.push_back(std::make_pair(last_end, s.size()));
	return p;
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		std::cerr << "usage: " << argv[0] << " <xml> <idx>" << std::endl;
		return 1;
	}
	index_state is(argv[2]);
	split_pairs p(get_split_positions(argv[1]));
	std::vector<index_thread *> threads;
	typedef std::vector<index_thread *>::iterator thit;
	for (split_pairs::const_iterator it(p.begin()); it != p.end(); ++it) {
		std::cout << "partition: "
		          << it->first << '-' << it->second
		          << std::endl;
		index_thread *t(new index_thread(argv[1], it->first, it->second, is));
		t->start();
		threads.push_back(t);
	}
	for (size_t i(1); ; i++) {
		size_t finished_count(0);
		for (thit it(threads.begin()); it != threads.end(); ++it) {
			if ((*it)->finished()) {
				finished_count++;
			}
		}
		const size_t articles(is.article_count());
		const size_t aps(articles / i);
		std::cout << is.article_count() << " articles indexed" 
		          << " (~" << aps << "/s)" 
		          << std::endl;
		if (finished_count >= threads.size()) {
			break;
		}
		sleep(1);
	}
	std::cout << is.term_count() << " terms indexed" << std::endl;
	for (thit it(threads.begin()); it != threads.end(); ++it) {
		(*it)->join();
		delete *it;
	}
	std::cout << "finalizing" << std::endl;
	is.finalize();
	std::cout << "merging" << std::endl;
	merge("index.header", argv[2]);
	return 0;
}
