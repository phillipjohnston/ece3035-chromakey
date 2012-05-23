/*               Rollers (Lite)

This library supports image rollers; fast linear and area density
scanner for locating large contiguous non-zero regions. A routine
"rolls" across an image measuring the number of non-blackened pixels
in a specified linear or area window size. The library also includes
blob detection. A paint function for artificial colorization and a
thresholding function are provided.

(c) 2008-2011 Scott & Linda Wills

Documentation:

This library provides support high efficiency linear and array density
functions, plus fast blob finders. It also supports frame rendering in
artificial color, grayscale, and binary. Below is a usage example.

Key Model Parameters:

Spatial Mask Resolution (WheelSize): This parameter sets the linear or
area mask size for density assessment. Small sizes yield finder detail
in density measures. Larger sizes blur fine details, favoring area
density.

Blob Threshold (Bth): This parameter sets the blob threshold (between
0 and mask size WheelSize^2). Small values expand blob bounds and
reduce voids. Larger values tend to sharpen blob-bloat.

Key Data Structures:

DensityMap: An array of ints chars holding per pixel density values
produced by linear and area density scans. Should be allocated by
caller.

Blob Object: A blob object contains an area (Count), Bounding Box
(Xmin, Xmax, Ymin, Ymax), and ratiometric maintained center of mass
(Xsum/Count, Ysum/Count). Blobs also contain an internal forwarding
pointer.

Key Usage Functions:

=== Density Analysis ===

Horizontal_Image_Density(): Compute horizontal linear non-blackened
pixel density. WheelSize is mask length.

Vertical_Image_Density(): Compute vertical linear non-blackened pixel
density. WheelSize is mask length.

Area_Image_Density(): Compute area non-blackened pixel density
WheelSize is window edge size.

Paint_Frame(): Colorize frame based on scaled density map value.

Paint_Frame_Mod(): Colorize frame based on mod density map value.

Grayscale_Frame(): Grayscale frame based on density map.

Threshold_Frame(): Make frame binary at specified threshold.

=== Blob Finding ===

Blob_List_Length(): Print length of Blob List.

Print_Blob(): Print Single blob object

Print_Blobs(): Print list of blobs.

Mark_Blob_CoM(): Mark center of mass in frame for blob list.

Mark_Blob_BB(): Mark bounding box in frame for blob list.

Free_Blobs(): Free list of blobs.

Blob_Finder(): Find blobs in density map. Bth is threshold.

Blob_Finder_Map(): Find blobs in density map. Also returns a blob
map. Bth is threshold.

Example:

   FrmBuf               *FB;
   int		        Bth = BTH, WSIZE = Wsize, I, NumBlobs = 0;
   int                  *DensityMap;
   Blob                 *Blobs;

   ...
   DensityMap = (int *) malloc(FB->Width * FB->Height * sizeof(int));
   ...
   if (DensityMap == NULL) {
   for (...) {
      ...
      Area_Image_Density(FB, DensityMap, Wsize);
      Blobs = Blob_Finder(DensityMap, FB->Width, FB->Height, Bth);
      Paint_Frame(FB, Wsize*Wsize, DensityMap);
      Mark_Blob_CoM(Blobs, FB);
      Mark_Blob_BB(Blobs, FB);
      Free_Blobs(Blobs);
   }

*/
#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include "rollers.h"

/**********************************************************************
                        Frame Density

This library supports linear and area density analysis on frames.
***********************************************************************/

Pixel W2C16up[] = {{  0,   0,   0},
		   {  0,   0, 128},
		   {128,   0,   0},
		   {  0, 128,   0},
		   {128,   0, 128},
		   {128, 128,   0},
		   {  0, 128, 128},
		   {128, 128, 128},
		   {  0,   0, 255},
		   {192, 192, 192},
		   {255,   0, 255},
		   {  0, 255, 255},
		   {  0, 255,   0},
		   {255, 255,   0},
		   {255,   0,   0},
		   {255, 255, 255}};

Blob                    *FreeBlobs = NULL;            // free blob list

