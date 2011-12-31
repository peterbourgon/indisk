#ifndef INDEX_STATE_HH_
#define INDEX_STATE_HH_

#include <vector>
#include <fstream>
#include "definitions.hh"
#include "thread.hh"

// How many articles we will link to a term in the inverted index
// before flushing that information to the disk (index file).
#define FLUSH_LIMIT 256

struct index_state : public monitor
{
public:
	index_state(const std::string index_filename);
	
	str_id_map articles;
	str_id_map terms;
	tid_aids_map inverted_index;
	tid_offsets_map tid_offsets;
	
	std::string index_file;
	std::string header_file;
	std::ofstream ofs_header;
	std::ofstream ofs_index;
	
	bool finalized;
	
private:
	uint32_t aid;
	uint32_t tid;
	
	uint32_t termid(const std::string& s);
	uint32_t articleid(const std::string& s);

	// Dump the term ID + a vector of article IDs to the current position
	// in the passed output file stream, registers that offset in the
	// tid_offsets map (via register_tid_offset, below).
	void flush(
			std::ofstream& ofs_index,
			uint32_t tid,
			id_vector& aids);
	
	// Register an offset (from flush(), above) with a term ID, to be later
	// used while writing the index header.
	void register_tid_offset(uint32_t tid, uint32_t offset);
	
	// Write the index header to the given header file stream.
	void write_header();
	
public:
	
	// Associate the given article to the given term.
	// If FLUSH_LIMIT articles have been associated with the term,
	// flush that term ID + article ID list to the given file stream,
	// and register the resultant offset with the term ID, for the header.
	void index(const std::string& term, const std::string& article);
	
	// Flush all remaining term ID + article ID vectors
	// to the given file stream.
	void finalize();
	
	// Return how many unique articles or terms have been indexed.
	size_t article_count() const;
	size_t term_count() const;
};

#endif
