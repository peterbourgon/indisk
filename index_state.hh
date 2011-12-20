#ifndef INDEX_STATE_HH_
#define INDEX_STATE_HH_

#include <vector>
#include <fstream>
#include "definitions.hh"

#define FLUSH_LIMIT 64
struct index_state
{
public:
	index_state();
	
	str_id_map articles;
	str_id_map terms;
	tid_aids_map inverted_index;
	tid_offsets_map tid_offsets;
	
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
	
public:
	
	// Associate the given article to the given term.
	// If FLUSH_LIMIT articles have been associated with the term,
	// flush that term ID + article ID list to the given file stream,
	// and register the resultant offset with the term ID, for the header.
	void index(
			const std::string& term,
			const std::string& article,
			std::ofstream& ofs_index);
	
	// Flush all remaining term ID + article ID vectors
	// to the given file stream.
	void finalize(std::ofstream& ofs_index);
	
	// After finalizing, write the index header
	// to the given file stream.
	void write_header(std::ofstream& ofs_header);

	// Return how many unique terms have been indexed.
	size_t term_count() const;
};

#endif
