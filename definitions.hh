#ifndef DEFINITIONS_HH_
#define DEFINITIONS_HH_

#include <string>
#include <vector>
#include <set>

#ifndef UINT32_MAX
#define UINT32_MAX (0xFFFFFFFF)
#endif

typedef std::vector<uint32_t> id_vector;
typedef std::vector<uint32_t> offset_vector; // in index portion
typedef std::vector<uint32_t> header_offset_vector;

struct search_result {
	search_result(const std::string& article, size_t weight)
	: article(article)
	, weight(weight)
	{
		//
	}
	
	bool operator<(const search_result& rhs) const
	{
		if (weight > rhs.weight) {
			return true;
		} else if (weight == rhs.weight) {
			return article < rhs.article;
		} else {
			return false;
		}
	}
	
	std::string article;
	size_t weight;
};

struct search_results {
	search_results() : total(0) { }
	
	size_t total;
	std::set<search_result> top;
};

// Works On My Machines(tm)
// The following code compiles on Mac OS X 10.7 (Lion) with the
// Xcode 4 SDK (giving g++ 4.2.1), and Debian Linux 6 (Squeeze) 
// with g++ 4.3.2. I hope it works for you, too.

#if __GNUG__ >= 4 // __GNUG__ == (__GNUC__ && __cplusplus)
# if __GNUC_MINOR__ == 2

// I *think* this is the proper definition for g++ 4.2.x, but
// in reality it's just what my Mac OS 10.7 (Lion) computer
// happens to have. Maybe Apple fucked around with namespace
// names.

#include <ext/hash_map>

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

typedef __gnu_cxx::hash_map<std::string, uint32_t> str_id_map;
typedef __gnu_cxx::hash_map<uint32_t, id_vector> tid_aids_map;
typedef __gnu_cxx::hash_map<uint32_t, offset_vector> tid_offsets_map;
typedef __gnu_cxx::hash_map<uint32_t, std::string> aid_title_map;
typedef __gnu_cxx::hash_map<std::string, header_offset_vector> term_hov_map;

# elif __GNUC_MINOR__ > 2

// Similarly to above, this is just what my Debian 6 (Squeeze)
// server happened to have installed. I hope it maps to true
// g++ 4.3.x definitions...

#include <tr1/unordered_map>

typedef std::tr1::unordered_map<std::string, uint32_t> str_id_map;
typedef std::tr1::unordered_map<uint32_t, id_vector> tid_aids_map;
typedef std::tr1::unordered_map<uint32_t, offset_vector> tid_offsets_map;
typedef std::tr1::unordered_map<uint32_t, std::string> aid_title_map;
typedef std::tr1::unordered_map<std::string, header_offset_vector> term_hov_map;

# endif
#else

// Lower versions are (probably) supportable, but I don't have a way to test
#error g++ version 4.2 or higher is required

#endif


#endif
