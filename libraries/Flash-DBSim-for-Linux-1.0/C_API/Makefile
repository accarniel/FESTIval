INCLUDES=./include
BIN=./bin/*
LIBRARY_PATH=../SO
LIBRARY_BIN=../SO/bin/*

$(shell mkdir -p bin)

all: FlashDBSim_capi
	@g++ -shared $(BIN) -o libflashdb_capi.so $(LIBRARY_BIN) -L$(LIBRARY_PATH) -lflashdb

FlashDBSim_capi:
	@g++ -c -fPIC -I$(INCLUDES) src/FlashDBSim_capi.cpp -L$(LIBRARY_PATH) -lflashdb
	@mv FlashDBSim_capi.o bin/

clean:
	@rm -f ./bin/*
	@rm -f libflashdb_capi.so
