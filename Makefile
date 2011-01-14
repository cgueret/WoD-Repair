CXXFLAGS = -O3 -Wall -fmessage-length=0 -std=c++0x -pipe

LIBS_GEN = -lz -lpcre -lboost_thread-mt
OBJS_GEN = generate_raw_data.o
TARGET_GEN = generate_raw_data

LIBS_NET = -lz -lpcre
OBJS_NET = generate_network.o
TARGET_NET = generate_network

LIBS_PR = -lz -lpcre
OBJS_PR = process_network.o
TARGET_PR = process_network

$(TARGET_PR): $(OBJS_PR)
	$(CXX) -o $(TARGET_PR) $(OBJS_PR) $(LIBS_PR)
	
$(TARGET_NET): $(OBJS_NET)
	$(CXX) -o $(TARGET_NET) $(OBJS_NET) $(LIBS_NET)

$(TARGET_GEN): $(OBJS_GEN)
	$(CXX) -o $(TARGET_GEN) $(OBJS_GEN) $(LIBS_GEN)
	
all: $(TARGET_GEN) $(TARGET_NET) $(TARGET_PR)

clean:
	rm -f $(OBJS_GEN) $(TARGET_GEN) $(OBJS_NET) $(TARGET_NET) $(TARGET_PR) $(OBJS_PR)
