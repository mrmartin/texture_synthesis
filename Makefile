UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CCFLAGS += -O2 -L/usr/X11R6/lib -lm -lpthread -lX11
endif
ifeq ($(UNAME_S),Darwin)
	CCFLAGS += -lm -lpthread -L/usr/X11R6/lib -lm -lpthread -lX11
endif
	
test: myexample.bin
	./myexample.bin cohen03_in.jpg #~/Datasets/Textures/numbers.jpg

render: myexample.bin
	./myexample.bin yellow-peppers256.o.jpg yellow-peppers-output.jpg #~/Datasets/Textures/numbers.jpg
	./myexample.bin olives256.o.jpg olives-output.jpg
	
	
myexample.bin: myexample.cpp CImg GridCut SubImage.cpp
	g++ myexample.cpp -o myexample.bin $(CCFLAGS) -I./GridCut/include -I.
	
GridCut:
	unzip -d GridCut GridCut-1.1.zip

ex1: GridCut
	g++ GridCut/examples/ex1-basic/ex1-basic.cpp -o ex1.bin -I./GridCut/include
	./ex1.bin

ex2: GridCut
	g++ GridCut/examples/ex2-segmentation/ex2-segmentation.cpp -o ex2.bin -I./GridCut/include
	cp GridCut/examples/ex2-segmentation/image.tga .
	cp GridCut/examples/ex2-segmentation/mask.tga .

CImg:
	git clone http://git.code.sf.net/p/cimg/source CImg
	
clean:
	rm -r GridCut
	rm ex1.bin myexample.bin output.png output_map.png output_repeating.png