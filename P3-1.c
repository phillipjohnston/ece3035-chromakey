/*                  Virtual Blue-Screen

                "Students take a walk in the park"

This program performs a moving object extraction from one sequence,
and superposes it to another static background. It employs a utility
library for jpeg/frame buffer access, a multi-modal mean library for
dynamic background modeling and foreground extraction, and a roller
utility for area density and blob detection. The blobs are area
filtered and used as a mask layer for foreground transfer to the
target image.

NN April 2011                      Phillip Johnston  */

#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include "rollers.h"
#include "mmm.h"


//Function Declarations
void LoadOriginalImage(int N);
void GrabForegroundImage(int N);
void GrabDensityMap(int N);
void GrabBlobAnnotatedMap(int N);
void WriteOutResultsStack(int N);
void WriteOutOutputImage(int N);

//Globals

/* Declarations */
int  	        MCDth = 33, Cth = 4, DecRate = 2; //TODO: Set these to appropriate values
Cell            **BGM;
FrmBuf          *FB, *wFB, *dFB, *oFB, *woFB, *rsFB;
int             *DensityMap;
int		Bth = 20, Wsize = 7, NumBlobs = 0;
Blob            *Blobs;


//BEGIN MAIN LOOP
int main(int argc, char *argv[]) {
   char                 Path[128], cFile[128] = {0}, *SeqName;
   int			Start, End, Step, N;
   int			height, width; //Declare variables to use with initializations of buffers

   if (argc !=5) {
      fprintf(stderr, "usage: %s seqname start end step\n", argv[0]);
      exit(1);
   }
   SeqName = argv[1];
   if (sscanf(argv[2], "%d", &Start) != 1 || Start < 0 ||
       sscanf(argv[3], "%d", &End) != 1 || End < Start ||
       sscanf(argv[4], "%d", &Step) != 1 || Step < 1) {
      fprintf(stderr, "[%s:%s:%s] are invalid start/end/step numbers\n", argv[2], argv[3], argv[4]);
      exit(1);
   }
   if (InDir("trials", BASE_DIR) == FALSE) {
      if (DEBUG)
         printf("   creating %s ...\n", TRIAL_DIR);
      mkdir(TRIAL_DIR, (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH));
   }
   if (InDir(SeqName, TRIAL_DIR) == FALSE) {
      sprintf(Path, "%s/%s", TRIAL_DIR, SeqName);
      if (DEBUG)
         printf("   creating %s ...\n", Path);
      mkdir(Path, (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH));
   }

   /* Get information to allocate frame buffers */
   sprintf(cFile, "InSeq/%05d.jpg", Start + 1);
   
   //Let's get the image row/column information
   Read_Header(cFile, &width, &height);
   if(DEBUG){
	   printf("Image width: %d, Image height: %d\n", width, height);
	   printf("Results stack width: %d, height: %d\n", width, height*4);
   }

   /* Allocate Frame Buffers */
   rsFB = Alloc_Frame(width, height * 4);				//Results Stack
   wFB = Alloc_Frame(width, height);					//Working Frame Buffer
   Read_Header("park.jpg", &width, &height);				// Get the width and heigh
   woFB = Alloc_Frame(width, height);					//Output Image
   oFB = Create_Frame("park.jpg");					//Set oFB equal to the park img

   /* Process Background */
   FB = Create_Frame(cFile);
   BGM = Create_Initial_BGM(FB);
   
   //Hopefully 3 frames is enough to pick the foreground
   //And hopefully I"m actually supposed to do this...
   for (N = Start + 1; N <= 3; N += Step) { 
	   sprintf(cFile, "InSeq/%05d.jpg", N);	//Load the path into cFile
	   Load_Image(cFile, FB);		//Load the image into FB

	   //From examples given in library
	   Process_Frame_BG(BGM, FB, MCDth, Cth);
	   if (N % DecRate == 0) {
		   Decimate_BGM(BGM, Cth, FB->Width * FB->Height);
	   }
   }


   /* Process Image Results Set */
   for (N = Start + 1; N < End + 1; N += Step) {            // for each frame in sequence
      if (DEBUG)
         printf("   processing frame %05d.jpg ...\n", N);

      if(DEBUG)
    	  printf("\tLoading Original Image...\n");
      LoadOriginalImage(N);

      if(DEBUG)
    	  printf("\tGrabbing foreground image...\n");
      GrabForegroundImage(N);

      if(DEBUG)
    	  printf("\tGrabbing density map...\n");
      GrabDensityMap(N);

      if(DEBUG)
    	  printf("\tGrabbing blob annotated map...\n");
      GrabBlobAnnotatedMap(N);
      
      //Write out the resulting images
      WriteOutResultsStack(N);
      WriteOutOutputImage(N);
   }
   exit(0);
}