/*               Horizontal Image Density

This routine horizontally scans an input frame containing salient
regions with blackened pixels elsewhere. It returns a preallocated map
of horizontal window fill levels (contiguous salient pixels). */

void Horizontal_Image_Density(FrmBuf *FB, int *DensityMap, int WheelSize) {

   int                   Sum = 0, Wheel = 0, WheelOff, I, J, Edge;

   Edge = 1 << (int) (WheelSize - 1);                 // initialize one in left edge of window
   WheelOff = WheelSize >> 1;                         // window offset = half window size
   for (I = 0; I < FB->Width * FB->Height; I++) {     // for each pixel in frame
      Sum -= Wheel & 1;                               // remove outgoing pixel count
      Wheel >>= 1;                                    // shift window for new pixel
      if ((FB->Frm[3*I] | FB->Frm[3*I+1] | FB->Frm[3*I+2]) != 0) { // if pixel contains non-blackened value
	 Sum += 1;                                    // increment count
	 Wheel |= Edge;                               // and insert new edge pixel count
      }
      if (I % FB->Width >= WheelOff)                  // wait until fully into row
	 DensityMap[I - WheelOff] = Sum;              // write current sum to density map
      if (I % FB->Width == FB->Width - 1) {           // if at end of row
	 for (J = 1; J <= WheelOff; J++) {            // write out end of row
            Sum -= Wheel & 1;                         // remove outgoing pixel count
            Wheel >>= 1;                              // shift window for new pixel
	    DensityMap[I - WheelOff + J] = Sum;       // write diminishing sum
         }
	 Sum = Wheel = 0;                             // clear sum and window at row's end
      }
   }
}

/*               Vertical Image Density

This routine vertically scans an input frame containing salient
regions with blackened pixels elsewhere. It returns a preallocated map
of vertical window fill levels (contiguous salient pixels). */

void Vertical_Image_Density(FrmBuf *FB, int *DensityMap, int WheelSize) {

   int                   Sum = 0, Wheel = 0, WheelOff, X, Y, I, J, Edge;

   Edge = 1 << (int) (WheelSize - 1);                 // initialize one in left edge of window
   WheelOff = (WheelSize >> 1) * FB->Width;           // window offset = half window size x Width
   for (X = 0; X < FB->Width; X++) {                  // for each column
      for (Y = 0; Y < (FB->Width * FB->Height); Y += FB->Width) { // for each row
	 I = X + Y;                                   // compute index
         Sum -= Wheel & 1;                            // remove outgoing pixel count
         Wheel >>= 1;                                 // shift window for new pixel
         if ((FB->Frm[3*I] | FB->Frm[3*I+1] | FB->Frm[3*I+2]) != 0) { // if pixel contains non-blackened value
	    Sum += 1;                                 // increment count
	    Wheel |= Edge;                            // and insert new edge pixel count
         }
         if (Y >= WheelOff)                           // wait until fully into column
	    DensityMap[I - WheelOff] = Sum;           // write current sum to density map
         if (Y == (FB->Height - 1) * FB->Width) {     // if at end of column
	    for (J = FB->Width; J <= WheelOff; J += FB->Width) { // write out end of column
               Sum -= Wheel & 1;                      // remove outgoing pixel count
               Wheel >>= 1;                           // shift window for new pixel
	       DensityMap[I - WheelOff + J] = Sum;    // write diminishing sum
            }
	    Sum = Wheel = 0;                          // clear sum and window at row's end
         }
      }
   }
}

/*               Area Image Density

This routine area (two dimensionally) scans an input frame containing
salient regions with blackened pixels elsewhere. It returns a
preallocated map of window fill levels (contiguous salient pixels).

ToDo: reverse scan order to better exploit spacial locality in data
cache. */

