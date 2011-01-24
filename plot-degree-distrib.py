#!/usr/bin/python2

import matplotlib.pyplot as plt
import matplotlib
import networkx as nx

G=nx.read_pajek("data/network/lod-cloud.net")
degree_sequence=sorted(nx.degree(G).values(),reverse=True) 
dmax=max(degree_sequence)
plt.loglog(degree_sequence,'b-',marker='o')
plt.title("Degree rank plot")
plt.ylabel("degree")
plt.xlabel("rank")
plt.savefig("degree_histogram.png")
plt.show()

