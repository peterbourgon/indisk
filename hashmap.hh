#ifndef HASHMAP_HH_
#define HASHMAP_HH_

#include <ext/hash_map>
#include <string>

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
