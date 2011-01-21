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

// Profiles for the namespaces
typedef google::sparse_hash_map<unsigned int, unsigned int, std::tr1::hash<unsigned int> > t_map;
typedef struct __profile__ {
	t_set ids_raw_data;
	unsigned int final_id;
	std::string ns_name;
	unsigned int nb_triples;
	t_map domains;
	t_map ranges;
	t_map connections;
} t_profile;
typedef std::vector<t_profile*> t_profiles;
t_profiles profiles;
t_set valid_raw_ids;

// Store the domain and ranges of predicates
t_domain_range domain_range;

// Store the topology of the network as a dict "id_ns id_ns"->count
t_dict network;

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
unsigned int get_profile_index(unsigned int id) {
	for (size_t i = 0; i < profiles.size(); ++i)
		if (profiles[i]->ids_raw_data.find(id) != profiles[i]->ids_raw_data.end())
			return i;

	std::cout << "BUG! Non existent profile asked" << std::endl;
	return profiles.size() + 1;
}

/*
 * Loads the list of namespaces, including the white list
 */
void init() {
	// Load the white list
	std::string name;
	std::vector<std::string> white_list;
	std::ifstream inFile;
	inFile.open("data/raw/white_list.txt");
	while (!inFile.eof()) {
		inFile >> name;
		white_list.push_back(name);
	}
	inFile.close();
	std::cout << "Loaded " << white_list.size() << " namespaces from the white list" << std::endl;

	// Resize the profiles vector to the number of namespaces
	profiles.resize(white_list.size());
	for (size_t i = 0; i < profiles.size(); ++i) {
		profiles[i] = new t_profile();
		profiles[i]->ns_name = white_list[i];
		profiles[i]->nb_triples = 0;
		profiles[i]->final_id = 0;
	}

	// Load the network namespaces and prepare the profiles
	std::string ns;
	unsigned int id;
	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler = gzopen("data/raw/dictionary_namespace.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		if (buffer[0] == '#')
			continue;

		// Read the line
		std::stringstream ss(buffer);
		ss >> ns >> id;

		// Find the right profile
		unsigned int index = profiles.size() * 4;
		for (size_t i = 0; (i < profiles.size()) && (index > profiles.size()); ++i) {
			size_t l = ns.size();
			if (white_list[i].size() < l)
				l = white_list[i].size();
			if (ns.compare(0, l, white_list[i]) == 0)
				index = i;
		}

		// If no matching namespace has been found, ignore
		if (index > profiles.size())
			continue;

		// Append the raw data id to the list of ids for this namespace
		profiles[index]->ids_raw_data.insert(id);
		valid_raw_ids.insert(id);
	}

	size_t c = 0;
	for (size_t i = 0; i < profiles.size(); ++i)
		if (profiles[i]->ids_raw_data.size() > 0)
			c++;
	std::cout << "Found " << c << " profiles" << std::endl;
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

		// If that id does not match a profile, ignore
		if (valid_raw_ids.find(name_id) == valid_raw_ids.end())
			continue;

		// If we don't have domain/ranges about that predicate ignore it
		t_reverse_dict_it it = predicates_dict.find(predicate_id);
		if (it == predicates_dict.end())
			continue;

		// Get the domain and ranges
		std::string pred_str = it->second;
		t_domains_and_ranges* record = domain_range.find(pred_str)->second;

		// Get the relevant profile
		t_profile* profile = profiles[get_profile_index(name_id)];

		// Increment the triples
		profile->nb_triples++;

		// Append domains and ranges
		foreach ( unsigned int domain, record->domains)
					{
						t_map::const_iterator a = profile->domains.find(domain);
						unsigned int v = (a == profile->domains.end() ? 0 : a->second);
						profile->domains[domain] = v + count;
					}
		foreach ( unsigned int range, record->ranges)
					{
						t_map::const_iterator a = profile->ranges.find(range);
						unsigned int v = (a == profile->ranges.end() ? 0 : a->second);
						profile->ranges[range] = v + count;
					}
	}
	gzclose(handler);
	delete buffer;
}

