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
typedef std::unordered_set<unsigned int> t_set;
typedef struct {
	t_set domains;
	t_set ranges;
} t_domains_and_ranges;
typedef google::sparse_hash_map<std::string, t_domains_and_ranges*, std::tr1::hash<std::string> > t_domain_range;
typedef t_domain_range::const_iterator t_domain_range_it;

typedef google::sparse_hash_map<unsigned int, unsigned int, std::tr1::hash<unsigned int> > t_map;
typedef t_map::const_iterator t_map_it;
typedef struct {
	t_map* domains;
	t_map* ranges;
	unsigned int triples;
	unsigned int links;
} t_profile;
typedef google::sparse_hash_map<unsigned int, t_profile*, std::tr1::hash<unsigned int> > t_profiles;
typedef t_profiles::const_iterator t_profile_it;

// Store the domain and ranges of predicates
t_domain_range domain_range;

// Store the topology of the network as a dict "id_ns id_ns"->count
t_dict network;

// Namespaces profiles as "id_ns" -> t_domains_and_ranges_counter
t_profiles namespaces_profiles;

// Predicates dictionary as dict id->string
t_reverse_dict predicates_dict;

// Domain/range kw as string->id
t_dict domains_dict;
t_dict ranges_dict;

// The regular expression used to match triples and related stuff
#define CHUNK 524288
#define OVECCOUNT 30

/*
 *
 */
unsigned int string_to_index(std::string label, t_dict& dict) {
	unsigned int index = 0;
	t_dict_it it = dict.find(label);

	if (it == dict.end()) {
		index = dict.size() + 1;
		dict[label] = index;
	} else {
		index = it->second;
	}
	return index;
}

/*
 * Return the profile of a given identifier
 */
t_profile* get_profile(unsigned int id, bool create) {
	t_profile* result;
	t_profile_it it = namespaces_profiles.find(id);
	if (it == namespaces_profiles.end()) {
		if (!create)
			return NULL;

		result = new t_profile();
		result->domains = new t_map();
		result->ranges = new t_map();
		result->links = 0;
		result->triples = 0;
		namespaces_profiles[id] = result;
	} else {
		result = it->second;
	}
	return result;
}

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
		std::string predicate = std::string(buffer + ovector[2], ovector[3] - ovector[2]);
		// Second part indicates if it is a range or a domain
		std::string domain_or_range = std::string(buffer + ovector[4], ovector[5] - ovector[4]);
		// Last is the value
		std::string value = std::string(buffer + ovector[6], ovector[7] - ovector[6]);

		// Get the domain/range record for the predicate, or create it
		t_domains_and_ranges* record;
		t_domain_range_it it = domain_range.find(predicate);
		if (it == domain_range.end()) {
			record = new t_domains_and_ranges();
			domain_range[predicate] = record;
		} else {
			record = it->second;
		}

		// Record the domain or the range, as applicable
		if (domain_or_range == "www.w3.org/2000/01/rdf-schema#range") {
			record->ranges.insert(string_to_index(value, ranges_dict));
		} else if (domain_or_range == "www.w3.org/2000/01/rdf-schema#domain") {
			record->domains.insert(string_to_index(value, domains_dict));
		}
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

	// Content of a line
	std::string predicate;
	unsigned int identifier;

	// Parse the file
	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/raw/dictionary_predicate.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		if (buffer[0] == '#')
			continue;

		// Read the line
		std::stringstream ss(buffer);
		ss >> predicate >> identifier;

		// If we don't have domain/ranges about that predicate ignore it
		if (domain_range.find(predicate) == domain_range.end())
			continue;

		// Insert the predicate into the reversed dict
		predicates_dict[identifier] = predicate;
	}
	gzclose(handler);
	delete buffer;

	std::cout << "Loaded " << predicates_dict.size() << " entries from dict" << std::endl << std::flush;
}

/*
 *
 */
void load_internal_connections() {
	std::cout << "Add internal connections to the profiles" << std::endl;

	// Content of a line
	unsigned int name_id;
	unsigned int predicate_id;
	unsigned int count;

	// Parse the file
	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/raw/connections_internal.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		if (buffer[0] == '#')
			continue;

		// Read the line
		std::stringstream ss(buffer);
		ss >> name_id >> predicate_id >> count;

		// If we don't have domain/ranges about that predicate ignore it
		t_reverse_dict_it it = predicates_dict.find(predicate_id);
		if (it == predicates_dict.end())
			continue;

		// Get the domain and ranges
		std::string pred_str = it->second;
		t_domains_and_ranges* record = domain_range.find(pred_str)->second;

		// Get the relevant profile
		t_profile* profile = get_profile(name_id, true);

		// Append domains and ranges
		foreach ( unsigned int domain, record->domains)
					{
						t_map_it a = profile->domains->find(domain);
						unsigned int v = (a == profile->domains->end() ? 0 : a->second);
						(*profile->domains)[domain] = v + count;
					}
		foreach ( unsigned int range, record->ranges)
					{
						t_map_it a = profile->ranges->find(range);
						unsigned int v = (a == profile->ranges->end() ? 0 : a->second);
						(*profile->ranges)[range] = v + count;
					}

		profile->triples++;
	}
	gzclose(handler);
	delete buffer;
}

