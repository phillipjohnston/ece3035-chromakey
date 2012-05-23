/*                  Vision Utilities

This library introduces basic data structures and functions for
processing images. It builds on support functions for the Independent
JPEG Group image access library. It also supports sequence library
access functions.

     Linda & Scott Wills                       (c) 2008-2011  */

#include <sys/stat.h>

typedef struct          Pixel {
   unsigned char        R, G, B;
} Pixel;

typedef struct FrmBuf {
   unsigned char       *Frm;
   int                 Height;
   int                 Width;
   struct FrmBuf       *Next;
} FrmBuf;

typedef struct Point {
   int X, Y;
   struct Point *Next;
} Point;

#define BASE_DIR        "./"
#define SEQ_DIR         "./seqs"
#define TRIAL_DIR       "./trials"
#define TRUE            1
#define FALSE           0
#define DEBUG           0    // set to 1 for debug, reset to 0 for speed
#define QUALITY        75    // default image quality
#define RED             0    // pixel offset for Red
#define GREEN           1    // pixel offset for Green
#define BLUE            2    // pixel offset for Blue
#define POINTSBLOCKSIZE 20   // point block size
#define NW              0    // north west quad position
#define NE              1    // north east quad position
#define SW              2    // south west quad position
#define SE              3    // south east quad position
#define FATLINE         1    // make lines thicker

extern void Clear_Frame (FrmBuf *FB);
extern FrmBuf *Alloc_Frame(int Width, int Height);
extern FrmBuf *Create_Frame(char *FileName);
extern FrmBuf *Duplicate_Frame(FrmBuf *Src);
extern void Free_Frame(FrmBuf *FB);
extern void Load_Image(char *FileName, FrmBuf *FB);
extern void Store_Image(char *FileName, FrmBuf *FB);
extern void Copy_Image(FrmBuf *Src, FrmBuf *Dst, int Offset);
extern void Read_Header(char *FileName, int *Width, int *Height);
extern Point *New_Point(int X, int Y);
extern Point *Add_Point(Point *Line, int X, int Y);
extern void Free_Point (Point *Pt);
extern void Free_Line(Point *Line);
extern void Print_Point(Point *M);
extern void Print_Line(Point *M);
extern void Mark_Pixel(int X, int Y, Pixel Color, FrmBuf *FB);
extern void Draw_Hline(int Xmin, int Xmax, int Y, Pixel Color, FrmBuf *FB);
extern void Draw_Vline(int Ymin, int Ymax, int X, Pixel Color, FrmBuf *FB);
extern void Draw_Line(int X1, int Y1, int X2, int Y2, Pixel Color, FrmBuf *FB);
extern void Draw_Multi_Seg_Line(Point *Points, Pixel Color, FrmBuf *FB);
extern void Draw_Rectangle(int Xmin, int Ymin, int Xmax, int Ymax, Pixel Color, FrmBuf *FB);
extern void Draw_Circle(int X0, int Y0, int Radius, Pixel Color, FrmBuf *FB);
extern void Rainbow(unsigned char X, Pixel *P);
extern void RainbowMod(unsigned char X, unsigned char Y, Pixel *P);
extern int InDir(char *Name, char *Dir);
