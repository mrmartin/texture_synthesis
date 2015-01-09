#include "CImg/CImg.h"
using namespace cimg_library;
#include <cstdio>
#include <cmath>
#include <GridCut/GridGraph_2D_4C.h>
#include <string>
#include <cfloat>


//can't be DBL_MAX because I need to add and subtract several of them from a double.
const double arbitrary_large_constant = 100000;

typedef GridGraph_2D_4C<double,double,double> Grid;

int min(int a, int b){
	return a<b?a:b;
}

double gaussian2D(int x, int y, double sigma_squared){
	return exp(-((x*x)/(2*sigma_squared)+(y*y)/(2*sigma_squared)));
}

class SubImage {
  	public:
			int w, h, x1, y1, x2, y2;
			CImg<unsigned char> real_image;
			CImg<unsigned char> * image;
			bool * known_mask;
			double * cut_cost_mask;
	//make subImage in a known image a random square, without overflowing over borders		
	SubImage(CImg<unsigned char> * passed_image, int rect_size)
	{
		image=passed_image;
		CImg<unsigned char> real_image;
		real_image = *image;
		w=rect_size;
		h=rect_size;
		int randx=rand()%(real_image.width()-rect_size);
		int randy=rand()%(real_image.height()-rect_size);
		
		x1=randx;
		y1=randy;
		x2=randx+rect_size;
		y2=randy+rect_size;
		//printf("the random square is taken from image(%d, %d, %d, %d)\n",randx,randy,randx+rect_size,randy+rect_size);
		known_mask = new bool[w*h];
		cut_cost_mask = new double[w*h];
		for (int i=0;i<w;i++)
			for (int j=0;j<h;j++){
				set_known(i,j);
				set_cut_cost(i,j,-1);
			}
	}
	//make entire image a subImage object
	SubImage(CImg<unsigned char> * passed_image)
	{
		image=passed_image;
		CImg<unsigned char> real_image;
		real_image = *image;
		w=real_image.width();
		h=real_image.height();
		x1=0;
		y1=0;
		x2=w;
		y2=h;
		known_mask = new bool[w*h];
		cut_cost_mask = new double[w*h];
		for (int i=0;i<w;i++)
			for (int j=0;j<h;j++){
				set_known(i,j);
				set_cut_cost(i,j,-1);
			}
	}
	//make subImage a rectangle in an existing image
	SubImage(CImg<unsigned char> * passed_image, int x_from, int y_from, int x_to, int y_to)
	{
		image=passed_image;
		CImg<unsigned char> real_image;
		real_image = *image;
		w=x_to-x_from;
		h=y_to-y_from;
		x1=x_from;
		y1=y_from;
		x2=x_to;
		y2=y_to;
		known_mask = new bool[w*h];
		cut_cost_mask = new double[w*h];
		for (int i=0;i<w;i++)
			for (int j=0;j<h;j++){
				set_known(i,j);
				set_cut_cost(i,j,-1);
			}
	}
	//make blank subImage
	SubImage(int width, int height)
	{
		real_image = CImg<unsigned char>(width,height,1,3,0);
		image=&real_image;
		x1=0;
		y1=0;
		x2=width;
		y2=height;
		w=width;
		h=height;
		known_mask = new bool[w*h];
		cut_cost_mask = new double[w*h];
		for (int i=0;i<w;i++)
			for (int j=0;j<h;j++){
				set_unknown(i,j);
				set_cut_cost(i,j,-1);
			}
	}
	//randomly take samples from passed_image of size patch_size x patch_size, and fit them onto the canvas until it's full
	bool render_texture(CImg<unsigned char> * passed_image, int patch_size, int overlap)
	{
		//is the passed image smaller than the desired patch?
		if((*passed_image).width()<patch_size || (*passed_image).height()<patch_size){
			printf("patch image is smaller than patch");
			return false;
		}			
		SubImage patch(passed_image,patch_size);
		//is patch size less than this image?
		if(patch.w>w || patch.h>h){
			printf("patch size is more than image size in render_texture\n");
			return false;
		}
		//is overlap more than patch size?
		if(patch.w<=overlap || patch.h<=overlap){
			printf("overlap is more or equal to patch size\n");
			return false;
		}
		//put the patch in the top left corner
		//paste_on(patch,0,0);
		//for each collumn
		for (int y=0;y<h;y+=patch.h-overlap){
			int y_offset=0;
			if(y+patch.h>h)
				y_offset=-y-patch.h+h;
			//for each row, place patches
			for (int x=0;x<w;x+=patch.w-overlap){
				printf("%d, ",x);
				patch = SubImage(passed_image,patch_size);
				//will the patch overlap?
				int x_offset=0;
				if(x+patch.w>w)
					x_offset=-x-patch.w+w;
				fit_on(patch,x+x_offset,y+y_offset);
			}
		}
	}

