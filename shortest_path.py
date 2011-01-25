#!/usr/bin/python2
import networkx as nx

G=nx.read_pajek("/tmp/graph.net").to_undirected()
comps = [g for g in nx.connected_component_subgraphs(G)]
comp = comps[0]
apl = nx.average_shortest_path_length(G)
apl = 0
try:
    apl = nx.average_shortest_path_length(comp)
except ZeroDivisionError:
    pass
avg = float(0)
tot = 0
for g in nx.connected_component_subgraphs(G):
    try:
        avg = avg + nx.average_shortest_path_length(g)
        tot = tot + 1
    except ZeroDivisionError:
        pass
if tot != 0:
    avg = avg / tot
print("%f %f" % (apl,comp.number_of_nodes()) )
