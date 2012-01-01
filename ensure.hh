#ifndef ENSURE_HH_
#define ENSURE_HH_

void ensure(bool condition, const std::string& file, int line)
{
	if (!condition) {
		std::ostringstream oss;
		oss << "ensure failed at " << file << ":" << line;
		throw std::runtime_error(oss.str());
	}
}

#define ENSURE(x) ensure(x, __FILE__, __LINE__)

#endif