void Area_Image_Density(FrmBuf *FB, int *DensityMap, int WheelSize) {

   int                   Wheels[FB->Height];
   int                   Sums[FB->Height];
   int                   Vwheel[WheelSize];
   int                   Vsum, Vptr, HalfWheel, WheelOff, X, Y, I, Edge;

   Edge = 1 << (int) (WheelSize - 1);                 // initialize one in left edge of window
   HalfWheel = WheelSize >> 1;                        // half wheel size
   WheelOff = HalfWheel * (FB->Width + 1);            // window offset = half window size x Width
   for (Y = 0; Y < FB->Height; Y++) {                 // for all rows
      Wheels[Y] = 0;                                  // clear the wheels
      Sums[Y] = 0;                                    // clear the sums
   }
   for (X = 0; X < FB->Width + HalfWheel; X++) {      // for each column plus write out rows
      for (Vptr = 0; Vptr < WheelSize; Vptr++)        // for all positions in vertical wheel
         Vwheel[Vptr] = 0;                            // clear vertical wheel
      Vsum = Vptr = 0;                                // clear vertical sum and ptr
      for (Y = 0; Y < FB->Height + HalfWheel; Y++) {  // for each row
	 I = X + Y * FB->Width;                       // compute index
         Sums[Y] -= Wheels[Y] & 1;                    // remove outgoing pixel count
         Wheels[Y] >>= 1;                             // shift window for new pixel
         Vsum -= Vwheel[Vptr];                        // removing outgoing vertical wheel count
         if (Y < FB->Height) {                        // while in image column
	    if (X < FB->Width )                       // while in image row
               if (FB->Frm[3*I] | FB->Frm[3*I+1] | FB->Frm[3*I+2]) { // if pixel contains non-blackened value
	          Sums[Y] += 1;                       // increment count
	          Wheels[Y] |= Edge;                  // and insert new edge pixel count
               }
            Vwheel[Vptr] = Sums[Y];                   // insert row sum on vertical wheel
            Vsum += Sums[Y];                          // and add to vertical sum
         }
         Vptr = (Vptr + 1) % WheelSize;               // increment vertical ptr
         if (X >= HalfWheel && Y >= HalfWheel)        // wait until fully into row and column
	    DensityMap[I - WheelOff] = Vsum;          // write current vertical sum to density map
      }
   }
}

/*              Paint Frame

This routine uses a DensityMap to paint a frame using a rainbow paint
function. Its scales the coloration based on the maximum count. */

void Paint_Frame(FrmBuf *FB, int MaxCount, int *DensityMap) {

   int                  I;
   Pixel                P;

   for (I = 0; I < FB->Width * FB->Height; I++) {     // for each pixel
      P = W2C16up[DensityMap[I] * 15 / MaxCount];     // compute scaled color
      FB->Frm[3*I] = P.R;                             // write red component
      FB->Frm[3*I+1] = P.G;                           // write green component
      FB->Frm[3*I+2] = P.B;                           // write blue component
   }
}

/*              Paint Frame Mod

This routine uses a DensityMap to paint a frame using a rainbow paint
function. It mods the map value using the number of colors. */

void Paint_Frame_Mod(FrmBuf *FB, int *DensityMap) {

   int                  I;
   Pixel                P;

   for (I = 0; I < FB->Width * FB->Height; I++) {     // for each pixel
      P = W2C16up[DensityMap[I] % 15];                // compute mod color
      FB->Frm[3*I] = P.R;                             // write red component
      FB->Frm[3*I+1] = P.G;                           // write green component
      FB->Frm[3*I+2] = P.B;                           // write blue component
   }
}

/*              Grayscale Frame

This routine uses a DensityMap to produce a grayscale map of the image
values. The values are scaled to the max count value. */

void Grayscale_Frame(FrmBuf *FB, int MaxCount, int *DensityMap) {

   int                  I;

   for (I = 0; I < FB->Width * FB->Height; I++)       // for each pixel
      FB->Frm[3*I] = FB->Frm[3*I+1] = FB->Frm[3*I+2] = DensityMap[I] * 255 / MaxCount;
}

/*              Threshold Frame

This routine uses a DensityMap to threshold a frame into a binary image. */

void Threshold_Frame(FrmBuf *FB, int Threshold, int *DensityMap) {

  int                  I;

   for (I = 0; I < FB->Width * FB->Height; I++) {     // for each pixel
      if (DensityMap[I] >= Threshold)                 // if density above threshold
	 FB->Frm[3*I] = FB->Frm[3*I+1] = FB->Frm[3*I+2] = 255; // pixel on
      else
	 FB->Frm[3*I] = FB->Frm[3*I+1] = FB->Frm[3*I+2] = 0;   // pixel off
   }
}

