INSTALL_DIR=../

all: demtools 

demtools:
	g++ hillshade.cpp -lgdal -o hillshade
	g++ color-relief.cxx -lgdal -o color-relief
	g++ aspect.cpp -lgdal -o aspect
	g++ slope.cpp -lgdal -o slope

install:
	cp slope aspect color-relief hillshade ${INSTALL_DIR}

clean:
	rm -f slope aspect color-relief hillshade
