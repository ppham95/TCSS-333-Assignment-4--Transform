#include "pixutils.h"
#include "bmp/bmp.h"

//private methods -> make static
static pixMap* pixMap_init();
static pixMap* pixMap_copy(pixMap *p);

//plugin methods <-new for Assignment 4;
static void rotate(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void convolution(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void flipVertical(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);
static void flipHorizontal(pixMap *p, pixMap *oldPixMap,int i, int j,void *data);

static pixMap* pixMap_init(){
	pixMap *p=malloc(sizeof(pixMap));
	p->pixArray_overlay=0;
	return p;
}	

void pixMap_destroy (pixMap **p){
	if(!p || !*p) return;
	pixMap *this_p=*p;
	if(this_p->pixArray_overlay)
	 free(this_p->pixArray_overlay);
	if(this_p->image)free(this_p->image);	
	free(this_p);
	this_p=0;
}
	
pixMap *pixMap_read(char *filename){
	pixMap *p=pixMap_init();
 	int error;
 	if((error=lodepng_decode32_file(&(p->image), &(p->imageWidth), &(p->imageHeight),filename))){
  		fprintf(stderr,"error %u: %s\n", error, lodepng_error_text(error));
  		return 0;
	}
	p->pixArray_overlay=malloc(p->imageHeight*sizeof(rgba*));
	p->pixArray_overlay[0]=(rgba*) p->image;
	for(int i=1;i<p->imageHeight;i++){
  		p->pixArray_overlay[i]=p->pixArray_overlay[i-1]+p->imageWidth;
	}			
	return p;
}
int pixMap_write(pixMap *p,char *filename){
	int error=0;
 	if(lodepng_encode32_file(filename, p->image, p->imageWidth, p->imageHeight)){
  		fprintf(stderr,"error %u: %s\n", error, lodepng_error_text(error));
  		return 1;
	}
	return 0;
}	 

pixMap *pixMap_copy(pixMap *p){
	pixMap *new=pixMap_init();
	*new=*p;
	new->image=malloc(new->imageHeight*new->imageWidth*sizeof(rgba));
	memcpy(new->image,p->image,p->imageHeight*p->imageWidth*sizeof(rgba));	
	new->pixArray_overlay=malloc(new->imageHeight*sizeof(void*));
	new->pixArray_overlay[0]=(rgba*) new->image;
	for(int i=1;i<new->imageHeight;i++){
  		new->pixArray_overlay[i]=new->pixArray_overlay[i-1]+new->imageWidth;
	}	
	return new;
}

	
void pixMap_apply_plugin(pixMap *p,plugin *plug){
	pixMap *copy=pixMap_copy(p);
	for(int i=0;i<p->imageHeight;i++){
	 	for(int j=0;j<p->imageWidth;j++){
			plug->function(p,copy,i,j,plug->data);
		}
	}
	pixMap_destroy(&copy);	 
}

int pixMap_write_bmp16(pixMap *p,char *filename){
 	BMP16map *bmp16=BMP16map_init(p->imageHeight,p->imageWidth,0,5,6,5); //initialize the bmp type
 	if(!bmp16) return 1;
 	// bmp16->pixArray[i][j] is 2-d array for bmp files. It is analogous to the one for our png file pixMaps except that it is 16 bits
 	//However pixMap and BMP16_map are "upside down" relative to each other
 	//need to flip one of the the row indices when copying

 	for(int i = 0; i < p->imageHeight; i++){
		for(int j = 0; j < p->imageWidth; j++){
			const int newI = p->imageHeight - i- 1;
			const int newJ = j;
			uint16_t newBit=0;

   			newBit  =  (p->pixArray_overlay[newI][newJ].b >> (3));
      		newBit |= (p->pixArray_overlay[newI][newJ].g >> (2)) << (5);
	  		newBit |= (p->pixArray_overlay[newI][newJ].r >> (3)) << (11);
	  		newBit |= (p->pixArray_overlay[newI][newJ].a >> (8)) << (16);
	  		bmp16->pixArray[i][j]=newBit;
		}			
	}	

 	BMP16map_write(bmp16,filename);
	BMP16map_destroy(&bmp16);
 	return 0;
}	 

void plugin_destroy(plugin **plug){
	if(!plug || !*plug) return;
	if((*plug)->data) free((*plug)->data);
	if (*plug) free(*plug);
	*plug = 0;
}

plugin *plugin_parse(char *argv[] ,int *iptr){
	//malloc new plugin
	plugin *new=malloc(sizeof(plugin));
	new->function=0;
	new->data=0;
	
	int i=*iptr;
	if(!strcmp(argv[i]+2,"rotate")){
		new->function=rotate;
		new->data=malloc(2*sizeof(float));
		float *sc=(float*) new ->data;
		float theta=atof(argv[i+1]);
		sc[0] = sin(degreesToRadians(-theta));
		sc[1] = cos(degreesToRadians(-theta));
		*iptr = i+2;
		return new;	
	}	

	if(!strcmp(argv[i]+2,"convolution")){
		new->function=convolution;
		new->data=malloc(9*sizeof(int));
		int *matrix=(int*) new->data;
		// 1 2 3
		// 4 5 6
		// 7 8 9
		for (int j = 1; j <= 9; j++) {
			matrix[j-1] = atoi(argv[i+j]);
		}
		*iptr=i+10;	
  		return new;
	}
	if(!strcmp(argv[i]+2,"flipHorizontal")){	
		new->function=flipHorizontal;
  		*iptr=i+1;
  		return new;
	}
	if(!strcmp(argv[i]+2,"flipVertical")){
		new->function=flipVertical;
  		*iptr=i+1;
  		return new;
	}		
	return(0);
}	

//define plugin functions

static void rotate(pixMap *p, pixMap *oldPixMap,int y, int x,void *data){
	float *sc =(float*) data;
	const float ox = p->imageWidth/2.0f;
	const float oy = p->imageHeight/2.0f;
	const float s = sc[0];
	const float c = sc[1];
	float rotx = c*(x-ox) - s * (oy-y) + ox;
   	float roty = -(s*(x-ox) + c * (oy-y) - oy);
	int rotj = rotx+.5;
	int roti = roty+.5; 

	if (roti >=0 && roti < oldPixMap->imageHeight && rotj >=0 && rotj < oldPixMap->imageWidth) {
		memcpy(p->pixArray_overlay[y]+x,oldPixMap->pixArray_overlay[roti]+rotj,sizeof(rgba));
	} else {
		memset(p->pixArray_overlay[y]+x, 0, sizeof(rgba));
	}
}

static void convolution(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
	//implement algorithm givne in https://en.wikipedia.org/wiki/Kernel_(image_processing)
	//assume that the kernel is a 3x3 matrix of integers
	//don't forget to normalize by dividing by the sum of all the elements in the matrix

	int *matrix =(int*) data;
	int sum = 0, r = 0, g = 0, b = 0, count = 0, weight = 0;
	for (int n = 0; n < 9; n++) {
		sum += matrix[n];
	}

	for (int row = i-1; row <= i+1; row++) {
		int theY = row;
		if (theY < 0) {
			theY = 0;
		} else if (theY > oldPixMap->imageHeight - 1) {
			theY = oldPixMap->imageHeight-1;
		}

		for (int col = j-1; col <= j+1; col++) {
			weight = matrix[count];
			int theX = col;
			if (theX < 0) {
				theX = 0;
			} else if (theX > oldPixMap->imageWidth - 1) {
				theX = oldPixMap->imageWidth-1;
			}
			// printf("%d %d\n", theY, theX);

			r+= oldPixMap->pixArray_overlay[theY][theX].r*weight;
			g+= oldPixMap->pixArray_overlay[theY][theX].g*weight;
			b+= oldPixMap->pixArray_overlay[theY][theX].b*weight;
			count++;
		}
	}
	// printf("%d\n", count);

	if (!sum==0) {
		r /= sum;
		g /= sum;
		b /= sum;
	}

	if (r < 0 ) r = 0;
	else if (r > 255) r = 255;
	p->pixArray_overlay[i][j].r = (char) r;

	if (g < 0 ) g = 0;
	else if (g > 255) g = 255;
	p->pixArray_overlay[i][j].g = (char) g;

	if (b < 0 ) b = 0;
	else if (b > 255) b = 255;
	p->pixArray_overlay[i][j].b = (char) b;
}

//very simple functions - does not use the data pointer - good place to start
 
static void flipVertical(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
	memcpy(p->pixArray_overlay[i]+j, oldPixMap->pixArray_overlay[oldPixMap->imageHeight-i-1]+j, sizeof(rgba));
}	 
static void flipHorizontal(pixMap *p, pixMap *oldPixMap,int i, int j,void *data){
	memcpy(p->pixArray_overlay[i]+j, oldPixMap->pixArray_overlay[i] + oldPixMap->imageWidth-1-j, sizeof(rgba));
}