	//this function creates a texture such that the left matches the right and the top matches the bottom
	bool render_repeatable_texture(CImg<unsigned char> * passed_image, int patch_size, int overlap, int max_itterations, double cutoff_cost)
	{
		//automatically make a good estimate for overlap, such that it works over the edges
		if(overlap==0){
			//assume that the width equals the height
			int highest_mod = 0;
			for (int i=patch_size-1;i>patch_size/4;i--){
				if(highest_mod<w%(patch_size-i)){
					overlap=i;
					highest_mod=w%(patch_size-i);
				}
			}
			printf("automatically chose best overlap to be %d\n",overlap);
		}
		//is the passed image smaller than the desired patch?
		if((*passed_image).width()<patch_size || (*passed_image).height()<patch_size){
			printf("patch image is smaller than patch");
			return false;
		}	
		SubImage patch(passed_image,patch_size);
		//is patch size less than this image?
		if(patch.w>w || patch.h>h){
			printf("patch size is more than image size in render_texture\n");
			return false;
		}
		//is overlap more than patch size?
		if(patch.w<=overlap || patch.h<=overlap){
			printf("overlap is more or equal to patch size\n");
			return false;
		}
		printf("repeatable texture rendering will have overlap %d, but the overlap over the edge will be different!\n",overlap);
		//it was actually much easier to do it properly with the overlaps, than it was to fix the bugs of the other method
		//for each collumn
		for (int y=0;y<h;y+=patch.h-overlap){
			int y_offset=0;
			if(y+patch.h>h+overlap)
				y_offset=-y-patch.h+h+overlap;
			//for each row, place patches
			for (int x=0;x<w;x+=patch.w-overlap){
				//printf("%d, ",x);
				//will the patch overlap?
				int x_offset=0;
				if(x+patch.w>w+overlap)
					x_offset=-x-patch.w+w+overlap;

				patch = SubImage(passed_image,patch_size);
				SubImage best_patch = patch;
				double best_cost=DBL_MAX;
				double this_cost=get_cut_cost_overflow(patch,x+x_offset,y+y_offset);
				int counter = max_itterations;
				
				while(best_cost>cutoff_cost && counter>0){
					counter--;
					if(best_cost>this_cost){
						best_patch = patch;
						best_cost = this_cost;
					}else{
						patch = SubImage(passed_image,patch_size);
						this_cost=get_cut_cost_overflow(patch,x+x_offset,y+y_offset);
					}
				}
				printf("best flow is %f\n",best_cost);
					
				fit_on_overflow(best_patch,x+x_offset,y+y_offset);
				
				save("output.png");
				//system("read");
			}
		}
	}
	
	bool is_known(int x, int y){
		if(x<0 || y<0 || x>=w || y>=h){
			//printf("checking known array at %d, %d for subImage of size %d, %d\n",x,y,w,h);
			return false;
		}else{
			return known_mask[x*h+y];
		}
	}
	bool set_known(int x, int y){
		if(x<0 || y<0 || x>=w || y>=h){
			printf("setting known array outside subImage\n");
			return false;
		}else{
			known_mask[x*h+y]=true;
			return true;
		}
	}
	bool set_unknown(int x, int y){
		if(x<0 || y<0 || x>=w || y>=h){
			printf("unsetting known array outside subImage\n");
			return false;
		}else{
			known_mask[x*h+y]=false;
			return true;
		}
	}
	double get_cut_cost(int x, int y){
		if(x<0 || y<0 || x>=w || y>=h){
			//printf("checking known array at %d, %d for subImage of size %d, %d\n",x,y,w,h);
			return false;
		}else{
			return cut_cost_mask[x*h+y];
		}
	}
	bool set_cut_cost(int x, int y, double cost){
		if(x<0 || y<0 || x>=w || y>=h){
			printf("setting cut cost array outside subImage\n");
			return false;
		}else{
			cut_cost_mask[x*h+y]=cost;
			return true;
		}
	}
	
