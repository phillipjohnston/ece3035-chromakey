/*                     Rollers (Lite)

This library supports image rollers; a fast horizontal linear density
scanner for locating large contiguous non-zero regions. A routine
"rolls" across an image measuring the number of non-blackened pixels
in a specified window size. A rainbow paint function provide
artificial colorization for windows up to 16 pixel long.

(c) 2008-2011 Scott & Linda Wills                         */

typedef struct          Blob {
   int                  ID, Xsum, Ysum, Xmin, Ymin, Xmax, Ymax, Count, Xreg, Yreg, Expire;
   struct Blob          *FP, *Next;
}  Blob;

#define                 FREEBLOBSBLOCKSIZE 20

extern Blob *FreeBlobs;

extern void Horizontal_Image_Density(FrmBuf *FB, int *DensityMap, int WheelSize);
extern void Vertical_Image_Density(FrmBuf *FB, int *DensityMap, int WheelSize);
extern void Area_Image_Density(FrmBuf *FB, int *DensityMap, int WheelSize);
extern void Paint_Frame(FrmBuf *FB, int MaxCount, int *DensityMap);
extern void Paint_Frame_Mod(FrmBuf *FB, int *DensityMap);
extern void Grayscale_Frame(FrmBuf *FB, int MaxCount, int *DensityMap);
extern void Threshold_Frame(FrmBuf *FB, int Threshold, int *DensityMap);
extern Blob *New_Blob(int X, int Y, int ID);
extern void Free_Blobs(Blob *Blobs);
extern void Number_Blobs(Blob *Blobs);
extern int Blob_List_Length(Blob *Blobs);
extern void Print_Blob(Blob *ThisBlob);
extern void Print_Blobs(Blob *Blobs);
extern void Mark_Blob_CoM(Blob *Blobs, FrmBuf *FB);
extern void Mark_Blob_BB(Blob *Blobs, FrmBuf *FB);
extern Blob *Reduce_FP(Blob *ThisBlob);
extern void Merge_Blobs(Blob *Blob1, Blob *Blob2, int Expire);
extern void Add_Position(Blob *ThisBlob, int X, int Y);
extern Blob *Reap_Expired_Blobs(Blob *Blobs, int Now);
extern Blob *Reap_FP_Blobs(Blob *Blobs);
extern void Mark_Blob_ID_Map(int *DensityMap, int NumPos);
extern Blob *Blob_Finder(int *DensityMap, int Width, int Height, int Bth);
extern Blob *Blob_Finder_Map(int *DensityMap, int Width, int Height, int Bth);