/*
 *
 */
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

		// If that id does not match a profile, ignore the triple
		if (valid_raw_ids.find(start_id) == valid_raw_ids.end())
			continue;

		// Get the profile
		t_profile* profile = profiles[get_profile_index(start_id)];

		// Add domain/range if we have the information
		t_reverse_dict_it it = predicates_dict.find(pred_id);
		if (it != predicates_dict.end()) {
			// Get the domain and ranges
			t_domains_and_ranges* record = domain_range.find(it->second)->second;

			// Append domain of relation to range of start
			foreach ( unsigned int domain, record->domains)
						{
							t_map::const_iterator a = profile->ranges.find(domain);
							unsigned int v = (a == profile->ranges.end() ? 0 : a->second);
							profile->ranges[domain] = v + count;
						}
		}

		// If we don't know the destination, skip adding an edge
		if (valid_raw_ids.find(end_id) == valid_raw_ids.end())
			continue;

		// Add the edge to the existing edges
		unsigned int index_target = get_profile_index(end_id);
		t_map::const_iterator a = profile->connections.find(index_target);
		unsigned int v = (a == profile->connections.end() ? 0 : a->second);
		profile->connections[index_target] = v + count;
	}
	gzclose(handler);
	delete buffer;
}

/*
 * Save the profiles of the nodes
 */
void save_files() {
	std::ofstream handle;
	std::ofstream handle_profile;
	std::ofstream handle_network;

	// Examine all the profiles filter and assign final ids
	unsigned int final = 1;
	for (size_t i = 0; i < profiles.size(); ++i) {
		// Ignore the profile if we found no domain or range
		if ((profiles[i]->domains.size() == 0) && (profiles[i]->ranges.size() == 0))
			continue;

		// Assign a final id
		profiles[i]->final_id = final;
		final++;
	}

	std::cout << "Save nodes dictionary" << std::endl;
	handle.open("data/network/dictionary_nodes.csv");
	handle << "# Node index" << std::endl;
	for (size_t i = 0; i < profiles.size(); ++i)
		if (profiles[i]->final_id != 0)
			handle << profiles[i]->ns_name << " " << profiles[i]->final_id << std::endl;
	handle.close();

	std::cout << "Save range dictionary" << std::endl;
	handle.open("data/network/dictionary_ranges.csv");
	handle << "# Resource index " << std::endl;
	for (t_dict_it it = ranges_dict.begin(); it != ranges_dict.end(); it++)
		handle << it->first << " " << it->second << std::endl;
	handle.close();

	std::cout << "Save domain dictionary" << std::endl;
	handle.open("data/network/dictionary_domains.csv");
	handle << "# Resource index " << std::endl;
	for (t_dict_it it = domains_dict.begin(); it != domains_dict.end(); it++)
		handle << it->first << " " << it->second << std::endl;
	handle.close();

	std::cout << "Save the profile of the nodes and the network" << std::endl;
	handle_profile.open("data/network/profiles.csv");
	handle_profile << "# Domain/Range id predicate count" << std::endl;
	handle_network.open("data/network/network.csv");
	handle_network << "# Start end count" << std::endl;
	for (size_t i = 0; i < profiles.size(); ++i) {
		// Skip non validated profiles
		if (profiles[i]->final_id == 0)
			continue;

		// Write domain and range
		for (t_map::const_iterator it = profiles[i]->domains.begin(); it != profiles[i]->domains.end(); it++)
			handle_profile << "D " << profiles[i]->final_id << " " << it->first << " " << it->second << std::endl;
		for (t_map::const_iterator it = profiles[i]->ranges.begin(); it != profiles[i]->ranges.end(); it++)
			handle_profile << "R " << profiles[i]->final_id << " " << it->first << " " << it->second << std::endl;

		// Write connections if the target has been validated
		for (t_map::const_iterator it = profiles[i]->connections.begin(); it != profiles[i]->connections.end(); it++)
			if (profiles[it->first]->final_id != 0)
				handle_network << profiles[i]->final_id << " " << profiles[it->first]->final_id << " " << it->second
						<< std::endl;

	}
	handle_network.close();
	handle_profile.close();
}


/*
 * Main executable part
 */
int main() {
// Init all the data structures
init();

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