	unsigned char get(int x, int y, int c){
		if(x<0 || x>=w || y<0 || y>=h || c<0 || c>2){
			printf("in get, getting information outside of SubImage (x:%d, y:%d, w:%d, h:%d, c:%d)! ",x,y,w,h,c);
			return 0;
		}else{
			return (*image)(x1+x,y1+y,0,c);
		}
	}
	bool set(int x, int y, int c, unsigned char v){
		if(x<0 || x>=w || y<0 || y>=h || c<0 || c>2){
			printf("setting information outside of SubImage");
			return false;
		}else{
			set_known(x,y);
			(*image)(x1+x, y1+y, 0, c)=v;
			return true;
		}
	}
	double get_cut_cost_overflow(int x, int y){
		if(x<0 || y<0){
			//printf("checking known array at %d, %d for subImage of size %d, %d\n",x,y,w,h);
			return false;
		}else{
			return cut_cost_mask[(x%w)*h+(y%h)];
		}
	}
	bool set_cut_cost_overflow(int x, int y, double cost){
		if(x<0 || y<0){
			//printf("setting cut cost array outside subImage\n");
			//don't allow negatives
			return false;
		}else{
			cut_cost_mask[(x%w)*h+(y%h)]=cost;
			return true;
		}
	}
	unsigned char get_overflow(int x, int y, int c){
		if(x<0 || y<0 || c<0 || c>2){
			printf("in get_overflow, getting information outside of SubImage! ");
			return 0;
		}else{
			return (*image)(x1+x%w,y1+y%h,0,c);
		}
	}
	bool set_overflow(int x, int y, int c, unsigned char v){
		if(x<0 || y<0 || c<0 || c>2){
			printf("setting information below 0 of SubImage");
			return false;
		}else{
			set_known_overflow(x,y);
			(*image)(x1+x%w, y1+y%h, 0, c)=v;
			return true;
		}
	}
	bool is_known_overflow(int x, int y){
		if(x<0 || y<0){
			//printf("checking known array at %d, %d for subImage of size %d, %d\n",x,y,w,h);
			//don't allow checking below 0
			return false;
		}else{
			return known_mask[(x%w)*h+(y%h)];
		}
	}
	bool set_known_overflow(int x, int y){
		if(x<0 || y<0){
			//printf("setting known array outside subImage\n");
			//don't allow to set negative values
			return false;
		}else{
			known_mask[(x%w)*h+(y%h)]=true;
			return true;
		}
	}
	bool ok(){
		bool ok=true;
		if(x2>x1 && y2>y1)
			return ok;
		else
			return !ok;
	}
	void print(){
		printf("%s: %d, %d\n",ok()?"ok":"not_ok",w,h);
	}
	//this function pastes patch onto image, putting the corner of patch at (at_x, at_y)
	bool paste_on(SubImage patch, int at_x, int at_y){
		printf("putting patch of size %d x %d at (%d, %d) into an image of size %d x %d\n",patch.w, patch.h,at_x,at_y,w,h);
		//first, check if all the values are sensible
		if(at_x<0 || at_y<0){
			printf("at is smaller than 0\n");
			return false;
		}
		if(at_x+patch.w>w || at_y+patch.h>h){
			printf("patch goes beyond image\n");
			return false;
		}
		for (int x=at_x;x<at_x+patch.w;x++)
			for (int y=at_y;y<at_y+patch.h;y++)
				for (int c=0;c<3;c++)
					set(x,y,c,patch.get(x-at_x,y-at_y,c));
		return true;
	}
	//this function pastes patch onto image, putting the corner of patch at (at_x, at_y)
	bool paste_subimage(SubImage patch, int at_x, int at_y, int from_x, int from_y, int to_x, int to_y){
		//printf("putting subimage (%d, %d - %d, %d) of patch of size %d x %d at (%d, %d) into an image of size %d x %d\n",from_x,from_y,to_x,to_y,patch.w, patch.h,at_x,at_y,w,h);
		//first, check if all the values are sensible
		if(at_x<0 || at_y<0){
			printf("at is smaller than 0\n");
			return false;
		}
		if(to_x<from_x || to_y<from_y){
			printf("width or height of patch is smaller than 0\n");
			return false;
		}
		if(to_x-from_x>w-at_x || to_y-from_y>h-at_y){
			printf("patch goes beyond image\n");
			return false;
		}
		for (int x=from_x;x<to_x;x++)
			for (int y=from_y;y<to_y;y++)
				for (int c=0;c<3;c++)
					set(x-from_x+at_x,y-from_y+at_y,c,patch.get(x,y,c));
		return true;
	}
	//without telling it where to put the patch, it will put it over the worst cut
	bool fit_onto_overflow(SubImage patch){
		int worst_x=0;
		int worst_y=0;
		double worst_cut=0;
		for (int x=0;x<w;x++)
			for (int y=0;y<h;y++)
				if(worst_cut<get_cut_cost(x,y)){
					worst_x=x;
					worst_y=y;
					worst_cut=get_cut_cost(x,y);
				}
		printf("worst cut found was %.2f\n",worst_cut);
		fit_onto_overflow(patch,worst_x-(patch.w/2)+w,worst_y-(patch.h/2)+h);
	}
	//without telling it where to put the patch, it will put it over the worst cut
	bool fit_onto_overflow(CImg<unsigned char> * passed_image, int patch_size, int retry_patches, double cutoff_cost){
		int worst_x=0;
		int worst_y=0;
		double worst_cut=0;
		for (int x=0;x<w;x++)
			for (int y=0;y<h;y++)
				if(worst_cut<get_cut_cost(x,y)){
					worst_x=x;
					worst_y=y;
					worst_cut=get_cut_cost(x,y);
				}
		printf("worst cut found was %.2f\n",worst_cut);
		//now, find a decent patch to stick onto it
		SubImage patch = SubImage(passed_image,patch_size);
		SubImage best_patch = patch;
		double best_cost=DBL_MAX;
		double this_cost=get_onto_overflow_cost(patch,worst_x-(patch.w/2)+w,worst_y-(patch.h/2)+h);
		
		while(best_cost>cutoff_cost && retry_patches>0){
			retry_patches--;
			if(best_cost>this_cost){
				best_patch = patch;
				best_cost = this_cost;
			}else{
				patch = SubImage(passed_image,patch_size);
				this_cost=get_onto_overflow_cost(patch,worst_x-(patch.w/2)+w,worst_y-(patch.h/2)+h);
			}
		}
		printf("best flow is %f\n",best_cost);
		fit_onto_overflow(patch,worst_x-(patch.w/2)+w,worst_y-(patch.h/2)+h);
		return true;
	}
	//get predicted cut cost without applying cut
	double get_onto_overflow_cost(SubImage patch, int at_x, int at_y){
		if(at_x<0 || at_y<0){
			printf("at is smaller than 0\n");
			return false;			
		}
		if(at_x+patch.w>w || at_y+patch.h>h){
			printf("patch goes beyond image, but that's okay\n");
		}
		//location in this image
		int min_x_i=at_x;
		int min_y_i=at_y;
		int max_x_i=at_x+patch.w;
		int max_y_i=at_y+patch.h;

		//location in patch
		int min_x_p=0;
		int min_y_p=0;
		
		//sigma squared depends on the size of the patch
		double sigma_squared = pow(sqrt((double)patch.w)/5,2);
		
		Grid* grid = new Grid(max_x_i-min_x_i+3,max_y_i-min_y_i+3);//if min_x==max_x, the width is 3, not 0. That's because there are uncuttable pixels on either side to which the one collumn of undecided pixels are attached to
		//middle pixel is sink
		grid->set_terminal_cap(grid->node_id((max_x_i-min_x_i+3)/2,(max_y_i-min_y_i+3)/2),0,DBL_MAX);
		
		//in the grid, the nodes refer to pixels at x+min_x_i-1, y+min_y_i-1
		for (int x=0;x<max_x_i-min_x_i+3;x++){
			for (int y=0;y<max_y_i-min_y_i+3;y++){
				//beyond the edge of the overlapping region
				if(x<=1 || y<=1 || x>=max_x_i-min_x_i+1 || y>=max_y_i-min_y_i+1){
					grid->set_terminal_cap(grid->node_id(x,y),DBL_MAX,0);
				}
				if(x>1)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,-1,0,(max_x_i-min_x_i+3)/2-x,(max_y_i-min_y_i+3)/2-y,sigma_squared);
				if(y>1)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,-1,(max_x_i-min_x_i+3)/2-x,(max_y_i-min_y_i+3)/2-y,sigma_squared);
				if(x<max_x_i-min_x_i+1)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,+1,0,(max_x_i-min_x_i+3)/2-x-1,(max_y_i-min_y_i+3)/2-y,sigma_squared);
				if(y<max_y_i-min_y_i+1)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,+1,(max_x_i-min_x_i+3)/2-x,(max_y_i-min_y_i+3)/2-y-1,sigma_squared);
			}
		}
		//initialize those not on the edge only. Many connections are calculated twice, which is wasteful
		//now, the information needs to be put into the connections of the nodes

		grid->compute_maxflow();
		double max_flow = fmod(grid->get_flow(),arbitrary_large_constant);
		return max_flow;
	}
	//using graphcut and a gaussian bell function multiplying the weights, make a round cut to replace the center of the underlying image with the patch
	bool fit_onto_overflow(SubImage patch, int at_x, int at_y){
		if(at_x<0 || at_y<0){
			printf("at is smaller than 0\n");
			return false;			
		}
		if(at_x+patch.w>w || at_y+patch.h>h){
			printf("patch goes beyond image, but that's okay\n");
		}
		//location in this image
		int min_x_i=at_x;
		int min_y_i=at_y;
		int max_x_i=at_x+patch.w;
		int max_y_i=at_y+patch.h;

		//location in patch
		int min_x_p=0;
		int min_y_p=0;
		
		//sigma squared depends on the size of the patch
		double sigma_squared = pow(sqrt((double)patch.w)/5,2);
		
		Grid* grid = new Grid(max_x_i-min_x_i+3,max_y_i-min_y_i+3);//if min_x==max_x, the width is 3, not 0. That's because there are uncuttable pixels on either side to which the one collumn of undecided pixels are attached to
		//middle pixel is sink
		grid->set_terminal_cap(grid->node_id((max_x_i-min_x_i+3)/2,(max_y_i-min_y_i+3)/2),0,DBL_MAX);
		
		//in the grid, the nodes refer to pixels at x+min_x_i-1, y+min_y_i-1
		for (int x=0;x<max_x_i-min_x_i+3;x++){
			for (int y=0;y<max_y_i-min_y_i+3;y++){
				//beyond the edge of the overlapping region
				if(x<=1 || y<=1 || x>=max_x_i-min_x_i+1 || y>=max_y_i-min_y_i+1){
					grid->set_terminal_cap(grid->node_id(x,y),DBL_MAX,0);
				}
				if(x>1)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,-1,0,(max_x_i-min_x_i+3)/2-x,(max_y_i-min_y_i+3)/2-y,sigma_squared);
				if(y>1)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,-1,(max_x_i-min_x_i+3)/2-x,(max_y_i-min_y_i+3)/2-y,sigma_squared);
				if(x<max_x_i-min_x_i+1)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,+1,0,(max_x_i-min_x_i+3)/2-x-1,(max_y_i-min_y_i+3)/2-y,sigma_squared);
				if(y<max_y_i-min_y_i+1)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,+1,(max_x_i-min_x_i+3)/2-x,(max_y_i-min_y_i+3)/2-y-1,sigma_squared);
			}
		}
		//initialize those not on the edge only. Many connections are calculated twice, which is wasteful
		//now, the information needs to be put into the connections of the nodes

		grid->compute_maxflow();
		double max_flow = fmod(grid->get_flow(),arbitrary_large_constant);
		
		//find the length of the cuts by finding how many adjacent values are different
		int length=0;
		for (int y=1;y<max_y_i-min_y_i+1;y++){
			for (int x=1;x<max_x_i-min_x_i+1;x++){
				//set_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1,-1);
				if(grid->get_segment(grid->node_id(x,y)) == 1){//delete previous seams on new patch
					set_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1,-1);
				}
				if(grid->get_segment(grid->node_id(x,y)) != grid->get_segment(grid->node_id(x+1,y))){
					length++;
					set_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1,get_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,x,y,+1,0));
				}
				if(grid->get_segment(grid->node_id(x,y)) != grid->get_segment(grid->node_id(x,y+1))){
					length++;
					set_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1,get_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,x,y,0,+1));
				}
				if(get_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1)==-1)
						printf("    ");
				else
						printf("%.1f ",get_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1));
			}
			printf("|\n");
		}
		
		for (int y=1;y<max_y_i-min_y_i+2;y++){
			for (int x=1;x<max_x_i-min_x_i+2;x++){
				if(grid->get_segment(grid->node_id(x,y)) == 0)
					for (int c=0;c<3;c++)
						;//set(x+min_x_i-1,y+min_y_i-1,c,255);
				else
					for (int c=0;c<3;c++)
						set_overflow(x+min_x_i-1,y+min_y_i-1,c,patch.get(x+min_x_p-1,y+min_y_p-1,c));
			}
		}
		return true;
	}

	bool fit_on(SubImage patch, int at_x, int at_y){
		//this function uses the mask to find construct the GridCut problem, calculate the GraphCut, and finally place the patch at the right place just like paste_on
		if(at_x<0 || at_y<0){
			printf("at is smaller than 0\n");
			return false;
		}
		if(at_x+patch.w>w || at_y+patch.h>h){
			printf("patch goes beyond image\n");
			return false;
		}
		//how big is the rectangular grid to run GridCut on?
		//location in this image
		int min_x_i=w;
		int min_y_i=h;
		int max_x_i=0;
		int max_y_i=0;
		//go over where the new patch will be
		for (int x=at_x;x<at_x+patch.w;x++){
			for (int y=at_y;y<at_y+patch.h;y++){
		  	if(is_known(x,y)){
					if(x<min_x_i)
						min_x_i=x;
					if(y<min_y_i)
						min_y_i=y;
					if(x>max_x_i)
						max_x_i=x;
					if(y>max_y_i)
						max_y_i=y;
				}
			}
		}
		//location in patch
		int min_x_p=min_x_i-at_x;
		int min_y_p=min_y_i-at_y;
		
		//printf("the GridCut grid will be from %d, %d to %d, %d, which is from %d, %d in the patch. ",min_x_i,min_y_i,max_x_i,max_y_i,min_x_p,min_y_p);
			
		Grid* grid = new Grid(max_x_i-min_x_i+3,max_y_i-min_y_i+3);//if min_x==max_x, the width is 3, not 0. That's because there are uncuttable pixels on either side to which the one collumn of undecided pixels are attached to
		//in the grid, the nodes refer to pixels at x+min_x_i-1, y+min_y_i-1
		for (int x=0;x<max_x_i-min_x_i+3;x++){
			for (int y=0;y<max_y_i-min_y_i+3;y++){
				//in area undefined in underlying image
				if(!is_known(x+min_x_i-1,y+min_y_i-1))
						grid->set_terminal_cap(grid->node_id(x,y),0,DBL_MAX);
				//beyond the edge of the overlapping region
				if(x==0 || y==0 || x==max_x_i-min_x_i+2 || y==max_y_i-min_y_i+2){
					//we are either on the underlying image, on the new patch, or on a pixel that won't be set at all
					if(is_known(x+min_x_i-1,y+min_y_i-1)){//connect to underlying image
						grid->set_terminal_cap(grid->node_id(x,y),DBL_MAX,0);
						//set(x+min_x_i-1,y+min_y_i-1,0,255);
					}else if(patch.is_known(x+min_x_p-1,y+min_y_p-1)){
						grid->set_terminal_cap(grid->node_id(x,y),0,DBL_MAX);
						//set(x+min_x_i-1,y+min_y_i-1,1,255);						
					}else{
						//set(x+min_x_i-1,y+min_y_i-1,2,255);
					}
				}
				if(x>0)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,-1,0);
				if(y>0)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,-1);
				if(x<max_x_i-min_x_i+2)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,+1,0);
				if(y<max_y_i-min_y_i+2)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,+1);
			}
		}
		//initialize those not on the edge only. Many connections are calculated twice, which is wasteful
		//now, the information needs to be put into the connections of the nodes

		grid->compute_maxflow();
		double max_flow = fmod(grid->get_flow(),arbitrary_large_constant);
		
		//find the length of the cuts by finding how many adjacent values are different
		int length=0;
		for (int y=1;y<max_y_i-min_y_i+1;y++){
			for (int x=1;x<max_x_i-min_x_i+1;x++){
				double pixel_cut_cost=0;
				if(grid->get_segment(grid->node_id(x,y)) != grid->get_segment(grid->node_id(x+1,y))){
					length++;
					pixel_cut_cost+=get_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,x,y,+1,0);
				}
				if(grid->get_segment(grid->node_id(x,y)) != grid->get_segment(grid->node_id(x,y+1))){
					length++;
					pixel_cut_cost+=get_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,x,y,0,+1);
				}
				set_cut_cost(x+min_x_i-1,y+min_y_i-1,pixel_cut_cost);
				if(get_cut_cost(x+min_x_i-1,y+min_y_i-1)==0)
						printf("    ");
				else
						printf("%.1f ",get_cut_cost(x+min_x_i-1,y+min_y_i-1));
			}
			printf("|\n");
		}
		length=length/2;
		printf("The used max flow is %f, for a cut of length %d\n",max_flow, length);
		
		for (int y=1;y<max_y_i-min_y_i+2;y++){
			for (int x=1;x<max_x_i-min_x_i+2;x++){
				if(grid->get_segment(grid->node_id(x,y)) == 0)
					for (int c=0;c<3;c++)
						;//set(x+min_x_i-1,y+min_y_i-1,c,255);
				else
					for (int c=0;c<3;c++)
						set(x+min_x_i-1,y+min_y_i-1,c,patch.get(x+min_x_p-1,y+min_y_p-1,c));
			}
		}
		for (int x=at_x;x<at_x+patch.w;x++)
			for (int y=at_y;y<at_y+patch.h;y++)
		  		if(is_known(x,y)){
					;
				}else{
					for (int c=0;c<3;c++)
						set(x,y,c,patch.get(x-at_x,y-at_y,c));
				}

		return true;
	}
	
	bool fit_on_overflow(SubImage patch, int at_x, int at_y){
		//this function uses the mask to find construct the GridCut problem, calculate the GraphCut, and finally place the patch at the right place just like paste_on
		if(at_x<0 || at_y<0){
			printf("at is smaller than 0\n");
			return false;
		}
		if(at_x+patch.w>w || at_y+patch.h>h){
			//printf("patch goes beyond image! That's okay.\n");
		}
		//how big is the rectangular grid to run GridCut on?
		//location in this image
		int min_x_i=w*2;
		int min_y_i=h*2;
		int max_x_i=0;
		int max_y_i=0;
		//go over where the new patch will be
		for (int x=at_x;x<at_x+patch.w;x++){
			for (int y=at_y;y<at_y+patch.h;y++){
		  	if(is_known_overflow(x,y)){
					if(x<min_x_i)
						min_x_i=x;
					if(y<min_y_i)
						min_y_i=y;
					if(x>max_x_i)
						max_x_i=x;
					if(y>max_y_i)
						max_y_i=y;
				}
			}
		}
		//location in patch
		int min_x_p=min_x_i-at_x;
		int min_y_p=min_y_i-at_y;
		
		//printf("the GridCut grid will be from %d, %d to %d, %d, which is from %d, %d in the patch. ",min_x_i,min_y_i,max_x_i,max_y_i,min_x_p,min_y_p);
			
		Grid* grid = new Grid(max_x_i-min_x_i+3,max_y_i-min_y_i+3);//if min_x==max_x, the width is 3, not 0. That's because there are uncuttable pixels on either side to which the one collumn of undecided pixels are attached to
		//in the grid, the nodes refer to pixels at x+min_x_i-1, y+min_y_i-1
		for (int x=0;x<max_x_i-min_x_i+3;x++){
			for (int y=0;y<max_y_i-min_y_i+3;y++){
				//in area undefined in underlying image
				if(!is_known_overflow(x+min_x_i-1,y+min_y_i-1))
						grid->set_terminal_cap(grid->node_id(x,y),0,DBL_MAX);
				//beyond the edge of the overlapping region
				if(x==0 || y==0 || x==max_x_i-min_x_i+2 || y==max_y_i-min_y_i+2){
					//we are either on the underlying image, on the new patch, or on a pixel that won't be set at all
					if(is_known_overflow(x+min_x_i-1,y+min_y_i-1)){//connect to underlying image
						grid->set_terminal_cap(grid->node_id(x,y),DBL_MAX,0);
						//set(x+min_x_i-1,y+min_y_i-1,0,255);
					}else if(patch.is_known(x+min_x_p-1,y+min_y_p-1)){
						grid->set_terminal_cap(grid->node_id(x,y),0,DBL_MAX);
						//set(x+min_x_i-1,y+min_y_i-1,1,255);						
					}else{
						//set(x+min_x_i-1,y+min_y_i-1,2,255);
					}
				}
				if(x>0)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,-1,0);
				if(y>0)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,-1);
				if(x<max_x_i-min_x_i+2)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,+1,0);
				if(y<max_y_i-min_y_i+2)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,+1);
			}
		}
		//initialize those not on the edge only. Many connections are calculated twice, which is wasteful
		//now, the information needs to be put into the connections of the nodes

		grid->compute_maxflow();
		double max_flow = fmod(grid->get_flow(),arbitrary_large_constant);
		
		//find the length of the cuts by finding how many adjacent values are different
		int length=0;
		for (int y=1;y<max_y_i-min_y_i+1;y++){
			for (int x=1;x<max_x_i-min_x_i+1;x++){
				if(grid->get_segment(grid->node_id(x,y)) == 1){//delete previous seams on new patch
					set_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1,-1);
				}
				if(grid->get_segment(grid->node_id(x,y)) != grid->get_segment(grid->node_id(x+1,y))){
					length++;
					set_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1,get_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,x,y,+1,0));
				}
				if(grid->get_segment(grid->node_id(x,y)) != grid->get_segment(grid->node_id(x,y+1))){
					length++;
					set_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1,get_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,x,y,0,+1));
				}
				//if(get_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1)==-1)
				//		printf("    ");
				//else
				//		printf("%.1f ",get_cut_cost_overflow(x+min_x_i-1,y+min_y_i-1));
			}
			//printf("|\n");
		}
		length=length/2;
		printf("The used max flow is %f, for a cut of length %d\n",max_flow, length);
		
		for (int y=1;y<max_y_i-min_y_i+2;y++){
			for (int x=1;x<max_x_i-min_x_i+2;x++){
				if(grid->get_segment(grid->node_id(x,y)) == 0)
					for (int c=0;c<3;c++)
						;//set(x+min_x_i-1,y+min_y_i-1,c,255);
				else
					for (int c=0;c<3;c++)
						set_overflow(x+min_x_i-1,y+min_y_i-1,c,patch.get(x+min_x_p-1,y+min_y_p-1,c));
			}
		}
		for (int x=at_x;x<at_x+patch.w;x++)
			for (int y=at_y;y<at_y+patch.h;y++)
		  		if(is_known_overflow(x,y)){
					;
				}else{
					for (int c=0;c<3;c++)
						set_overflow(x,y,c,patch.get(x-at_x,y-at_y,c));
				}

		return true;
	}
	
	double get_cut_cost(SubImage patch, int at_x, int at_y){
		//this function uses the mask to find construct the GridCut problem, calculate the GraphCut, and finally place the patch at the right place just like paste_on
		if(at_x<0 || at_y<0){
			printf("at is smaller than 0\n");
			return false;
		}
		if(at_x+patch.w>w || at_y+patch.h>h){
			printf("patch goes beyond image\n");
			return false;
		}
		//how big is the rectangular grid to run GridCut on?
		//location in this image
		int min_x_i=w;
		int min_y_i=h;
		int max_x_i=0;
		int max_y_i=0;
		//go over where the new patch will be
		for (int x=at_x;x<at_x+patch.w;x++){
			for (int y=at_y;y<at_y+patch.h;y++){
		  	if(is_known(x,y)){
					if(x<min_x_i)
						min_x_i=x;
					if(y<min_y_i)
						min_y_i=y;
					if(x>max_x_i)
						max_x_i=x;
					if(y>max_y_i)
						max_y_i=y;
				}
			}
		}
		//location in patch
		int min_x_p=min_x_i-at_x;
		int min_y_p=min_y_i-at_y;
		
		//printf("the GridCut grid will be from %d, %d to %d, %d, which is from %d, %d in the patch. ",min_x_i,min_y_i,max_x_i,max_y_i,min_x_p,min_y_p);
			
		Grid* grid = new Grid(max_x_i-min_x_i+3,max_y_i-min_y_i+3);//if min_x==max_x, the width is 3, not 0. That's because there are uncuttable pixels on either side to which the one collumn of undecided pixels are attached to
		//in the grid, the nodes refer to pixels at x+min_x_i-1, y+min_y_i-1
		for (int x=0;x<max_x_i-min_x_i+3;x++){
			for (int y=0;y<max_y_i-min_y_i+3;y++){
				//in area undefined in underlying image
				if(!is_known(x+min_x_i-1,y+min_y_i-1))
						grid->set_terminal_cap(grid->node_id(x,y),0,DBL_MAX);
				//beyond the edge of the overlapping region
				if(x==0 || y==0 || x==max_x_i-min_x_i+2 || y==max_y_i-min_y_i+2){
					//we are either on the underlying image, on the new patch, or on a pixel that won't be set at all
					if(is_known(x+min_x_i-1,y+min_y_i-1)){//connect to underlying image
						grid->set_terminal_cap(grid->node_id(x,y),DBL_MAX,0);
						//set(x+min_x_i-1,y+min_y_i-1,0,255);
					}else if(patch.is_known(x+min_x_p-1,y+min_y_p-1)){
						grid->set_terminal_cap(grid->node_id(x,y),0,DBL_MAX);
						//set(x+min_x_i-1,y+min_y_i-1,1,255);						
					}else{
						//set(x+min_x_i-1,y+min_y_i-1,2,255);
					}
				}
				if(x>0)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,-1,0);
				if(y>0)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,-1);
				if(x<max_x_i-min_x_i+2)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,+1,0);
				if(y<max_y_i-min_y_i+2)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,+1);
			}
		}
		//initialize those not on the edge only. Many connections are calculated twice, which is wasteful
		//now, the information needs to be put into the connections of the nodes

		grid->compute_maxflow();
		double max_flow = fmod(grid->get_flow(),arbitrary_large_constant);
		return max_flow;
	}
	
	double get_cut_cost_overflow(SubImage patch, int at_x, int at_y){
		//this function uses the mask to find construct the GridCut problem, calculate the GraphCut, and finally place the patch at the right place just like paste_on
		if(at_x<0 || at_y<0){
			printf("at is smaller than 0\n");
			return false;
		}
		if(at_x+patch.w>w || at_y+patch.h>h){
			//printf("patch goes beyond image! That's okay.\n");
		}
		//how big is the rectangular grid to run GridCut on?
		//location in this image
		int min_x_i=w*2;
		int min_y_i=h*2;
		int max_x_i=0;
		int max_y_i=0;
		//go over where the new patch will be
		for (int x=at_x;x<at_x+patch.w;x++){
			for (int y=at_y;y<at_y+patch.h;y++){
		  	if(is_known_overflow(x,y)){
					if(x<min_x_i)
						min_x_i=x;
					if(y<min_y_i)
						min_y_i=y;
					if(x>max_x_i)
						max_x_i=x;
					if(y>max_y_i)
						max_y_i=y;
				}
			}
		}
		//location in patch
		int min_x_p=min_x_i-at_x;
		int min_y_p=min_y_i-at_y;
		
		//printf("the GridCut grid will be from %d, %d to %d, %d, which is from %d, %d in the patch. ",min_x_i,min_y_i,max_x_i,max_y_i,min_x_p,min_y_p);
			
		Grid* grid = new Grid(max_x_i-min_x_i+3,max_y_i-min_y_i+3);//if min_x==max_x, the width is 3, not 0. That's because there are uncuttable pixels on either side to which the one collumn of undecided pixels are attached to
		//in the grid, the nodes refer to pixels at x+min_x_i-1, y+min_y_i-1
		for (int x=0;x<max_x_i-min_x_i+3;x++){
			for (int y=0;y<max_y_i-min_y_i+3;y++){
				//in area undefined in underlying image
				if(!is_known_overflow(x+min_x_i-1,y+min_y_i-1))
						grid->set_terminal_cap(grid->node_id(x,y),0,DBL_MAX);
				//beyond the edge of the overlapping region
				if(x==0 || y==0 || x==max_x_i-min_x_i+2 || y==max_y_i-min_y_i+2){
					//we are either on the underlying image, on the new patch, or on a pixel that won't be set at all
					if(is_known_overflow(x+min_x_i-1,y+min_y_i-1)){//connect to underlying image
						grid->set_terminal_cap(grid->node_id(x,y),DBL_MAX,0);
						//set(x+min_x_i-1,y+min_y_i-1,0,255);
					}else if(patch.is_known(x+min_x_p-1,y+min_y_p-1)){
						grid->set_terminal_cap(grid->node_id(x,y),0,DBL_MAX);
						//set(x+min_x_i-1,y+min_y_i-1,1,255);						
					}else{
						//set(x+min_x_i-1,y+min_y_i-1,2,255);
					}
				}
				if(x>0)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,-1,0);
				if(y>0)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,-1);
				if(x<max_x_i-min_x_i+2)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,+1,0);
				if(y<max_y_i-min_y_i+2)
					connect_grid_nodes(x+min_x_i-1,y+min_y_i-1,patch,x+min_x_p-1,y+min_y_p-1,grid,x,y,0,+1);
			}
		}
		//initialize those not on the edge only. Many connections are calculated twice, which is wasteful
		//now, the information needs to be put into the connections of the nodes

		grid->compute_maxflow();
		double max_flow = fmod(grid->get_flow(),arbitrary_large_constant);

		return max_flow;
	}
	
	void connect_grid_nodes(int ix, int iy, SubImage patch, int px, int py, Grid* grid, int gx, int gy, int plus_x, int plus_y){
		//if ix and ix+plus_x are known, and px and px+plus_x are known, and the same is true for y, then calculate matching_quality_cost
		grid->set_neighbor_cap(grid->node_id(gx,gy),plus_x,plus_y, get_grid_nodes(ix, iy, patch, px, py, gx, gy, plus_x, plus_y));
	}
	
	void connect_grid_nodes(int ix, int iy, SubImage patch, int px, int py, Grid* grid, int gx, int gy, int plus_x, int plus_y, int x_from_center, int y_from_center, double sigma_squared){
		double gaussian = 0.01 + gaussian2D(x_from_center, y_from_center, sigma_squared);
		//if ix and ix+plus_x are known, and px and px+plus_x are known, and the same is true for y, then calculate matching_quality_cost
		grid->set_neighbor_cap(grid->node_id(gx,gy),plus_x,plus_y, get_grid_nodes(ix, iy, patch, px, py, gx, gy, plus_x, plus_y)*gaussian);
	}
	
	double get_grid_nodes(int ix, int iy, SubImage patch, int px, int py, int gx, int gy, int plus_x, int plus_y){
		if(is_known_overflow(ix,iy) && is_known_overflow(ix+plus_x,iy+plus_y) && patch.is_known_overflow(px,py) && patch.is_known_overflow(px+plus_x,py+plus_y)){//all points are known
			return matching_quality_cost(ix,iy,patch,px,py)+matching_quality_cost(ix+plus_x,iy+plus_y,patch,px+plus_x,py+plus_y);
		}else{
			return arbitrary_large_constant;
		}
	}
	
	double cielab_ab(double div) {
	    double ret;
	    if (div > 0.008856f) {
	        ret = pow(div, 1.0f/3.0f);
	    } else {
	        ret = 7.787f * div + 16.0f/116.0f;
	    }
	    return ret;
	}
	
	double matching_quality_cost(int ix, int iy, SubImage patch, int px, int py){
		double R=get_overflow(ix,iy,0);
		double G=get_overflow(ix,iy,1);
		double B=get_overflow(ix,iy,2);
		double x = 0.4124564*R + 0.3575761*G + 0.1804375*B;
		double y = 0.2126729*R + 0.7151522*G + 0.0721750*B;
		double z = 0.0193339*R + 0.1191920*G + 0.9503041*B;
		double whitediv = y/1.0;
		double L1 = 116.0 * cielab_ab(whitediv) - 16.0;
		double a1 = 500.0 * (cielab_ab(x/0.9505) - cielab_ab(whitediv));
		double b1 = 200.0 * (cielab_ab(whitediv) - cielab_ab(z/1.0890));

		R=patch.get_overflow(px,py,0);
		G=patch.get_overflow(px,py,1);
		B=patch.get_overflow(px,py,2);
		x = 0.4124564*R + 0.3575761*G + 0.1804375*B;
		y = 0.2126729*R + 0.7151522*G + 0.0721750*B;
		z = 0.0193339*R + 0.1191920*G + 0.9503041*B;
		double L2 = 116.0 * cielab_ab(whitediv) - 16.0;
		double a2 = 500.0 * (cielab_ab(x/0.9505) - cielab_ab(whitediv));
		double b2 = 200.0 * (cielab_ab(whitediv) - cielab_ab(z/1.0890));
		
		double squared_sum=0;
		//for (int c=0;c<3;c++)
		//	squared_sum+=pow((double)get(ix,iy,c)-(double)patch.get(px,py,c),2);
		squared_sum = pow(L1-L2,2)+pow(a1-a2,2)+pow(b1-b2,2);
		//printf("returning %f\n",sqrt(squared_sum)/1000);
		return sqrt(squared_sum)/1000;
	}
	
	bool save(const char* loc){
		printf("saving to %s\n",loc);
		(*image).save(loc);
		return true;
	}
	bool save_cut_map(const char* loc){
		printf("saving cut map to %s\n",loc);
		CImg<unsigned char> real_image = CImg<unsigned char>(w,h,1,3,0);
		for (int x=0;x<w;x++)
			for (int y=0;y<h;y++)
				for (int c=0;c<3;c++)
					real_image(x,y,c)=get_cut_cost(x,y)==-1?0:get_cut_cost(x,y)*100;
		real_image.save(loc);
		return true;
	}
};