/*
Load the original image into FB.  Then copy it to the results stack
*/

void LoadOriginalImage(int N) {
   char file[128] = {0};
   sprintf(file, "InSeq/%05d.jpg", N);		//Put the path in file

   //Load the image into FB
   FB = Create_Frame(file);

   //Before we finish, let's copy the original image into the top of the results buffer
   Copy_Image(FB, rsFB, 0); //0 offset for top of the image.

}


/*
Extract the foreground from the image.  Copy this to the results stack.
*/
void GrabForegroundImage(int N) {
	//Copy the original image into the current frame for processing
	wFB = Duplicate_Frame(FB);
	
	//Process the foreground of the image
	Process_Frame_FG(BGM, wFB, MCDth, Cth);

	Copy_Image(wFB, rsFB, 1); //140 offset for below original image
}

/*
Using the extracted foreground, create a density map of the current frame.
Copy this to the results stack.
*/

void GrabDensityMap(int N) {
	dFB = Duplicate_Frame(wFB);	//Duplicate foreground-extracted image
	DensityMap = (int *) malloc(dFB->Width * dFB->Height * sizeof(int)); //Alloc the density map
	Area_Image_Density(dFB, DensityMap, Wsize);
	Paint_Frame(dFB, Wsize*Wsize, DensityMap);
	Copy_Image(dFB, rsFB, 2); //28 for below foreground image
}

/*
Annotate the blob bounds and centers of mass from the density map.
Copy this result to the results stack.
*/

void GrabBlobAnnotatedMap(int N) {
	wFB = Duplicate_Frame(dFB);
	Blobs = Blob_Finder(DensityMap, wFB->Width, wFB->Height, Bth);
	//Blobs = Blob_Finder_Map(DensityMap, wFB->Width, wFB->Height, Bth);
	Mark_Blob_CoM(Blobs, wFB);
	Mark_Blob_BB(Blobs, wFB);
	Mark_Blob_ID_Map(DensityMap, NumBlobs);
	Copy_Image(wFB, rsFB, 3); //420 offset for top below density map

	if(DEBUG)
		Print_Blobs(Blobs);
		
	free(DensityMap);	//Let's free it when we're done
}

/*
Writes the results stack image to the proper locatoin
*/

void WriteOutResultsStack(int N) {
	char file[128] = {0};
	sprintf(file, "trials/00/rs%05d.jpg", N);

	if(DEBUG)
		printf("Outputting results to file: %s \n", file);

	Store_Image(file, rsFB);
}

/*
Goes through the blobs, checks the pixel coloring of the density map, and copies the appropriate pixels from the original image ot the park.

Writes out the result once all the blobs have been processed
*/

void WriteOutOutputImage(int N) {
	woFB = Duplicate_Frame(oFB);	//Copy the park frame buffer to the working output frame buffer
	int i,j;
	Pixel *P;
	Blob *CurrentBlob = Blobs;
	
	///Let's go through all the blobs that we found
	while(CurrentBlob != NULL) {
	
		//In order to get rid of some of the blobs and other nuisance objects from
		//the background, we are going to check if the area is large enough to warrant copying.
		if(CurrentBlob->Count > 750) { //Arbitary pixel area size
			//Iterate over the x coordinates of the blob
			for(i = CurrentBlob->Xmin; i < CurrentBlob->Xmax; i++) {
				//And then the y coordinates
				for (j = CurrentBlob->Ymin; j < CurrentBlob->Ymax; j++) {
					//Grab the current pixel...
					P = (Pixel *) &(dFB->Frm[(j*dFB->Width+i)*3]);
					//Arbitrary total pixel coloring threshold.  
					//If the total on the density map is above this threshold,
					//We copy the pixel from the original image to the output image
					if(P->R + P->G + P->B > 175) {
						P = (Pixel *) &(FB->Frm[(j*wFB->Width+i) * 3]);
						Mark_Pixel(i, j + 250, *P, woFB);		
						//Vertical offset by 250 to put in the appropriate 
						//location in the park image
					}
				}
			}
		}
		CurrentBlob = CurrentBlob->Next; //Go to the next blob
	}

	char file[128] = {0}; //Allocate the path variable
	sprintf(file, "trials/00/out%05d.jpg", N); //Format the path properly

	if(DEBUG)
		printf("Outputting results to file: %s \n", file);

	Store_Image(file, woFB);	//Write the final output.

	Free_Blobs(Blobs);	//We're done using the blobs; free them
}
