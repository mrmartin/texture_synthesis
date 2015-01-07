/*
  ex2-segmentation
  ================

  This example shows a minimal image segmentation code based on GridCut.
*/
#include "CImg/CImg.h"
using namespace cimg_library;
#include <cstdio>
#include <cmath>
#include <GridCut/GridGraph_2D_4C.h>
#include <string>
#include <cfloat>


#define FG 255
#define BG 128

#define K 1024
#define SIGMA2 16.0f

#define WEIGHT(A) (int)(1+K*expf((-(A)*(A)/SIGMA2)))


typedef GridGraph_2D_4C<double,double,double> Grid;


class SubImage {
  	public:
			int w, h, x1, y1, x2, y2;
			CImg<unsigned char> real_image;
			CImg<unsigned char> * image;
			bool * known_mask;
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
		printf("the random rectangle is taken from image(%d, %d, %d, %d)\n",randx,randy,randx+rect_size,randy+rect_size);
		known_mask = new bool[w*h];
		for (int i=0;i<w;i++)
			for (int j=0;j<h;j++)
				set_known(i,j);
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
		for (int i=0;i<w;i++)
			for (int j=0;j<h;j++)
				set_known(i,j);
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
		for (int i=0;i<w;i++)
			for (int j=0;j<h;j++)
				set_known(i,j);
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
		for (int i=0;i<w;i++)
			for (int j=0;j<h;j++)
				set_unknown(i,j);
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
	unsigned char get(int x, int y, int c){
		if(x<0 || x>=w || y<0 || y>=h || c<0 || c>2){
			printf("getting information outside of SubImage");
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
	bool paste_on(SubImage patch, int at_x, int at_y){
		printf("putting patch of size %d x %d at (%d, %d) into an image of size %d x %d\n",patch.w, patch.h,at_x,at_y,w,h);
		//this function pastes patch onto image, putting the corner of patch at (at_x, at_y)
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
		
		printf("the GridCut grid will be from %d, %d to %d, %d, which is from %d, %d in the patch\n",min_x_i,min_y_i,max_x_i,max_y_i,min_x_p,min_y_p);
			
		Grid* grid = new Grid(max_x_i-min_x_i+3,max_y_i-min_y_i+3);//if min_x==max_x, the width is 3, not 0. That's because there are uncuttable pixels on either side to which the one collumn of undecided pixels are attached to
		//in the grid, the nodes refer to pixels at x+min_x_i-1, y+min_y_i-1
		for (int x=0;x<max_x_i-min_x_i+3;x++){
			for (int y=0;y<max_y_i-min_y_i+3;y++){
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
				
		printf("now, compute flow\n");
		grid->compute_maxflow();
		
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
	void connect_grid_nodes(int ix, int iy, SubImage patch, int px, int py, Grid* grid, int gx, int gy, int plus_x, int plus_y){
		//if ix and ix+plus_x are known, and px and px+plus_x are known, and the same is true for y, then calculate matching_quality_cost
		if(is_known(ix,iy) && is_known(ix+plus_x,iy+plus_y) && patch.is_known(px,py) && patch.is_known(px+plus_x,py+plus_y)){//all points are known
			grid->set_neighbor_cap(grid->node_id(gx,gy),plus_x, plus_y, matching_quality_cost(ix,iy,patch,px,py)+matching_quality_cost(ix+plus_x,iy+plus_y,patch,px+plus_x,py+plus_y));
		}else{
			grid->set_neighbor_cap(grid->node_id(gx,gy),plus_x, plus_y, DBL_MAX);
		}

	}
	double matching_quality_cost(int ix, int iy, SubImage patch, int px, int py){
		double squared_sum=0;
		for (int c=0;c<3;c++)
			squared_sum+=pow((double)get(ix,iy,c)-(double)patch.get(px,py,c),2);
		//printf("returning %f\n",sqrt(squared_sum));
		return sqrt(squared_sum);
	}
	bool save(const char* loc){
		printf("saving to %s\n",loc);
		(*image).save(loc);
		return true;
	}
};

int main(int argc, char **argv)
{
	srand(time(NULL));
	unsigned short w,h;
	CImg<unsigned char> image(argv[1]);
	SubImage whole(&image);//creates reference to entire image
	int width=image.width();
	int height=image.height();
	//whole.print();

	SubImage left(&image,0,0,width/2,height);//creates reference to subimage
	//left.print();
	
	SubImage right(&image,width/2,0,width,height);//creates reference to subimage
	//right.print();
	
	int out_size=512;
	//make a canvas for the Image Quilting result
	SubImage tile(out_size,out_size);
	//tile.print();
	
	//put a random rectangle in the middle of the canvas
	int rect_size=64;
	SubImage rect(&image,rect_size);//creates random subimage of size rect_size*rect_size
	rect.print();
	tile.paste_on(rect,0,0);
		
	//the first patch doesn't care about anything.
	//the second patch, until the last patch before the tile is filled, needs to know which pixels come from images and which ones don't. There needs to be a mask, and a way of finding cuts of arbitrary shape between the mask
	//once the tile is filled, any new patch won't need the mask, but will calculate the cut against everything.
	//for all these, the get and set functions need to be modulo arithmetic
	for (int i=0;i<250;i++){
		SubImage rect2(&image,rect_size);//creates random subimage of size rect_size*rect_size
		tile.fit_on(rect2,rand()%(out_size-rect_size),rand()%(out_size-rect_size));
	}
	
	tile.save("output.png");
	
  /*typedef GridGraph_2D_4C<short,short,short> Grid;

  unsigned short x,y,w,h,cap;
  unsigned char *img,*msk;
  
  img = load_TGA("image.tga",w,h);
  msk = load_TGA("mask.tga",w,h);
  
  #define IMG(X,Y) img[(X)+(Y)*w]
  #define MSK(X,Y) msk[(X)+(Y)*w]

  Grid* grid = new Grid(w,h);
  
  for (y=0;y<h;y++)
  {
    for (x=0;x<w;x++)
    {
      grid->set_terminal_cap(grid->node_id(x,y),(MSK(x,y)==FG)*K,(MSK(x,y)==BG)*K);

      if (x<w-1)
      {
        cap = WEIGHT(IMG(x,y)-IMG(x+1,y));

        grid->set_neighbor_cap(grid->node_id(x,y),  +1,0,cap);
        grid->set_neighbor_cap(grid->node_id(x+1,y),-1,0,cap);
      }

      if (y<h-1)
      {
        cap = WEIGHT(IMG(x,y)-IMG(x,y+1));

        grid->set_neighbor_cap(grid->node_id(x,y),  0,+1,cap);
        grid->set_neighbor_cap(grid->node_id(x,y+1),0,-1,cap);
      }
    }
  }

  grid->compute_maxflow();

  for (y=0;y<h;y++)
  {
    for (x=0;x<w;x++)
    {
      if (grid->get_segment(grid->node_id(x,y))) IMG(x,y)=0;
    }
  }

  save_TGA("output.tga",img,w,h);

  delete grid;
*/
  return 0;
}
