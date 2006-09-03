INSTALL_DIR=/usr/local/bin/

all: demtools 

demtools:
	g++ hillshade.cpp -lgdal -o bin/hillshade
	g++ color-relief.cpp -lgdal -o bin/color-relief
	g++ aspect.cpp -lgdal -o bin/aspect
	g++ slope.cpp -lgdal -o bin/slope

install:
	cp bin/slope bin/aspect bin/color-relief bin/hillshade ${INSTALL_DIR}

clean:
	cd bin
        rm -f slope aspect color-relief hillshade