/**********************************************************************
                        Blob Finding

This library supports blob identification on a density map.
***********************************************************************/

/*               New Blob

This routine returns a new blob. If none are available, it allocates a
block from the heap and adds them to the free list. The registration
point and ID are supplied when the blob is created. */

Blob *New_Blob(int X, int Y, int ID) {

   Blob                 *NewBlob;

   if (FreeBlobs == NULL) {          // if no free blobs, allocate a block from the heap
      FreeBlobs = (Blob *) malloc(FREEBLOBSBLOCKSIZE * sizeof(Blob));
      if (FreeBlobs == NULL) {
         fprintf(stderr, "Unable to allocate blob\n");
         exit (1);
      }
      for (NewBlob = FreeBlobs; NewBlob < &(FreeBlobs[FREEBLOBSBLOCKSIZE - 1]); NewBlob++)
 	 NewBlob->Next = (NewBlob + 1);
      (FreeBlobs[FREEBLOBSBLOCKSIZE - 1]).Next = NULL;
   }
   NewBlob = FreeBlobs;             // pop a blob from the free list
   FreeBlobs = FreeBlobs->Next;
   NewBlob->Xmin = NewBlob->Xmax = NewBlob->Ymin = NewBlob->Ymax = 0;
   NewBlob->Xsum = NewBlob->Ysum = NewBlob->Count = 0;
   NewBlob->Xreg = X;               // set registration point
   NewBlob->Yreg = Y;
   NewBlob->ID = ID;                // set blob ID
   NewBlob->Expire = -1;
   NewBlob->Next = NewBlob->FP = NULL;
   return (NewBlob);
}

/*              Free Blobs

This routine reclaims a list of blobs, adding them to the free list */

void Free_Blobs(Blob *Blobs) {

   Blob                  *Head = Blobs;

   if (Blobs) {
      while (Blobs->Next)                             // find last blob in list
         Blobs = Blobs->Next;
      Blobs->Next = FreeBlobs;                        // point to head of blob free list
      FreeBlobs = Head;                               // update free blob ptr
   }
}

/*              Number Blobs

This routine numbers all non-forwarded blobs starting from 1. */

void Number_Blobs(Blob *Blobs) {

   int                     ID = 1;

   while (Blobs) {                                    // for each blob
      if (Blobs->FP == NULL) {                        // if not forwarded blob
	 Blobs->ID = ID;                              // set blob ID
	 ID += 1;                                     // increment blob ID
      }
      Blobs = Blobs->Next;                            // go to next blob
   }
}

/*               Blob List Length

This routine returns the length of a blob list. */

int Blob_List_Length(Blob *Blobs) {

   int                  L = 0;

   while(Blobs) {
      L += 1;
      Blobs = Blobs->Next;
   }
   return (L);
}

/*               Print Blob

This routine prints the contents of a blob. */

void Print_Blob(Blob *ThisBlob) {

   printf("   Blob %2d: RegPT= (%3d,%3d) BB= (%3d,%3d)x(%3d,%3d) CoM= (%3d,%3d), area= %d\n",
	  ThisBlob->ID, ThisBlob->Xreg, ThisBlob->Yreg, ThisBlob->Xmin, ThisBlob->Ymin, ThisBlob->Xmax,
	  ThisBlob->Ymax, ThisBlob->Xsum / ThisBlob->Count, ThisBlob->Ysum / ThisBlob->Count, ThisBlob->Count);
}

/*               Print Blobs

This recursive routine prints a blob list. */

void Print_Blobs(Blob *Blobs) {

   if (Blobs) {                                       // if non-NULL list
      Print_Blob(Blobs);                              // print first blob
      Print_Blobs(Blobs->Next);                       // then print rest of list
   }
}

/*               Mark_Blob_CoM

This routine marks the center of mass for each blob on a frame. The
mark is a green plus. Forwarding pointers are marked as a red pixel. */

