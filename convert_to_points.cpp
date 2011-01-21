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

#include "kmlocal-1.7.2/src/KMlocal.h"

// Convert some predicate text to a dim index
t_dict pred_to_dim;

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

/*
 * Main executable part
 */
int main() {
	// Vars for a raw
	char type;
	unsigned int name_id;
	unsigned int predicate_id;
	double tfidf;

	int buffer_size = 5 * 1024 * 1024;
	char* buffer = new char[buffer_size];
	gzFile handler;

	// Count the dims
	handler = gzopen("data/network/profiles-tfidf.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		if (buffer[0] == '#')
			continue;

		// Read the line
		std::stringstream ss(buffer);
		ss >> type >> name_id >> predicate_id >> tfidf;
		if ((type != 'D') && (type != 'R'))
			continue;

		// Create the new dim if needed
		std::ostringstream tmp;
		tmp << type << " " << predicate_id;
		std::string label = tmp.str();
		if (pred_to_dim.find(label) == pred_to_dim.end())
			pred_to_dim[label] = pred_to_dim.size();
	}
	gzclose(handler);
	std::cout << "Dimensions: " << pred_to_dim.size() << std::endl;

	// Load the points
	handler = gzopen("data/network/profiles-tfidf.csv.gz", "r");
	while (gzgets(handler, buffer, buffer_size) != NULL) {
		if (buffer[0] == '#')
			continue;

		// Read the line
		std::stringstream ss(buffer);
		ss >> type >> name_id >> predicate_id >> tfidf;
		if ((type != 'D') && (type != 'R'))
			continue;

		// Get the dim index
		std::ostringstream tmp;
		tmp << type << " " << predicate_id;
		unsigned int dim = pred_to_dim[tmp.str()];

		// Get or create the data point
		t_point* point = NULL;
		for (size_t i = 0; i < points.size(); ++i)
			if (points[i]->id == name_id)
				point = points[i];
		if (point == NULL) {
			point = new t_point();
			point->id = name_id;
			point->data.resize(pred_to_dim.size(), 0);
			points.push_back(point);
		}

		// Set the value
		point->data[dim] = tfidf;
	}
	std::cout << "Points: " << points.size() << std::endl;

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
	delete buffer;

	// Load the points into KML structure
	KMdata dataPts(pred_to_dim.size(), points.size());
	for (size_t p = 0; p < points.size(); ++p)
		for (size_t i = 0; i < points[p]->data.size(); ++i)
			dataPts[p][i] = points[p]->data[i];
	dataPts.setNPts(points.size()); // set actual number of pts
	dataPts.buildKcTree(); // build filtering structure

	// Run clustering algorithm
	int k = 6; //26
	KMterm term(500, 0, 0, 0, // run for 100 stages
			0.10, // min consec RDL
			0.10, // min accum RDL
			3, // max run stages
			0.50, // init. prob. of acceptance
			10, // temp. run length
			0.95); // temp. reduction factor
	KMfilterCenters ctrs(k, dataPts); // allocate centers
	KMlocalHybrid kmHybrid(ctrs, term);
	ctrs = kmHybrid.execute(); // go
	std::cout << "k =  " << k << "\n";
	std::cout << "Number of stages: " << kmHybrid.getTotalStages() << "\n";
	std::cout << "Average distortion: " << ctrs.getDist(false) / double(ctrs.getNPts()) << "\n";
	KMctrIdxArray closeCtr = new KMctrIdx[dataPts.getNPts()];
	double* sqDist = new double[dataPts.getNPts()];
	ctrs.getAssignments(closeCtr, sqDist);

	// Write the pajek network
	std::ofstream handle;
	handle.open("data/network/lod-cloud.net");
	handle << "*Network lod-cloud.net" << std::endl;
	handle << "*Vertices " << points.size() << std::endl;
	for (size_t i = 0; i < points.size(); ++i)
		handle << "\t" << i + 1 << "\t" << points[i]->id << std::endl;
	handle << "*Arcs" << std::endl;
	for (size_t i = 0; i < points.size(); ++i)
		for (size_t j = 0; j < points[i]->arcs.size(); ++j)
			handle << "\t" << i + 1 << "\t" << (points[i]->arcs[j]) + 1 << " 1.0" << std::endl;
	handle << "*Partition lod-cloud_profiles" << std::endl;
	handle << "*Vertices " << points.size() << std::endl;
	for (size_t i = 0; i < points.size(); ++i)
		handle << "\t" << closeCtr[i]+1 << std::endl;
	handle.close();
}
