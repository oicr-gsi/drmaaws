DRMAA_DIR ?= /opt/ogs2011.11/lib/linux-x64

drmaaws: $(wildcard *.cpp) $(wildcard *.hpp)
	$(CXX) -g -std=c++11 $(CPPFLAGS) $(wildcard *.cpp) -lSQLiteCpp -lsqlite3 -lpistache -ljsoncpp -lpthread -ldrmaa -lssl -lcrypto -Wl,-rpath=$(DRMAA_DIR) -o $@

clean:
	rm -f drmaaws

.PHONY: clean
