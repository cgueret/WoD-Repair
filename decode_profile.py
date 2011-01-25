#!/usr/bin/python

import sys
import gzip

def main(file):
    # Load dictionaries
    dict = {}
    dict['D'] = {}
    dict['R'] = {}
    f = gzip.open('data/network/dictionary_domains.csv.gz', 'rb')
    for line in f.readlines():
        if line.decode("utf-8")[0] != '#':
            parts = line.decode("utf-8")[:-1].split(' ')
            dict['D'][parts[1]] = parts[0]
    f.close()
    f = gzip.open('data/network/dictionary_ranges.csv.gz', 'rb')
    for line in f.readlines():
        if line.decode("utf-8")[0] != '#':
            parts = line.decode("utf-8")[:-1].split(' ')
            dict['R'][parts[1]] = parts[0]
    f.close()
    
    f = open(file)
    for line in f:
        if line[0] != '#':
            parts = line[:-1].split(' ')
            print (parts[0], parts[1], dict[parts[1]][parts[2]], parts[3])
    pass

if __name__ == '__main__':
    if len(sys.argv) == 2:
        main(sys.argv[1])