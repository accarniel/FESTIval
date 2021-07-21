INCLUDES=./include
BIN=./bin/*

$(shell mkdir -p bin)

all: NandDevice01 NandDevice02 NandDevice03 NandDevice04 FTL01 stdafx FlashDBSim FlashDBSimSO
	@g++ -shared -o libflashdb.so $(BIN)
	
FlashDBSimSO:
	@g++ -c -fPIC -I$(INCLUDES) src/FlashDBSimSO.cpp
	@mv FlashDBSimSO.o bin/

NandDevice01:
	@g++ -c -fPIC -I$(INCLUDES) src/NandDevice01.cpp
	@mv NandDevice01.o bin/

NandDevice02:
	@g++ -c -fPIC -I$(INCLUDES) src/NandDevice02.cpp
	@mv NandDevice02.o bin/

NandDevice03:
	@g++ -c -fPIC -I$(INCLUDES) src/NandDevice03.cpp
	@mv NandDevice03.o bin/

NandDevice04:
	@g++ -c -fPIC -I$(INCLUDES) src/NandDevice04.cpp
	@mv NandDevice04.o bin/

FTL01:
	@g++ -c -fPIC -I$(INCLUDES) src/FTL01.cpp
	@mv FTL01.o bin/

FlashDBSim:
	@g++ -c -fPIC -I$(INCLUDES) src/FlashDBSim.cpp
	@mv FlashDBSim.o bin/

stdafx:
	@g++ -c -fPIC -I$(INCLUDES) src/stdafx.cpp
	@mv stdafx.o bin/

clean:
	@rm -f ./bin/*
	@rm -f libflashdb.so
