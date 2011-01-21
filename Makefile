CXXFLAGS = -O3  -Wall -fmessage-length=0 -std=c++0x -pipe

LIBS_GEN = -lz -lpcre -lboost_thread-mt
OBJS_GEN = generate_raw_data.o
TARGET_GEN = generate_raw_data

LIBS_NET = -lz -lpcre
OBJS_NET = generate_network.o
TARGET_NET = generate_network

LIBS_PR = -lz -lpcre -lm
OBJS_PR = process_network.o
TARGET_PR = process_network

LIBS_C = kmlocal-1.7.2/src/KCtree.o kmlocal-1.7.2/src/KCutil.o kmlocal-1.7.2/src/KM_ANN.o kmlocal-1.7.2/src/KMcenters.o kmlocal-1.7.2/src/KMdata.o kmlocal-1.7.2/src/KMeans.o kmlocal-1.7.2/src/KMfilterCenters.o kmlocal-1.7.2/src/KMlocal.o kmlocal-1.7.2/src/KMrand.o kmlocal-1.7.2/src/KMterm.o -lz -lpcre -lm 
OBJS_C = convert_to_points.o
TARGET_C = convert_to_points

$(TARGET_C): $(OBJS_C)
	$(CXX) -o $(TARGET_C) $(OBJS_C) $(LIBS_C)
	
$(TARGET_PR): $(OBJS_PR)
	$(CXX) -o $(TARGET_PR) $(OBJS_PR) $(LIBS_PR)
	
$(TARGET_NET): $(OBJS_NET)
	$(CXX) -o $(TARGET_NET) $(OBJS_NET) $(LIBS_NET)

$(TARGET_GEN): $(OBJS_GEN)
	$(CXX) -o $(TARGET_GEN) $(OBJS_GEN) $(LIBS_GEN)
	
all: $(TARGET_GEN) $(TARGET_NET) $(TARGET_PR) $(TARGET_C)

clean:
	rm -f $(OBJS_GEN) $(TARGET_GEN) $(OBJS_NET) $(TARGET_NET) $(TARGET_PR) $(OBJS_PR) $(TARGET_C) $(OBJS_C)