void load_other_connections() {
	std::cout << "Add connections between namespaces to the profiles" << std::endl;

	// Content of a line
	unsigned int start_id;
	unsigned int end_id;
	unsigned int pred_id;
	unsigned int count;

	// Parse the file
	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/raw/connections_inter_ns.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		if (buffer[0] == '#')
			continue;

		// Read the line
		std::stringstream ss(buffer);
		ss >> start_id >> end_id >> pred_id >> count;

		// If we don't have domain/ranges about that predicate ignore it
		t_reverse_dict_it it = predicates_dict.find(pred_id);
		if (it == predicates_dict.end())
			continue;

		// Get the domain and ranges
		std::string pred_str = it->second;
		//t_domains_and_ranges* record = domain_range.find(pred_str)->second;

		// Get the relevant profiles
		t_profile* start_profile = get_profile(start_id, true);
		//t_profile* end_profile = get_profile(end_id, true);
		//if (end_profile == NULL)
		//	continue;

		// Append domain of relation to range of start
		/*
		 foreach ( unsigned int domain, record->domains)
		 {
		 t_map_it a = start_profile->ranges->find(domain);
		 unsigned int v = (a == start_profile->ranges->end() ? 0 : a->second);
		 (*start_profile->ranges)[domain] = v + count;
		 }
		 start_profile->links++;
		 */
		 start_profile->triples++;

		/*
		 // Append range of relation to domain of end
		 foreach ( unsigned int range, record->ranges)
		 {
		 t_map_it a = end_profile->domains->find(range);
		 unsigned int v = (a == end_profile->domains->end() ? 0 : a->second);
		 (*end_profile->domains)[range] = v + count;
		 }
		 //end_profile->links++;
		 */

		// Store that edge into the network
		std::ostringstream edge_label;
		edge_label << start_id << " " << end_id;
		std::string edge = edge_label.str();
		t_dict_it it2 = network.find(edge);
		unsigned int v = (it2 == network.end() ? 0 : it2->second);
		network[edge] = v + 1;

	}
	gzclose(handler);
	delete buffer;
}

/*
 * Save the profiles of the nodes
 */
void save_files() {
	std::ofstream handle;
	t_set whitelist;

	std::cout << "Save profile of " << namespaces_profiles.size() << " nodes" << std::endl;
	handle.open("data/network/profiles.csv");
	handle << "# Domain/Range id predicate count" << std::endl;
	for (t_profile_it it = namespaces_profiles.begin(); it != namespaces_profiles.end(); it++) {
		if ((it->second->domains->size() == 0) && (it->second->ranges->size() == 0))
			continue;

		if (it->second->triples < 50)
			continue;

		whitelist.insert(it->first);
		for (t_map_it it2 = it->second->domains->begin(); it2 != it->second->domains->end(); it2++)
			handle << "D " << it->first << " " << it2->first << " " << it2->second << std::endl;
		for (t_map_it it2 = it->second->ranges->begin(); it2 != it->second->ranges->end(); it2++)
			handle << "R " << it->first << " " << it2->first << " " << it2->second << std::endl;
	}
	handle.close();
	std::cout << "Ok for " << whitelist.size() << " nodes" << std::endl;

	std::cout << "Save a network with " << network.size() << " edges" << std::endl;
	handle.open("data/network/network.csv");
	handle << "# start end count" << std::endl;
	unsigned int c = 0;
	for (t_dict_it it = network.begin(); it != network.end(); it++) {
		std::stringstream ss(it->first);
		unsigned int start_id;
		unsigned int end_id;
		ss >> start_id >> end_id;
		if ((whitelist.find(start_id) != whitelist.end()) && (whitelist.find(end_id) != whitelist.end())) {
			handle << it->first << " " << it->second << std::endl;
			c++;
		}
	}
	handle.close();
	std::cout << "Ok for " << c << " edges" << std::endl;

	std::cout << "Save range dictionary" << std::endl;
	handle.open("data/network/dictionary_ranges.csv");
	handle << "# resource index " << std::endl;
	for (t_dict_it it = ranges_dict.begin(); it != ranges_dict.end(); it++)
		handle << it->first << " " << it->second << std::endl;
	handle.close();

	std::cout << "Save domain dictionary" << std::endl;
	handle.open("data/network/dictionary_domains.csv");
	handle << "# resource index " << std::endl;
	for (t_dict_it it = domains_dict.begin(); it != domains_dict.end(); it++)
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
