//============================================================================
// Name        : process_network.cpp
// Author      : Christophe Gueret <christophe.gueret@gmail.com>
// Version     :
// Copyright   :
// Description :
//============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <google/sparse_hash_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include "zlib.h"

// Custom types
typedef std::set<std::string> t_set;
typedef struct {
	t_set* domains;
	t_set* ranges;
} t_profile;
typedef google::sparse_hash_map<unsigned int, t_profile*, std::tr1::hash<unsigned int> > t_profiles;

// White list
std::set<unsigned int> white_list;

// The profiles
t_profiles profiles;

/*
 * Compare the profiles
 */
void compare_profiles() {
	for (t_profiles::const_iterator it = profiles.begin(); it != profiles.end(); it++) {
		for (t_profiles::const_iterator it2 = it; it2 != profiles.end(); it2++) {
			if (it == it2)
				continue;

			t_set tmp;
			std::set_intersection(it->second->ranges->begin(), it->second->ranges->end(),
					it2->second->domains->begin(), it2->second->domains->end(), std::inserter(tmp, tmp.begin()));
			if (tmp.size() > 0) {
				std::cout << it->first << " " << it2->first << " ";
				std::copy(tmp.begin(), tmp.end(), std::ostream_iterator<const std::string>(std::cout, " "));
				std::cout << std::endl;
			}
		}
	}
}

void load_profiles() {
	std::cout << "Load profiles" << std::endl;
	// Vars for a raw
	char type;
	unsigned int id;
	std::string predicate;
	double count;

	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/network/profiles.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		// Read the line
		std::stringstream ss(buffer);
		ss >> type >> id >> predicate >> count;

		// White ?
		if (white_list.find(id) == white_list.end())
			continue;

		// Ignore very common stuff
		if (predicate == "www.w3.org/2002/07/owl#Thing")
			continue;
		if (predicate == "www.w3.org/2000/01/rdf-schema#Resource")
			continue;
		if (predicate == "www.w3.org/2000/01/rdf-schema#Class")
			continue;
		if (predicate == "www.w3.org/2002/07/owl#Class")
			continue;

		// Get the profile of the node
		t_profile* profile;
		t_profiles::const_iterator it = profiles.find(id);
		if (it == profiles.end()) {
			profile = new t_profile();
			profile->domains = new t_set();
			profile->ranges = new t_set();
			profiles[id] = profile;
		} else {
			profile = it->second;
		}

		// Update the profile
		if (type == 'D')
			profile->domains->insert(predicate);
		else if (type == 'R')
			profile->ranges->insert(predicate);
	}
	gzclose(handler);
	delete buffer;

	std::cout << "Loaded " << profiles.size() << std::endl;
}

void load_whitelist() {
	std::cout << "Load whitelist" << std::endl;
	// Vars for a raw
	unsigned int id;
	unsigned int predicate;
	double count;

	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/network/white_list.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		// Read the line
		std::stringstream ss(buffer);
		ss >> id >> predicate >> count;
		white_list.insert(id);
	}
	gzclose(handler);
	delete buffer;
}

/*
 * Main executable part
 */
int main() {
	// Load the whitelist
	load_whitelist();

	// Load the profiles
	load_profiles();

	// Compare the profiles
	compare_profiles();
}