void Mark_Blob_CoM(Blob *Blobs, FrmBuf *FB) {

  int                  X, Y;
  Pixel                R = {255, 0, 0}, G = {0, 255, 0};

   while (Blobs) {
      X = Blobs->Xsum / Blobs->Count;
      Y = Blobs->Ysum / Blobs->Count;
      if (Blobs->FP)
	 Mark_Pixel(X, Y, R, FB);
      else {
	 Mark_Pixel(X, Y,   G, FB);
	 Mark_Pixel(X-1, Y, G, FB);
	 Mark_Pixel(X, Y-1, G, FB);
	 Mark_Pixel(X+1, Y, G, FB);
	 Mark_Pixel(X, Y+1, G, FB);
      }
      Blobs = Blobs->Next;
   }
}

/*               Mark_Blob_BB

This routine marks the bounding box for each blob on a frame. The
bounding box is drawn in yellow. Forward pointer bounding boxes are
drawn in red. */

void Mark_Blob_BB(Blob *Blobs, FrmBuf *FB) {


   Pixel               R = {255, 0, 0}, Y = {255, 255, 0};

   while (Blobs) {
      if (Blobs->FP)
	 Draw_Rectangle(Blobs->Xmin, Blobs->Ymin, Blobs->Xmax, Blobs->Ymax, R, FB);
      else
	 Draw_Rectangle(Blobs->Xmin, Blobs->Ymin, Blobs->Xmax, Blobs->Ymax, Y, FB);
      Blobs = Blobs->Next;
   }
}

/*               Reduce Forwarding Pointer

This recursive routine reduces a forwarding pointer, returning a
pointer to the vectored blob object. */

Blob *Reduce_FP(Blob *ThisBlob) {

   if (ThisBlob->FP)                                  // if forwarding pointer
      return (Reduce_FP(ThisBlob->FP));               // then reduce to vectored blob
   else 
      return (ThisBlob);                              // else return ptr to blob
}

/*              Merge Blobs

This routine merges one blob into another. The first blob should be
reduced (not a fwd ptr) before it is merged. The second blob assumes
the first blobs positions. The blob registration points are combined.
The first blob is then converted into a forwarding pointer with an
expiration value (for reclaiming). */

void Merge_Blobs(Blob *Blob1, Blob *Blob2, int Expire) {

   if (Blob1 != Blob2) {                              // if first blob is not second blob
      if (Blob1->Xmin < Blob2->Xmin)                  // if new minimum X
         Blob2->Xmin = Blob1->Xmin;                   // update
      if (Blob1->Ymin < Blob2->Ymin)                  // if new minimum X
         Blob2->Ymin = Blob1->Ymin;                   // update
      if (Blob1->Xmax > Blob2->Xmax)                  // if new maximum X
         Blob2->Xmax = Blob1->Xmax;                   // update
      if (Blob1->Ymax > Blob2->Ymax)                  // if new maximum X
         Blob2->Ymax = Blob1->Ymax;                   // update
      if (Blob1->Yreg < Blob2->Yreg ||
	  (Blob1->Yreg == Blob2->Yreg && Blob1->Xreg < Blob2->Yreg)) { // find upper left reg point
	 Blob2->Xreg = Blob1->Xreg;
	 Blob2->Yreg = Blob1->Yreg;
      }
      Blob2->Xsum += Blob1->Xsum;                     // add center of mass sum
      Blob2->Ysum += Blob1->Ysum;                     // add center of mass sum
      Blob2->Count += Blob1->Count;                   // add area count
      Blob1->FP = Blob2;                              // vector second blob
      Blob1->Expire = Expire;                         // set expiration date
   }
}

/*              Add Position

This routine adds a position to a blob. */

