#ifndef IDX_HH_
#define IDX_HH_

#include <fstream>
#include "thread.hh"
#include "definitions.hh"

// The index_st accepts index() calls, and stores those associations
// to an inverted index, in memory.
//
// When a given term collects PARTIAL_FLUSH_LIMIT articles,
// index_st will flush that association to a partial index file.
// Metadata about that term/article association is kept in memory,
// to be used in a header section of the eventual complete index file.
//
// When the thing calling index_st::index detects article_count()
// above some threshold, it should call flush(), which will
//  - write out all partial index metadata to a header file
//  - merge the header and index files to a single file
//  - reset the internal state

// How many articles need to be linked to a term
// before we perform a partial_flush().
#define PARTIAL_FLUSH_LIMIT 256

class index_st : public monitor
{
public:
	index_st(const std::string& basename);
	
	// Associate term to article in the inverted index.
	void index(const std::string& term, const std::string& article);
	
	// Flush all collected state to disk, in a new index file.
	// Should be triggered by whoever calls index(),
	// after article_count() has reached some threshold.
	void flush();
	
	// Articles in memory, ie. since last flush.
	size_t article_count() const;
	
	
protected:
	// Returns the article or term ID for the given string,
	// or generates a new one if it doesn't yet exist.
	uint32_t article_id(const std::string& article);
	uint32_t term_id(const std::string& term);
	
	// Flushes the tid:aids mapping to the disk,
	// to the next position in the index output stream.
	void partial_flush(uint32_t tid, id_vector& aids);
	
	// Registers the offset of the partially-flushed term ID
	// into the tid_offsets_map, for use in the header.
	void register_tid_offset(uint32_t tid, uint32_t offset);
	
	//
	void recreate_output_files();
	std::string idx_filename();
	std::string hdr_filename();
	
	//
	void write_header();
	
	// 
	void reset_state();
	
private:
	const std::string m_basename;
	size_t m_flush_count;
	
	uint32_t m_aid;
	uint32_t m_tid;
	
	str_id_map m_articles;
	str_id_map m_terms;
	tid_aids_map m_inverted_index;
	tid_offsets_map m_tid_offsets;
	
	// The index and header portions of the currently active index file.
	// These should be maintained by flush().
	std::ofstream *m_ofs_idx;
	std::ofstream *m_ofs_hdr;
	
};

#endif