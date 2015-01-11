/*
  create interchangeable patches - example
*/
#include "CImg/CImg.h"
using namespace cimg_library;
#include <cstdio>
#include <cmath>
#include <GridCut/GridGraph_2D_4C.h>
#include <string>
#include <cfloat>
#include <SubImage.cpp>
#include <sstream>      // std::stringstream

int main(int argc, char **argv)
{
	srand(time(NULL));
	unsigned short w,h;
	CImg<unsigned char> original(argv[1]);
	CImg<unsigned char> repeatable_i(argv[2]);
	SubImage patch_source(&original);//creates reference to entire image
	SubImage repeatable(&repeatable_i);//creates reference to entire image
	
	//render number_patches patches, and chose best_patches patches
	int number_patches = 20;
	SubImage * patches = new SubImage[number_patches];
	
	for (int i=0;i<number_patches;i++){
		patches[i] = repeatable.get_random_patch(patch_source,1,100,100);//this construct a grid as large as patch_source
		std::stringstream sstm;
		sstm << "patch_" << i << ".png";
		std::string tmp = sstm.str();
		char tmpc[1024];		
		patches[i].save(strcpy(tmpc, tmp.c_str()));
	}
	
	bool * saved = new bool [number_patches];
	for (int i=0;i<number_patches;i++)
		saved[i]=false;
	
	int best_patches = 3;
	while (best_patches>0){
		double lowest_cut_score=DBL_MAX;
		int best_patch;
		//find best, save it, mark it as saved
		for(int i=0;i<number_patches;i++)
			if(!saved[i])
				if(patches[i].patch_cut_cost<lowest_cut_score){
					lowest_cut_score=patches[i].patch_cut_cost;
					best_patch=i;
				}
		std::stringstream sstm;
		sstm << "patch_" << best_patches << ".png";
		std::string tmp = sstm.str();
		char tmpc[1024];		
		patches[best_patch].save(strcpy(tmpc, tmp.c_str()));
		saved[best_patch]=true;
		
		best_patches--;
	}
//	interchangeable_patch.save(argv[3]);
	//interchangeable_patch.save_known_map(argv[4]);
	
  return 0;
}