void Add_Position(Blob *ThisBlob, int X, int Y) {

   if (ThisBlob->Count == 0) {                        // if first position
      ThisBlob->Xmin = ThisBlob->Xmax = X;            // then initialize X BB
      ThisBlob->Ymin = ThisBlob->Ymax = Y;            // then initialize Y BB
   } else {                                           // otherwise check min/max
      if (X < ThisBlob->Xmin)                         // if new minimum X
         ThisBlob->Xmin = X;                          // update
      if (Y < ThisBlob->Ymin)                         // if new minimum Y
         ThisBlob->Ymin = Y;                          // update
      if (X > ThisBlob->Xmax)                         // if new maximum X
         ThisBlob->Xmax = X;                          // update
      if (Y > ThisBlob->Ymax)                         // if new maximum Y
         ThisBlob->Ymax = Y;                          // update
   }
   ThisBlob->Xsum += X;                               // add X to center of mass sum
   ThisBlob->Ysum += Y;                               // add Y to center of mass sum
   ThisBlob->Count += 1;                              // increment area count
}

/*              Reap Expired Blobs

This routine reclaims expired blob objects, adding them to the blob
free list. Blobs are collected when their expiration time matches the
current X value. This guarantees that all column blob fwd ptrs have
been reduced before the blob object is reclaimed. The active blob list
is returned. */

Blob *Reap_Expired_Blobs(Blob *Blobs, int Now) {

   Blob                  *Head = Blobs, **Trail = &Head;

   while (Blobs)
      if (Now == Blobs->Expire) {                     // if expired blob object
	 *Trail = Blobs->Next;                        // splice it out of blob list
         Blobs->Next = FreeBlobs;                     // push on free blob list
         FreeBlobs = Blobs;                           // update free blob ptr
         Blobs = *Trail;                              // move to next blob in active blob list
      } else {
	 Trail = &(Blobs->Next);                      // advance trailing ptr
	 Blobs = Blobs->Next;                         // advance active blob list ptr
      }
   return (Head);                                     // return head of active blob list
}

/*              Reap Forwarded Blobs

This routine reclaims forwarded blob objects, adding them to the blob
free list. The active blob list is returned. */

Blob *Reap_FP_Blobs(Blob *Blobs) {

   Blob                  *Head = Blobs, **Trail = &Head;

   while (Blobs)
      if (Blobs->FP) {                                // if blob is forwarded
	 *Trail = Blobs->Next;                        // splice it out of blob list
         Blobs->Next = FreeBlobs;                     // push on free blob list
         FreeBlobs = Blobs;                           // update free blob ptr
         Blobs = *Trail;                              // move to next blob in active blob list
      } else {
	 Trail = &(Blobs->Next);                      // advance trailing ptr
	 Blobs = Blobs->Next;                         // advance active blob list ptr
      }
   return (Head);                                     // return head of active blob list
}

/*              Mark_Blob_ID_Map

This routine replaces all blob pointers in blob map with the ID of the
base blob. The base blob is the blob that does not contain a
forwarding pointer. */

void Mark_Blob_ID_Map(int *DensityMap, int NumPos) {

   Blob                  *ThisBlob;
   int                   I;

   for (I = 0; I < NumPos; I++)                       // for each position in density map
      if (DensityMap[I]) {                            // if position is non-zero
	 ThisBlob = (Blob *) DensityMap[I];           // extract blob pointer
	 ThisBlob = Reduce_FP(ThisBlob);              // find base blob
	 DensityMap[I] = ThisBlob->ID;                // set map to base blob ID
      }
}

/*              Blob Finder

This routine identifies and returns blobs of salient regions in an
area density map. Blob threshold Bth defines density threshold. A list
of blob objects is returned. */

