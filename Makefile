CC=arm-linux-gnueabihf-g++
READELF=arm-linux-gnueabihf-readelf
CFLAGS=-Wall
INCLUDES=-I./inc
LFLAGS=
LIBS=-lpthread
LIBS+=-lrt
LIBS+=-lcgicc

SOURCES=main.cpp
OUTPUT=amu
OUTPUT_CGI=$(OUTPUT).cgi
OUTPUT_MAP=$(OUTPUT).map
SOURCES_DIR= src/
OBJECTS_DIR= obj/
OUTPUT_DIR= out/
#OUTPUT_CGI_DIR= /var/www/cgi/

OBJECTS=$(SOURCES:.cpp=.o)
SOURCES_ALL=$(addprefix $(SOURCES_DIR), $(SOURCES))
OBJECTS_ALL=$(addprefix $(OBJECTS_DIR), $(OBJECTS))
OUTPUT_ALL=$(addprefix $(OUTPUT_DIR), $(OUTPUT))
MAP_ALL=$(addprefix $(OUTPUT_DIR), $(OUTPUT_MAP))
#OUTPUT_CGI_ALL=$(addprefix $(OUTPUT_CGI_DIR), $(OUTPUT_CGI))


.PHONY: build rebuild release debug clean

release: CFLAGS+=-O3
build: CFLAGS+=-O2
debug: CFLAGS+=-O0 -g3
#build: $(SOURCES_ALL) $(OUTPUT_ALL)
build: $(MAP_ALL)

rebuild: clean build 
release: clean build
debug: clean build

$(OUTPUT_ALL): $(OBJECTS_ALL) 
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJECTS_ALL) -o $(OUTPUT_ALL) $(LFLAGS) $(LIBS)
#	$(CC) $(CFLAGS) $(INCLUDES) $(OBJECTS_ALL) -o $(OUTPUT_CGI_ALL) $(LFLAGS) $(LIBS)

$(OBJECTS_DIR)%.o: $(SOURCES_DIR)%.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -o $@ 

$(MAP_ALL): $(OUTPUT_ALL)
	$(READELF) -aW $^ > $@

clean:
	@echo cleaning all...
	@rm -f $(OBJECTS_ALL) $(OUTPUT_ALL)

