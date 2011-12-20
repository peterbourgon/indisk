#ifndef HASHMAP_HH_
#define HASHMAP_HH_

#include <string>

#if __GNUG__ >= 4 // __GNUG__ == (__GNUC__ && __cplusplus)
# if __GNUC_MINOR__ == 2
#include <ext/hash_map>
# elif __GNUC_MINOR__ > 2
#include <unordered_map>
# endif
#else
// Lower versions are (probably) supportable, but I don't have a way to test
#error g++ version 4.2 or higher is required
#endif

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

#endif
