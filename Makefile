######
#
# Makefile to install and build demtools
# Warning: check hardcoded paths
#
######

CPP=g++
GDAL_LIB=-lgdal

default: compile

compile:
	@echo "Demtools compilation ..."     
	${CPP} hillshade.cpp ${GDAL_LIB} -o bin/hillshade
	${CPP} color-relief.cpp ${GDAL_LIB} -o bin/color-relief
	${CPP} aspect.cpp ${GDAL_LIB} -o bin/aspect
	${CPP} slope.cpp ${GDAL_LIB} -o bin/slope
	@echo "Finished compilation: `date`" 

clean:
	@echo "Cleaning ... "
	rm -rf bin/*

install:
	@echo "Installing ... "
	cp bin/slope bin/aspect bin/color-relief bin/hillshade /usr/local/bin/ 
