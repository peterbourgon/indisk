#ifndef INDEX_STATE_HH_
#define INDEX_STATE_HH_

#include <ext/hash_map>
#include <vector>
#include <fstream>

namespace __gnu_cxx
{
	template<> struct hash<std::string>
	{
		size_t operator()(const std::string& s) const
		{
			return hash<const char *>()(s.c_str());
		}
	};
}

#define FLUSH_LIMIT 64
struct index_state
{
public:
	index_state();
	
	typedef std::vector<uint32_t> id_vector;
	typedef std::vector<uint32_t> offset_vector;
	typedef __gnu_cxx::hash_map<std::string, uint32_t> str_id_map;
	typedef __gnu_cxx::hash_map<uint32_t, id_vector> tid_aids_map;
	typedef __gnu_cxx::hash_map<uint32_t, offset_vector> tid_offsets_map;
	
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
	// in the passed output file stream, and returns that position as an
	// offset.
	uint32_t flush(
			std::ofstream& ofs_index,
			uint32_t tid,
			const id_vector& aids) const;
	
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
};

#endif
