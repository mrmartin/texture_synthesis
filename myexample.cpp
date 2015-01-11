/*
  repeatable texture example
*/
#include "CImg/CImg.h"
using namespace cimg_library;
#include <cstdio>
#include <cmath>
#include <GridCut/GridGraph_2D_4C.h>
#include <string>
#include <cfloat>
#include <SubImage.cpp>


int main(int argc, char **argv)
{
	srand(time(NULL));
	unsigned short w,h;
	CImg<unsigned char> image(argv[1]);
	SubImage whole(&image);//creates reference to entire image
	SubImage copy = SubImage(whole);
	//return 0;
	int width=image.width();
	int height=image.height();
	//whole.print();

	SubImage left(&image,0,0,width/2,height);//creates reference to subimage
	//left.print();
	
	SubImage right(&image,width/2,0,width,height);//creates reference to subimage
	//right.print();
	
	int out_size=250;
	//make a canvas for the Image Quilting result
	SubImage tile(out_size,out_size);
	//tile.print();
	
	//put a random rectangle in the middle of the canvas
	int rect_size=60;
	//tile.paste_on(rect,0,0);
		
	//the first patch doesn't care about anything.
	//the second patch, until the last patch before the tile is filled, needs to know which pixels come from images and which ones don't. There needs to be a mask, and a way of finding cuts of arbitrary shape between the mask
	//once the tile is filled, any new patch won't need the mask, but will calculate the cut against everything.
	//for all these, the get and set functions need to be modulo arithmetic
	/*for (int i=0;i<250;i++){
		SubImage rect2(&image,rect_size);//creates random subimage of size rect_size*rect_size
		tile.fit_on(rect2,rand()%(out_size-rect_size),rand()%(out_size-rect_size));
	}*/
	
	tile.render_repeatable_texture(&image,rect_size,0,150,5);
	
	tile.save("output.png");
	tile.save_cut_map("output_map.png");
	
	tile.save(argv[2]);
	/*
	for (int i=0;i<10;i++){
		//SubImage rect(&image,rect_size/3);//fit bad cuts with small patches
		printf("press to continue\n");
		system("read");
		tile.fit_onto_overflow(&image,rect_size,100,5);
		tile.save("output.png");
	}*/

	/*SubImage corner_test(62,62);
	corner_test.paste_on(rect,0,0);
	corner_test.paste_on(rect,20,0);
	corner_test.paste_on(rect,0,20);
	corner_test.save("output_corner_before.png");
	SubImage rect3(&image,rect_size);//creates random subimage of size rect_size*rect_size
	corner_test.fit_on(rect3,20,20);
	
	corner_test.save("output_corner_after.png");*/
  return 0;
}
