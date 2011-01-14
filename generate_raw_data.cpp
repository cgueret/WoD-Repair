//============================================================================
// Name        : generate_raw_data.cpp
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
#include <boost/thread.hpp>
#include <vector>

#include "common.h"

// Store the topology of the network as a dict "id_s id_o id_pred"->count
t_dict network;

// Count internal use of predicates "id_ns id_pred"-> count
t_dict innerloops;

// Namespaces and predicates dictionary as dicts string->id
t_dict namespaces_dict;
t_dict predicates_dict;

// The regular expression used to match triples and related stuff
#define CHUNK 524288
#define OVECCOUNT 30

/*
 * Return the index associated to a given namespace
 */
boost::mutex increase_counter_lock;
void increase_counter(std::string label, t_dict& dict) {
	increase_counter_lock.lock();
	unsigned int count = 0;
	t_dict_it it = dict.find(label);
	if (it != dict.end())
		count = it->second;
	dict[label] = count + 1;
	increase_counter_lock.unlock();
}

/*
 * Return the index associated to a given namespace
 */
boost::mutex string_to_index_lock;
unsigned int string_to_index(std::string label, t_dict& dict) {
	string_to_index_lock.lock();

	unsigned int index = 0;
	t_dict_it it = dict.find(label);

	if (it == dict.end()) {
		index = dict.size() + 1;
		dict[label] = index;
	} else {
		index = it->second;
	}

	string_to_index_lock.unlock();
	return index;
}


/*
 * Process a triple line
 */
void process_line(char* line, pcre* triple_re, int* ovector) {
	// Try to match the pattern
	int rc = pcre_exec(triple_re, NULL, line, strlen(line), 0, 0, ovector, OVECCOUNT);
	if (rc != 4)
		return;

	// Get subject information
	char* s_start = line + ovector[2];
	int s_length = ovector[3] - ovector[2];
	unsigned int s_id = string_to_index(std::string(s_start, get_namespace_end(s_start, s_length)), namespaces_dict);

	// Get predicate information
	char* p_start = line + ovector[4];
	int p_length = ovector[5] - ovector[4];
	unsigned int p_id = string_to_index(std::string(p_start, p_length), predicates_dict);

	// Get object information
	char* o_start = line + ovector[6];
	int o_length = ovector[7] - ovector[6];
	unsigned int o_id = string_to_index(std::string(o_start, get_namespace_end(o_start, o_length)), namespaces_dict);

	// Record the connection
	std::ostringstream edge_label;
	if (s_id == o_id) {
		// Internal connection
		edge_label << s_id << " " << p_id;
		increase_counter(edge_label.str(), innerloops);
	} else {
		// Cross-namespaces connection
		edge_label << s_id << " " << o_id << " " << p_id;
		increase_counter(edge_label.str(), network);
	}
}

/*
 * Process a compressed file with ntriples / nquads in it
 */
boost::mutex out_lock;
void process_file(const std::string file_name) {
	// Define the regexp to use to match the triples
	const char *error;
	int errpos;
	int ovector[OVECCOUNT];
	pcre *triple_re;
	triple_re = pcre_compile("^<https?://([^>]+)> <https?://([^>]+)> <https?://([^>]+)>.*$", 0, &error, &errpos, NULL);

	int buffer_size = 5 * 1024 * 1024;
	time_t now;
	time(&now);
	struct tm * ptm = localtime(&now);
	out_lock.lock();
	std::cout << "[" << ptm->tm_hour << ":" << ptm->tm_min << "." << ptm->tm_sec << "] now processing " << file_name
			<< std::endl;
	out_lock.unlock();

	char* buffer = new char[buffer_size];
	gzFile handler = gzopen(file_name.c_str(), "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		process_line(buffer, triple_re, ovector);
	}
	gzclose(handler);
	delete buffer;

	pcre_free(triple_re);
}

/*
 *
 */
void save_files() {
	// Local vars
	std::ofstream handle;
	t_dict_it it;

	// Write dictionary namespaces
	handle.open("dictionary_namespace.csv");
	handle << "# namespace id" << std::endl;
	for (it = namespaces_dict.begin(); it != namespaces_dict.end(); it++)
		handle << it->first << " " << it->second << std::endl;
	handle.close();

	// Write dictionary namespaces
	handle.open("dictionary_predicate.csv");
	handle << "# predicate id" << std::endl;
	for (it = predicates_dict.begin(); it != predicates_dict.end(); it++)
		handle << it->first << " " << it->second << std::endl;
	handle.close();

	// Write inner connections
	handle.open("connections_internal.csv");
	handle << "# start predicate occurrences" << std::endl;
	for (it = innerloops.begin(); it != innerloops.end(); it++)
		handle << it->first << " " << it->second << std::endl;
	handle.close();

	// Write cross namespaces connections
	handle.open("connections_inter_ns.csv");
	handle << "# start end predicate occurrences" << std::endl;
	for (it = network.begin(); it != network.end(); it++)
		handle << it->first << " " << it->second << std::endl;
	handle.close();
}

/*
 * Main executable part
 */
int main() {
	size_t max_running_tasks = 4;
	std::vector<boost::thread> tasks;

	// Load the index
	std::cout << "Load index" << std::endl;
	std::vector<std::string> files;
	std::ifstream inFile;
	inFile.open("/var/data/INDEX");
	if (!inFile.is_open())
		return -1;
	while (!inFile.eof()) {
		// Process a chunk
		std::string fileName;
		inFile >> fileName;
		fileName = "/var/data/" + fileName;
		if (fileName.size() > 20)
			files.push_back(fileName);
	}
	inFile.close();

	std::cout << "Start processing files in index" << std::endl;
	while (!files.empty()) {
		// Clear process queue
		tasks.clear();

		// Add new tasks
		while (!files.empty() && tasks.size() != max_running_tasks) {
			tasks.push_back(boost::thread(process_file, files.back()));
			files.pop_back();
		}

		// Wait
		for (size_t i = 0; i < tasks.size(); i++)
			tasks[i].join();
		save_files();
	}
	std::cout << "End!" << std::endl;

	return 0;
}
