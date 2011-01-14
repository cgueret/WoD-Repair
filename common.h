#ifndef _COMMON_
#define _COMMON_

// Custom types
typedef google::sparse_hash_map<std::string, unsigned int, std::tr1::hash<std::string> > t_dict;
typedef t_dict::const_iterator t_dict_it;
typedef google::sparse_hash_map<std::string, std::string, std::tr1::hash<std::string> > t_reverse_dict;
typedef t_reverse_dict::const_iterator t_reverse_dict_it;

/*
 * Returns the end position of the namespace or -1 if not found
 */
inline int get_namespace_end(char* uri, int length) {
	int pos = 0;
	int last_slash = -1;
	while (pos != length) {
		// Return the first # found or update position of last /
		if (uri[pos] == '#')
			return pos + 1;
		else if (uri[pos] == '/')
			last_slash = pos + 1;
		pos++;
	}
	if (last_slash > -1)
		return last_slash;

	std::cout << std::string(uri, length) << std::endl;

	return -1;
}

#endif
