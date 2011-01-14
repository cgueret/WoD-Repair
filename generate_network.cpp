//============================================================================
// Name        : generate_network.cpp
// Author      : Christophe Gueret <christophe.gueret@gmail.com>
// Version     :
// Copyright   :
// Description :
//============================================================================

#include <iostream>
#include <sstream>
#include <fstream>
#include "zlib.h"
#include <pcre.h>
#include <string>
#include <cstring>
#include <google/sparse_hash_map>
#include <vector>
#include <unordered_set>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#define foreach         BOOST_FOREACH

#include "common.h"

// Custom types
typedef std::unordered_set<std::string> t_set;
typedef struct {
	t_set domains;
	t_set ranges;
} t_domains_and_ranges;
typedef google::sparse_hash_map<std::string, t_domains_and_ranges, std::tr1::hash<std::string> > t_domain_range;
typedef t_domain_range::const_iterator t_domain_range_it;

typedef struct {
	t_dict* domains;
	t_dict* ranges;
} t_profile;
typedef google::sparse_hash_map<std::string, t_profile*, std::tr1::hash<std::string> > t_profiles;
typedef t_profiles::const_iterator t_profile_it;

// Store the domain and ranges of predicates
t_domain_range domain_range;

// Store the topology of the network as a dict "id_ns id_ns"->count
t_dict network;

// Namespaces profiles as "id_ns" -> t_domains_and_ranges_counter
t_profiles namespaces_profiles;

// Predicates dictionary as dict id->string
t_reverse_dict predicates_dict;

// The regular expression used to match triples and related stuff
#define CHUNK 524288
#define OVECCOUNT 30

/*
 * Load the ntriples files with the list of domain and ranges in it
 */
void load_domains_and_ranges() {
	std::cout << "Load domains and ranges" << std::endl;

	// Define the regexp to use to match the triples
	const char *error;
	int errpos;
	int ovector[OVECCOUNT];
	pcre *triple_re;
	triple_re = pcre_compile("^<https?://([^>]+)> <https?://([^>]+)> <https?://([^>]+)>.*$", 0, &error, &errpos, NULL);

	// Parse the file
	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/raw/ranges_and_domains.nt.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		// Try to match the pattern
		int rc = pcre_exec(triple_re, NULL, buffer, strlen(buffer), 0, 0, ovector, OVECCOUNT);
		if (rc != 4)
			continue;

		// The predicate we want to know the range/domain is first
		char* s_start = buffer + ovector[2];
		int s_length = ovector[3] - ovector[2];
		std::string predicate = std::string(s_start, s_length);

		// Second part indicates if it is a range or a domain
		char* p_start = buffer + ovector[4];
		int p_length = ovector[5] - ovector[4];
		std::string domain_or_range = std::string(p_start, p_length);

		// Last is the value
		char* o_start = buffer + ovector[6];
		int o_length = ovector[7] - ovector[6];
		std::string value = std::string(o_start, o_length);

		// Get the domain/range record for the predicate, or create it
		t_domains_and_ranges record;
		t_domain_range_it it = domain_range.find(predicate);
		if (it != domain_range.end())
			record = it->second;
		if (domain_or_range == "www.w3.org/2000/01/rdf-schema#range")
			record.ranges.insert(value);
		else if (domain_or_range == "www.w3.org/2000/01/rdf-schema#domain")
			record.domains.insert(value);

		domain_range[predicate] = record;
	}
	gzclose(handler);
	delete buffer;
	pcre_free(triple_re);

	std::cout << "Domain and range known for " << domain_range.size() << " predicates" << std::endl << std::flush;
}

/*
 * Load the predicate dictionary
 */
void load_predicate_dictionary() {
	std::cout << "Load predicates dict" << std::endl;

	// Define the regexp to use to match the triples
	const char *error;
	int errpos;
	int ovector[OVECCOUNT];
	pcre *triple_re;
	triple_re = pcre_compile("^([^ ]+) ([0-9]+)$", 0, &error, &errpos, NULL);

	// Parse the file
	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/raw/dictionary_predicate.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		// Try to match the pattern
		int rc = pcre_exec(triple_re, NULL, buffer, strlen(buffer), 0, 0, ovector, OVECCOUNT);
		if (rc != 3)
			continue;

		// Read the line
		std::string predicate = std::string(buffer + ovector[2], ovector[3] - ovector[2]);
		std::string id = std::string(buffer + ovector[4], ovector[5] - ovector[4]);

		// If we don't have domain/ranges about that predicate ignore it
		t_domain_range_it it = domain_range.find(predicate);
		if (it == domain_range.end())
			continue;

		predicates_dict[id] = predicate;
	}
	gzclose(handler);
	delete buffer;
	pcre_free(triple_re);

	std::cout << "Loaded " << predicates_dict.size() << " entries from dict" << std::endl << std::flush;
}

t_profile* get_profile(std::string id) {
	t_profile* result;
	t_profile_it it = namespaces_profiles.find(id);
	if (it == namespaces_profiles.end()) {
		result = new t_profile();
		result->domains = new t_dict();
		result->ranges = new t_dict();
		namespaces_profiles[id] = result;
	} else {
		result = it->second;
	}
	return result;
}

