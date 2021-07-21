CAPI_LIBRARY_PATH=../C_API
CAPI_INCLUDES=../C_API/include
	
all:
	@gcc -g main.c -o main -I$(CAPI_INCLUDES) -L$(CAPI_LIBRARY_PATH) -lflashdb_capi
	
run:
	@LD_LIBRARY_PATH=$(CAPI_LIBRARY_PATH):$$LD_LIBRARY_PATH ./main

clean:
	@rm -f main
	

#val:
#	valgrind --leak-check=yes ./main
#export LD_LIBRARY_PATH=../C_API:$LD_LIBRARY_PATH
#@LD_LIBRARY_PATH=$(CAPI_LIBRARY_PATH):$$LD_LIBRARY_PATH 
