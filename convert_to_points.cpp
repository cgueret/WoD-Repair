#include <iostream>
#include <fstream>
#include <sstream>
#include <google/sparse_hash_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include "zlib.h"
#include <math.h>
#include "common.h"
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH
#include <boost/lexical_cast.hpp>

#include "kmlocal-1.7.2/src/KMlocal.h"

// Convert some predicate text to a dim index
std::vector<std::string> dims;

typedef struct {
	unsigned int id;
	std::vector<double> data;
	std::vector<unsigned int> arcs;
} t_point;
typedef std::vector<t_point*> t_points;
t_points points;

unsigned int get_point(unsigned int id) {
	for (size_t i = 0; i < points.size(); ++i)
		if (points[i]->id == id)
			return i;
	return -1;
}

unsigned int get_dim(std::string& name) {
	for (size_t i = 0; i < dims.size(); ++i)
		if (dims[i] == name)
			return i;
	dims.push_back(name);
	return dims.size() - 1;
}

/*
 * Main executable part
 */
int main() {
	// Vars for a raw
	char type;
	unsigned int name_id;
	unsigned int predicate_id;
	double tfidf;

	std::ifstream inFile;

	// Count the dims
	inFile.open("data/network/profiles-tfidf.csv");
	while (!inFile.eof()) {
		inFile >> type >> name_id >> predicate_id >> tfidf;

		// Create the new dim if needed
		std::ostringstream tmp;
		tmp << type << " " << predicate_id;
		std::string label = tmp.str();
		get_dim(label);
	}
	inFile.close();
	std::cout << "Dimensions: " << dims.size() << std::endl;

	// Load the points
	inFile.open("data/network/profiles-tfidf.csv");
	while (!inFile.eof()) {
		inFile >> type >> name_id >> predicate_id >> tfidf;

		// Get the dim index
		std::ostringstream tmp;
		tmp << type << " " << predicate_id;
		std::string label = tmp.str();
		unsigned int dim = get_dim(label);

		// Get or create the data point
		t_point* point = NULL;
		for (size_t i = 0; i < points.size(); ++i)
			if (points[i]->id == name_id)
				point = points[i];
		if (point == NULL) {
			point = new t_point();
			point->id = name_id;
			point->data.resize(dims.size(), 0);
			points.push_back(point);
		}

		// Set the value
		point->data[dim] = tfidf;
	}
	inFile.close();
	std::cout << "Points: " << points.size() << std::endl;

	unsigned int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler;

	// Load the network
	unsigned int start;
	unsigned int end;
	unsigned int count;
	handler = gzopen("data/network/network.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		if (buffer[0] == '#')
			continue;

		// Read the line
		std::stringstream ss(buffer);
		ss >> start >> end >> count;

		// Get the corresponding points
		unsigned int start_point = get_point(start);
		unsigned int end_point = get_point(end);

		// Store connection
		points[start_point]->arcs.push_back(end_point);
	}
	gzclose(handler);

	// Load the dict
	t_reverse_dict d;
	handler = gzopen("data/network/dictionary_nodes.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		if (buffer[0] == '#')
			continue;

		// Read the line
		std::stringstream ss(buffer);
		std::string name;
		unsigned int id;
		ss >> name >> id;
		d[id] = name;
	}
	gzclose(handler);

	delete buffer;

	// Load the points into KML structure
	KMdata dataPts(dims.size(), points.size());
	for (size_t p = 0; p < points.size(); ++p)
		for (size_t i = 0; i < points[p]->data.size(); ++i)
			dataPts[p][i] = points[p]->data[i];
	dataPts.setNPts(points.size()); // set actual number of pts
	dataPts.buildKcTree(); // build filtering structure

	// Run clustering algorithm
	std::ofstream error;
	error.open("data/network/error.csv");
	for (int k = 1; k < 21; k++) {
		//	int k = 2; //26
		KMterm term(1000, 0, 0, 0, // run for 100 stages
				0.10, // min consec RDL
				0.10, // min accum RDL
				3, // max run stages
				0.50, // init. prob. of acceptance
				10, // temp. run length
				0.95); // temp. reduction factor
		KMfilterCenters ctrs(k, dataPts); // allocate centers
		KMlocalHybrid kmHybrid(ctrs, term);
		ctrs = kmHybrid.execute(); // go
		std::cout << "Number of classes (k):  " << k << "\n";
		std::cout << "Average distortion: " << ctrs.getDist(false) / double(ctrs.getNPts()) << "\n";
		KMctrIdxArray closeCtr = new KMctrIdx[dataPts.getNPts()];
		double* sqDist = new double[dataPts.getNPts()];
		ctrs.getAssignments(closeCtr, sqDist);

		// Compute the relative number of bad connections
		double c = 0;
		double tot = 0;
		for (size_t i = 0; i < points.size(); ++i) {
			for (size_t j = 0; j < points[i]->arcs.size(); ++j) {
				tot++;
				if (closeCtr[i] != closeCtr[(points[i]->arcs[j])])
					c++;
			}
		}
		std::cout << "Error: " << c / tot << "\n";

		// Compute clustering coefficient
		double cc = 0;
		for (size_t i = 0; i < points.size(); ++i) {
			double cc_node = 0;
			double cc_tot = 0;
			for (size_t n1 = 0; n1 < points[i]->arcs.size(); ++n1) {
				for (size_t n2 = n1 + 1; n2 < points[i]->arcs.size(); ++n2) {
					if (closeCtr[(points[i]->arcs[n1])] == closeCtr[i] && closeCtr[(points[i]->arcs[n1])] == closeCtr[(points[i]->arcs[n2])]) {
						cc_tot++;
						double aa = 0;
						for (size_t tmp = 0; tmp < points[n1]->arcs.size(); ++tmp)
							if (points[n1]->arcs[tmp] == n2)
								aa = 1;
						if (aa == 0)
							for (size_t tmp = 0; tmp < points[n2]->arcs.size(); ++tmp)
								if (points[n2]->arcs[tmp] == n1)
									aa = 1;
						cc_node += aa;
					}
				}
				if (cc_tot != 0)
					cc += cc_node / cc_tot;
			}
		}
		std::cout << "Clustering: " << cc / points.size() << "\n";

		error << k << " " << ctrs.getDist(false) / double(ctrs.getNPts()) << " " << c / tot << " " << cc
				/ points.size() << std::endl;

		// Write the pajek network
		std::ofstream handle;
		handle.open("data/network/lod-cloud-" + boost::lexical_cast<std::string>(k) + ".net");
		handle << "*Network lod-cloud-" << k << ".net" << std::endl;
		handle << "*Vertices " << points.size() << std::endl;
		for (size_t i = 0; i < points.size(); ++i)
			handle << "\t" << i + 1 << "\t\"" << d[points[i]->id] << "\"" << std::endl;
		handle << "*Arcs" << std::endl;
		for (size_t i = 0; i < points.size(); ++i)
			for (size_t j = 0; j < points[i]->arcs.size(); ++j)
				handle << "\t" << i + 1 << "\t" << (points[i]->arcs[j]) + 1 << "\t1.0" << std::endl;
		handle << "*Partition lod-cloud_profiles" << std::endl;
		handle << "*Vertices " << points.size() << std::endl;
		for (size_t i = 0; i < points.size(); ++i)
			handle << "\t" << closeCtr[i] + 1 << std::endl;
		handle.close();

		// Write an optimal network
		// In that net nodes are connected with others having the same profile
		handle.open("data/network/lod-cloud-" + boost::lexical_cast<std::string>(k) + "-opt.net");
		handle << "*Network lod-cloud-" << k << "-opt.net" << std::endl;
		handle << "*Vertices " << points.size() << std::endl;
		for (size_t i = 0; i < points.size(); ++i)
			handle << "\t" << i + 1 << "\t\"" << d[points[i]->id] << "\"" << std::endl;
		handle << "*Arcs" << std::endl;
		for (size_t i = 0; i < points.size(); ++i)
			for (size_t j = 0; j < points.size(); ++j)
				if ((i != j) && (closeCtr[i] == closeCtr[j]))
					handle << "\t" << i + 1 << "\t" << j + 1 << "\t1.0" << std::endl;
		handle << "*Partition lod-cloud_profiles" << std::endl;
		handle << "*Vertices " << points.size() << std::endl;
		for (size_t i = 0; i < points.size(); ++i)
			handle << "\t" << closeCtr[i] + 1 << std::endl;
		handle.close();
	}
	error.close();
}
