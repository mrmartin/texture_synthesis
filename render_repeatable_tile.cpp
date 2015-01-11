/*
  render a repeatable texture - example
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
	printf("argc:%d\n",argc);
	CImg<unsigned char> image(argv[1]);
	
	//size of rendered texture:
	int out_size=128;
	//size of each random patch taken from the input texture
	int rect_size=30;//texel size!
	//max number of random patches to try at each position
	int tries=1500;
	//early stopping cut cost for a patch
	double min_cut = 5.0;
	//nuber of pixels by which patches will overlap
	int overlap_size=0;//zero means that the function will try to find its own
	
	SubImage tile(out_size,out_size);
	tile.render_repeatable_texture(&image,rect_size,overlap_size,tries,min_cut);
	
	tile.save(argv[2]);
	
  return 0;
}
