#ifndef IDX_HH_
#define IDX_HH_

#include <fstream>
#include "thread.hh"
#include "def.hh"
#include "xml.hh"

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
//  - reset the internal state (ie. so that article_count() returns 0)

// How many articles need to be linked to a term
// before we perform a partial_flush().
#define PARTIAL_FLUSH_LIMIT 256

class index_st : public monitor
{
public:
	index_st(const std::string& basename);
	
	// Associate terms to article in the inverted index.
	void index(const std::vector<std::string>& terms, const std::string& article);
	
	// Flush all collected state to disk, in a new index file.
	// Should be triggered by whoever calls index(),
	// after article_count() has reached some threshold.
	// last_flush=true won't recreate_output_files().
	void flush(bool last_flush=false);
	
	// Articles in memory, ie. since last flush.
	size_t article_count() const;
	
	// Introspection methods for tests.
	bool has_article(const std::string& article);
	bool is_associated(const std::string& article, const std::string& term);
	
protected:
	// Associate term to article in the inverted index.
	void index(const std::string& term, const std::string& article);
	
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
	
	// Deletes the two member ofstream pointers,
	// and recreates them using idx_ and hdr_filename().
	// The streams should be close()d prior to calling this.
	void recreate_output_files();
	
	// Return the appropriate (partial) index and header filenames
	// based on basename and m_flush_count.
	std::string idx_filename();
	std::string hdr_filename();
	
	// Writes the current state of the index to the header file
	// as returned by hdr_filename. 
	void write_header();
	
	// Resets all per-index-file state in the class instance to 0.
	// Typically called as part of flush().
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

// How many articles need to be indexed (in memory)
// before we flush the entire index (including header)
// to disk and start fresh.
#define ARTICLE_FLUSH_LIMIT 100000

class idx_thread : public synchronized_threadbase
{
public:
	idx_thread(
			const std::string& xml_filename,
			const region& r,
			const std::string& idx_filename); // should already be per-thread
	virtual ~idx_thread();
	virtual void run();
	bool finished() const;
	size_t article_count() const;
	
	const stream& get_stream() const { return m_s; }
	const index_st& get_index_st() const { return m_idx_st; }
	
private:
	bool m_started;
	stream m_s;
	index_st m_idx_st;
	size_t m_article_count;
};

enum index_result {
	INDEX_GOOD,
	NO_INDEX_BUT_CONTINUE,
	END_OF_REGION
};

index_result index_article(stream& s, index_st& idx_st);

#endif