void load_internal_connections() {
	std::cout << "Add internal connections to the profiles" << std::endl;

	// Define the regexp to use to match the triples
	const char *error;
	int errpos;
	int ovec[OVECCOUNT];
	pcre *re;
	re = pcre_compile("^([0-9]+) ([0-9]+) ([0-9]+)$", 0, &error, &errpos, NULL);

	// Parse the file
	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/raw/connections_internal.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		// Try to match the pattern
		int rc = pcre_exec(re, NULL, buffer, strlen(buffer), 0, 0, ovec, OVECCOUNT);
		if (rc != 4)
			continue;

		// Read the line
		std::string name_id = std::string(buffer + ovec[2], ovec[3] - ovec[2]);
		std::string pred_id = std::string(buffer + ovec[4], ovec[5] - ovec[4]);
		unsigned int count = boost::lexical_cast<unsigned int>(std::string(buffer + ovec[6], ovec[7] - ovec[6]));

		// If we don't have domain/ranges about that predicate ignore it
		t_reverse_dict_it it = predicates_dict.find(pred_id);
		if (it == predicates_dict.end())
			continue;

		// Get the domain and ranges
		std::string pred_str = it->second;
		t_domains_and_ranges record = domain_range.find(pred_str)->second;

		// Get the relevant profile
		t_profile* profile = get_profile(name_id);

		// Append domains and ranges
		foreach ( std::string domain, record.domains)
					{
						unsigned int v = 0;
						t_dict_it a = profile->domains->find(domain);
						if (a != profile->domains->end())
							v = a->second;
						(*profile->domains)[domain] = v + count;
					}
		foreach ( std::string range, record.ranges)
					{
						unsigned int v = 0;
						t_dict_it a = profile->ranges->find(range);
						if (a != profile->ranges->end())
							v = a->second;
						(*profile->ranges)[range] = v + count;
					}
	}
	gzclose(handler);
	delete buffer;
	pcre_free(re);
}

void load_other_connections() {
	std::cout << "Add connections between namespaces to the profiles" << std::endl;

	// Define the regexp to use to match the triples
	const char *error;
	int errpos;
	int ovec[OVECCOUNT];
	pcre *re;
	re = pcre_compile("^([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)$", 0, &error, &errpos, NULL);

	// Parse the file
	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/raw/connections_inter_ns.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		// Try to match the pattern
		int rc = pcre_exec(re, NULL, buffer, strlen(buffer), 0, 0, ovec, OVECCOUNT);
		if (rc != 5)
			continue;

		// Read the line
		std::string start_id = std::string(buffer + ovec[2], ovec[3] - ovec[2]);
		std::string end_id = std::string(buffer + ovec[4], ovec[5] - ovec[4]);
		std::string pred_id = std::string(buffer + ovec[6], ovec[7] - ovec[6]);
		unsigned int count = boost::lexical_cast<unsigned int>(std::string(buffer + ovec[8], ovec[9] - ovec[8]));

		// If we don't have domain/ranges about that predicate ignore it
		t_reverse_dict_it it = predicates_dict.find(pred_id);
		if (it == predicates_dict.end())
			continue;

		// Get the domain and ranges
		std::string pred_str = it->second;
		t_domains_and_ranges record = domain_range.find(pred_str)->second;

		// Get the relevant profiles
		t_profile* start_profile = get_profile(start_id);
		t_profile* end_profile = get_profile(end_id);

		// Append domain of relation to range of start
		foreach ( std::string domain, record.domains)
					{
						unsigned int v = 0;
						t_dict_it a = start_profile->ranges->find(domain);
						if (a != start_profile->ranges->end())
							v = a->second;
						(*start_profile->ranges)[domain] = v + count;
					}

		// Append range of relation to domain of end
		foreach ( std::string range, record.ranges)
					{
						unsigned int v = 0;
						t_dict_it a = end_profile->domains->find(range);
						if (a != end_profile->domains->end())
							v = a->second;
						(*end_profile->domains)[range] = v + count;
					}

		// Store that edge into the network
		std::string edge_label = start_id + " " + end_id;
		t_dict_it it2 = network.find(edge_label);
		unsigned int v = 0;
		if (it2 != network.end())
			v = it2->second;
		network[edge_label] = v + 1;
	}
	gzclose(handler);
	delete buffer;
	pcre_free(re);
}

/*
 * Save the profiles of the nodes
 */
void save_files() {
	std::ofstream handle;

	std::cout << "Save profile of " << namespaces_profiles.size() << " nodes" << std::endl;
	handle.open("data/network/profiles.csv");
	handle << "# Domain/Range id predicate count" << std::endl;
	for (t_profile_it it = namespaces_profiles.begin(); it != namespaces_profiles.end(); it++) {
		for (t_dict_it it2 = it->second->domains->begin(); it2 != it->second->domains->end(); it2++)
			handle << "D " << it->first << " " << it2->first << " " << it2->second << std::endl;
		for (t_dict_it it2 = it->second->ranges->begin(); it2 != it->second->ranges->end(); it2++)
			handle << "R " << it->first << " " << it2->first << " " << it2->second << std::endl;
	}
	handle.close();

	std::cout << "Save a network with " << network.size() << " edges" << std::endl;
	handle.open("data/network/network.csv");
	handle << "# start end count" << std::endl;
	for (t_dict_it it = network.begin(); it != network.end(); it++)
		handle << it->first << " " << it->second << std::endl;
	handle.close();
}

/*
 * Main executable part
 */
int main() {
	// Load the domain and ranges
	load_domains_and_ranges();

	// Load the necessary part of the predicate dictionary
	load_predicate_dictionary();

	// Parse internal connections to create the profile
	load_internal_connections();

	// Parse internal connections to create the profile
	load_other_connections();

	// Save node profiles
	save_files();

	return 0;
}
