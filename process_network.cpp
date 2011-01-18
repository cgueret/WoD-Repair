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
#include <google/dense_hash_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include "zlib.h"
#include <math.h>
#include "common.h"

// Definition of the profiles
typedef google::sparse_hash_map<unsigned int, double, std::tr1::hash<unsigned int> > t_vals;
typedef t_vals::const_iterator t_vals_it;
typedef t_vals::iterator t_vals_itrw;
typedef struct {
	t_vals domains;
	t_vals ranges;
} t_profile;
typedef google::sparse_hash_map<unsigned int, t_profile*, std::tr1::hash<unsigned int> > t_profiles;
typedef t_profiles::const_iterator t_profiles_it;
t_profiles profiles;

// Nb of domain and ranges
t_reverse_dict range;
t_reverse_dict domain;
t_reverse_dict ns;

unsigned int NB_RANGES;
unsigned int NB_DOMAINS;

/*
 *
 */
void load_dictionaries() {
	//std::cout << "Count the number of domain and ranges" << std::endl;

	// Vars for a raw
	std::string predicate;
	unsigned int id;

	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler;

	std::cout << "Load ranges\n";
	NB_RANGES = 0;
	handler = gzopen("data/network/dictionary_ranges.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		std::stringstream ss(buffer);
		ss >> predicate >> id;
		range[id] = predicate;
		if (id > NB_RANGES)
			NB_RANGES = id;
	}
	gzclose(handler);

	std::cout << "Load domains\n";
	NB_DOMAINS = 0;
	handler = gzopen("data/network/dictionary_domains.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		std::stringstream ss(buffer);
		ss >> predicate >> id;
		domain[id] = predicate;
		if (id > NB_DOMAINS)
			NB_DOMAINS = id;
	}
	gzclose(handler);

	std::cout << "Load namespaces\n";
	handler = gzopen("data/raw/dictionary_namespace.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		std::stringstream ss(buffer);
		ss >> predicate >> id;
		ns[id] = predicate;
	}
	gzclose(handler);


	delete buffer;
}

/*
 *
 */
void load_profiles() {
	std::cout << "Load profiles" << std::endl;

	// Vars for a raw
	char type;
	unsigned int name_id;
	unsigned int predicate_id;
	double count;

	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/network/profiles.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		if (buffer[0] == '#')
			continue;

		// Read the line
		std::stringstream ss(buffer);
		ss >> type >> name_id >> predicate_id >> count;
		if ((type != 'D') && (type != 'R'))
			continue;

		// Get the profile of the node
		t_profile* profile;
		t_profiles::const_iterator it = profiles.find(name_id);
		if (it == profiles.end()) {
			profile = new t_profile();
			profiles[name_id] = profile;
		} else {
			profile = it->second;
		}

		// Update the profile
		if (type == 'D')
			(profile->domains)[predicate_id] = count;
		else if (type == 'R')
			(profile->ranges)[predicate_id] = count;
	}
	gzclose(handler);
	delete buffer;

	std::cout << "Loaded " << profiles.size() << std::endl;
}

/*
 * (t/T) * (D / (1+|t € D|))
 * http://en.wikipedia.org/wiki/Tfidf
 */
void compute_and_save_tf_idf() {
	// Compute the idf for the domains
	double* idf_domains = new double[NB_DOMAINS];
	double* idf_ranges = new double[NB_RANGES];
	for (size_t i = 0; i < NB_DOMAINS; i++)
		idf_domains[i] = 1;
	for (size_t i = 0; i < NB_RANGES; i++)
		idf_ranges[i] = 1;
	for (t_profiles_it it = profiles.begin(); it != profiles.end(); it++) {
		for (t_vals_it it2 = it->second->domains.begin(); it2 != it->second->domains.end(); it2++)
			idf_domains[it2->first - 1] += 1;
		for (t_vals_it it2 = it->second->ranges.begin(); it2 != it->second->ranges.end(); it2++)
			idf_ranges[it2->first - 1] += 1;
	}
	for (size_t i = 0; i < NB_DOMAINS; i++)
		idf_domains[i] = log(profiles.size() / idf_domains[i]);
	for (size_t i = 0; i < NB_RANGES; i++)
		idf_ranges[i] = log(profiles.size() / idf_ranges[i]);

	std::ofstream handle;
	std::ofstream handle2;
	handle.open("data/network/profiles-tfidf.csv");
	handle << "# Domain/Range id predicate value" << std::endl;
	handle2.open("data/network/profiles-tfidf-clear.csv");
	handle2 << "# Domain/Range id predicate value" << std::endl;

	// Compute the tfidf for each term
	double total = 0;
	for (t_profiles_it it = profiles.begin(); it != profiles.end(); it++) {
		total = 0;
		for (t_vals_it it2 = it->second->domains.begin(); it2 != it->second->domains.end(); it2++)
			total += it2->second;
		if (total != 0) {
			for (t_vals_it it2 = it->second->domains.begin(); it2 != it->second->domains.end(); it2++) {
				double v = (it2->second / total) * idf_domains[it2->first - 1];
				handle << "D " << it->first << " " << it2->first << " " << v << std::endl;
				handle2 << "D " << ns[it->first] << " " << domain[it2->first] << " " << v << std::endl;
			}
		}

		total = 0;
		for (t_vals_it it2 = it->second->ranges.begin(); it2 != it->second->ranges.end(); it2++)
			total += it2->second;
		if (total != 0) {
			for (t_vals_it it2 = it->second->ranges.begin(); it2 != it->second->ranges.end(); it2++) {
				double v = (it2->second / total) * idf_ranges[it2->first - 1];
				handle << "R " << it->first << " " << it2->first << " " << v << std::endl;
				handle2 << "R " << ns[it->first] << " " << range[it2->first] << " " << v << std::endl;
			}
		}

		handle << std::flush;
		handle2 << std::flush;
	}

	handle.close();
	handle2.close();
}

/*
 * Main executable part
 */
int main() {
	profiles.resize(13000000);

	// Load dicts and count the domain and ranges
	load_dictionaries();

	// Load the profiles
	load_profiles();

	// Compute tf_idf
	compute_and_save_tf_idf();
}
