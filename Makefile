DRMAA_DIR ?= /opt/ogs2011.11/lib/linux-x64

drmaaws: $(wildcard *.cpp) $(wildcard *.hpp)
	c++ -g -std=c++11 $(wildcard *.cpp) -lSQLiteCpp -lsqlite3 -lpistache -ljsoncpp -lpthread -ldrmaa -Wl,-rpath=$(DRMAA_DIR) -o $@

clean:
	rm -f drmaaws

.PHONY: clean
