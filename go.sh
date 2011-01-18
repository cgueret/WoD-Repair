#!/bin/bash
rm -f data/network/dictionary_domains.csv.gz
rm -f data/network/dictionary_ranges.csv.gz
rm -f data/network/network.csv.gz
rm -f data/network/profiles.csv.gz
rm -f data/network/profiles-tfidf-clear.csv.gz  
rm -f data/network/profiles-tfidf.csv.gz
./generate_network
gzip data/network/dictionary_domains.csv
gzip data/network/dictionary_ranges.csv
gzip data/network/network.csv
gzip data/network/profiles.csv
./process_network
gzip data/network/profiles-tfidf-clear.csv
gzip data/network/profiles-tfidf.csv

