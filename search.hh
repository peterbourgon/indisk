#ifndef SEARCH_HH_
#define SEARCH_HH_

#include <string>
#include <vector>
#include "def.hh"

#define MAX_SEARCH_RESULTS 10

size_t init_indices(const std::vector<std::string>& filenames);
search_results search_indices(const std::string& term);

#endif