Blob *Blob_Finder(int *DensityMap, int Width, int Height, int Bth) {

   Blob                  *Blobs = NULL, *RowBlob;
   Blob                  *ColBlobs[Width];
   int                   X, Y, I = 0;

   for (X = 0; X < Width; X++)                        // for all columns
      ColBlobs[X] = NULL;                             // clear column blobs
   for (Y = 0; Y < Height; Y++) {                     // for each row
      RowBlob = NULL;                                 // clear row blob
      for (X = 0; X < Width; X++) {                   // for each row offset
         if (ColBlobs[X] && ColBlobs[X]->FP)          // if column blob is a fwd ptr
	    ColBlobs[X] = Reduce_FP(ColBlobs[X]);     // then reduce to the vectored blob
         if (DensityMap[I] >= Bth) {                  // if blob-worthy
	    if (RowBlob && ColBlobs[X])               // if two blobs touch
	       Merge_Blobs(ColBlobs[X], RowBlob, Y+1); // then merge column blob into rowblob
            else if (ColBlobs[X])                     // if only column blob exists
	       RowBlob = ColBlobs[X];                 // copy to row blob
            else if (RowBlob == NULL) {               // if no current blobs
	       RowBlob = New_Blob(X,Y, 0);            // allocate a new blob
               RowBlob->Next = Blobs;                 // push new blob onto blob list
               Blobs = RowBlob;                       // update blob list
            }
	    Add_Position(RowBlob, X, Y);              // add position to current blob
            ColBlobs[X] = RowBlob;                    // update column blob
	 } else                                       // else leaving blob
	    RowBlob = ColBlobs[X] = NULL;             // clear current blob pointers
	 I += 1;                                      // adjust map index
      }
      Blobs = Reap_Expired_Blobs(Blobs, Y);           // reap expired blobs
   }
   Blobs = Reap_FP_Blobs(Blobs);                      // eliminate residual fwd ptrs from last row
   Number_Blobs(Blobs);                               // set blob IDs
   return (Blobs);                                    // return blob list
}

/*              Blob Finder Map

This routine identifies and returns blobs of salient regions in an
area density map just like Blob Finder. Blob threshold Bth defines
density threshold. The function replaces the density map values with a
blob ID map that identifies the locations of each blob in the returned
blob list. The algorithm first annotates all density map values with
pointers to the corresponding blob. Following the map scan and blob ID
assignment, the map is flattened through forwarding pointers until a
non-forwarded blob is found and its ID is used to replace the blob
pointer in the blob map. */

Blob *Blob_Finder_Map(int *DensityMap, int Width, int Height, int Bth) {

   Blob                  *Blobs = NULL, *RowBlob;
   Blob                  *ColBlobs[Width];
   int                   X, Y, I = 0;

   for (X = 0; X < Width; X++)                        // for all columns
      ColBlobs[X] = NULL;                             // clear column blobs
   for (Y = 0; Y < Height; Y++) {                     // for each row
      RowBlob = NULL;                                 // clear row blob
      for (X = 0; X < Width; X++) {                   // for each row offset
         if (ColBlobs[X] && ColBlobs[X]->FP)          // if column blob is a fwd ptr
	    ColBlobs[X] = Reduce_FP(ColBlobs[X]);     // then reduce to the vectored blob
         if (DensityMap[I] >= Bth) {                  // if blob-worthy
	    if (RowBlob && ColBlobs[X])               // if two blobs touch
	       Merge_Blobs(ColBlobs[X], RowBlob, 0);  // then merge column blob into rowblob
            else if (ColBlobs[X])                     // if only column blob exists
	       RowBlob = ColBlobs[X];                 // copy to row blob
            else if (RowBlob == NULL) {               // if no current blobs
	       RowBlob = New_Blob(X,Y, 0);            // allocate a new blob
               RowBlob->Next = Blobs;                 // push new blob onto blob list
               Blobs = RowBlob;                       // update blob list
            }
	    Add_Position(RowBlob, X, Y);              // add position to current blob
            ColBlobs[X] = RowBlob;                    // update column blob
	    DensityMap[I] = (int) RowBlob;            // store blob address in blob map
	 } else {                                     // else leaving blob
	    DensityMap[I] = 0;                        // clear blob map for unblob-worthy positions
	    RowBlob = ColBlobs[X] = NULL;             // clear current blob pointers
	 }
	 I += 1;                                      // adjust map index
      }
   }
   Number_Blobs(Blobs);                               // set ID on non-forwarded blobs
   Mark_Blob_ID_Map(DensityMap, Width * Height);      // transform blob pointers to base ID
   Blobs = Reap_FP_Blobs(Blobs);                      // eliminate residual fwd ptrs from last row
   return (Blobs);                                    // return blob list
}
