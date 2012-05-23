/*                  Vision Utilities

This library introduces basic data structures and functions for
processing images. It builds on support functions for the Independent
JPEG Group image access library. It also supports sequence library
access functions.

     Linda & Scott Wills                       (c) 2008-2011

Key Data Structures:

Frame Buffer (FrmBuf): An array of unsigned chars (bytes) equal to the
number of pixels in an image times three (RGB). Typically frame
buffers are dynamically allocated in the heap once the image size is
known. The frame buffer struct includes its width and height. Frame
buffers are managed explicitly.

Point: An point object contains an X,Y position as two integers plus a
Next pointer to support lists of points. Points are used for
representing multi-segment lines.

Key Functions:

Clear_Frame(): This function clears the frame buffer array.

Alloc_Frame(): This function allocates and returns a frame buffer of a
specified height and width. The frame buffer data is cleared. NOTE:
the returned frame buffer should be deallocated when it is no longer
needed.

Create_Frame(): This function allocates a frame buffer (using the height
and width specified in the image) and then initializes it with the
decoded image. NOTE: the returned frame buffer should be deallocated
when it is no longer needed.

Duplicate_Frame(): This function duplicates a existing frame buffer
and returns the newly allocated and initialized frame buffer.

Free_Frame(): This function deallocates a frame buffer.

Load_Image(): This function loads and decodes an JPEG image into a
preallocated frame buffer.

Store_Image(): This function encodes and stores a frame buffer as an
output JPEG image file.

Copy_Image(): This function copies an image from a source buffer to a
destination buffer. The target frame buffer must be large enough to
accommodate the frame buffer being copied. An tile offset allows the
source image to be placed in a pane of a larger window.

Draw_Multi_Seg_Line(): This function draws a multi-segment line
constructed out of points.

Draw_Line(): This function draws a line in a frame buffer.

Draw_Rectangle(): This function draws a rectangle in a frame buffer.

Draw_Circle(): This function draws a filled circle in a frame buffer.

<depreciated functions>

Read_Header(): This function opens an image and returns its height and
width.

*/

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <jpeglib.h>
#include "utils.h"

Point               *FreePoints = NULL;           /* free points list */
FrmBuf              *FreeFrames = NULL;           /* free frame list */

/*              Clear Frame

This routine clears the pixel data of a frame buffer. */

void Clear_Frame (FrmBuf *FB) {
   int              N;

   for (N = 0; N < 3 * FB->Width * FB->Height; N++)
      FB->Frm[N] = 0;
}

/*              Allocate Frame

This routine allocates a new frame buffer of a specified width and
height. A new frame object and a pixel array are allocated, either
from the free frame list, or if unavailable, from the heap. The frame
buffer is cleared. If the frame buffer cannot be allocated, Null is
returned. */

FrmBuf *Alloc_Frame(int Width, int Height) {
   FrmBuf           *FB = NULL, *LastFB;

   if (FreeFrames) {   // if recycled frame buffers exist, check if proper size is available.       
      LastFB = FB = FreeFrames;
      while (FB)
	 if (FB->Width == Width && FB->Height == Height) {
	    if (FB == LastFB)
	       FreeFrames = FB->Next;
	    else
	       LastFB->Next = FB->Next;
	    break;
	 } else {
	    LastFB = FB;
	    FB = FB->Next;
	 }
   }
   if (FB == NULL) {   // if recycled frame buffer not available, then allocate one from heap.
      FB = (FrmBuf *) malloc(sizeof(FrmBuf));
      if (FB == NULL)
	 return(FB);
      FB->Frm = (unsigned char *) malloc(3 * Width * Height * sizeof(unsigned char));
      if (FB == NULL)
	 return (NULL);
      FB->Width = Width;
      FB->Height = Height;
   }
   FB->Next = NULL;   // either way, set next ptr to NULL
   Clear_Frame(FB);   // clear frame data
   return (FB);
}

/*              Create Frame

This routine loads and decodes a JPEG image, storing it in a new frame
buffer. If the image file cannot be opened, or if the frame buffer
cannot be allocated, an error message is printed and execution
terminates. */

FrmBuf *Create_Frame(char *FileName) {
   struct jpeg_decompress_struct       cinfo;
   struct jpeg_error_mgr               jerr;
   FILE                                *FP;
   int                                 Width, Height, Row;
   JSAMPROW                            *RowPtr;
   FrmBuf                              *FB;

   cinfo.err = jpeg_std_error(&jerr);
   jpeg_create_decompress(&cinfo);
   FP = fopen(FileName, "rb");
   if (FP == NULL) {
      fprintf(stderr, "ERROR: %s cannot be opened\n", FileName);
      exit(1);
   }
   jpeg_stdio_src(&cinfo, FP);
   jpeg_read_header(&cinfo, TRUE);
   cinfo.out_color_space = JCS_RGB;
   jpeg_start_decompress(&cinfo);
   Width = cinfo.output_width;
   Height = cinfo.output_height;
   FB = Alloc_Frame(Width, Height);
   if (FB == NULL) {
      fprintf(stderr, "ERROR: frame buffer cannot be allocated\n");
      exit(1);
   }
   for (Row = 0; Row < 3 * Height * Width; Row += 3 * Width) {
      RowPtr = (JSAMPROW *) &(FB->Frm[Row]);
      jpeg_read_scanlines(&cinfo, (JSAMPARRAY) &RowPtr, 1);
   }
   jpeg_finish_decompress(&cinfo);
   fclose(FP);
   return (FB);
}

/*              Duplicate Frame

This routine creates a new, identical frame buffer with the same image
data. The returned frame has been allocated and should be freed when
no longer required. If the frame buffer cannot be allocated, an error
message is printed and execution terminates.*/

FrmBuf *Duplicate_Frame(FrmBuf *Src) {
   FrmBuf               *Dst;

   Dst = Alloc_Frame(Src->Width, Src->Height);
   if (Dst == NULL) {
      fprintf(stderr, "ERROR: frame buffer cannot be allocated\n");
      exit(1);
   }
   Copy_Image(Src, Dst, 0);
   return(Dst);
}

/*              Print Free Frames

This routine prints all frame buffers on the free lists. */

void Print_Free_Frames() {
   FrmBuf                              *FB = FreeFrames;

   while (FB) {
      printf("%u: (%d,%d) next=%u\n", (unsigned int) FB, FB->Width, FB->Height, (unsigned int) FB->Next);
      FB = FB->Next;
   }
   printf("\n");
}

/*              Free Frame

This routine deallocates a frame object (including the data array) by
pushing it on the free frame list. */

void Free_Frame(FrmBuf *FB) {

   FB->Next = FreeFrames;
   FreeFrames = FB;
   //   Print_Free_Frames();
}

/*              Load Image

This routine loads and decodes a JPEG image, storing it in a
preallocated frame buffer. If the image file cannot be opened, or if
the image size exceeds the frame buffer, an error message is printed
and execution terminates. */

void Load_Image(char *FileName, FrmBuf *FB) {
   struct jpeg_decompress_struct       cinfo;
   struct jpeg_error_mgr               jerr;
   FILE                                *FP;
   int                                 Width, Height, Row;
   JSAMPROW                            *RowPtr;

   cinfo.err = jpeg_std_error(&jerr);
   jpeg_create_decompress(&cinfo);
   FP = fopen(FileName, "rb");
   if (FP == NULL) {
      fprintf(stderr, "ERROR: %s cannot be opened\n", FileName);
      exit(1);
   }
   jpeg_stdio_src(&cinfo, FP);
   jpeg_read_header(&cinfo, TRUE);
   cinfo.out_color_space = JCS_RGB;
   jpeg_start_decompress(&cinfo);
   Width = cinfo.output_width;
   Height = cinfo.output_height;
   if (Width > FB->Width || Height > FB->Height) {
      fprintf(stderr, "ERROR: image size (%d,%d) exceeds frame buffer size (%d,%d)\n", \
	      Width, Height, FB->Width, FB->Height);
      exit(1);
   }
   for (Row = 0; Row < 3 * Height * Width; Row += 3 * Width) {
      RowPtr = (JSAMPROW *) &(FB->Frm[Row]);
      jpeg_read_scanlines(&cinfo, (JSAMPARRAY) &RowPtr, 1);
   }
   jpeg_finish_decompress(&cinfo);
   fclose(FP);
}

/*              Store Image

This routine stores a frame buffer as a JPEG image. If the image file
cannot be opened, an error message is printed and execution
terminates. */

void Store_Image(char *FileName, FrmBuf *FB) {
   struct jpeg_compress_struct		cinfo;
   struct jpeg_error_mgr		jerr;
   FILE					*FP;
   JSAMPROW 				RowPtr[1];
   int					RowStride;

   cinfo.err = jpeg_std_error(&jerr);
   jpeg_create_compress(&cinfo);
   if ((FP = fopen(FileName, "wb")) == NULL) {
      fprintf(stderr, "can't open %s\n", FileName);
      exit(1);
   }
   jpeg_stdio_dest(&cinfo, FP);
   cinfo.image_width = FB->Width; 		/* image width and height, in pixels */
   cinfo.image_height = FB->Height;
   cinfo.input_components = 3;			/* # of color components per pixel */
   cinfo.in_color_space = JCS_RGB; 		/* colorspace of input image */
   jpeg_set_defaults(&cinfo);
   jpeg_set_quality(&cinfo, QUALITY, TRUE);	/* limit to baseline-JPEG values */
   jpeg_start_compress(&cinfo, TRUE);
   RowStride = FB->Width * 3;			/* JSAMPLEs per row in image_buffer */
   while (cinfo.next_scanline < cinfo.image_height) {
      RowPtr[0] = (JSAMPROW) &(FB->Frm[cinfo.next_scanline * RowStride]);
      (void) jpeg_write_scanlines(&cinfo, RowPtr, 1);
   }
   jpeg_finish_compress(&cinfo);
   fclose(FP);
   jpeg_destroy_compress(&cinfo);
}

/*              Copy Image

This routine stores one frame buffer onto another. Both frame buffers
are preallocated. It is possible for the copied image to be offset
within the destination frame. An Offset value of 0 is appropriate for
copying for equal sized frame buffers. If the destination frame is
larger than the source image, the offset defines a pane within the
larger window using row/column ordering. */

void Copy_Image(FrmBuf *Src, FrmBuf *Dst, int Offset) {
   int                  NumTilesX = Dst->Width /  Src->Width;       // number of tiles in row of window
   int                  Tx = (Offset % NumTilesX) * Src->Width;    // base X pixel offset
   int                  Ty = (Offset / NumTilesX) * Src->Height;   // base Y pixel offset
   int                  X, Y, I, J;

   for (Y = 0; Y < Src->Height; Y++)
      for (X = 0; X < Src->Width; X++) {
	 I = (Y * Src->Width + X) * 3;                 // pixel offset for source
         J = ((Ty + Y) * Dst->Width + Tx + X) * 3;     // pixel offset for destination
         Dst->Frm[J] = Src->Frm[I];
         Dst->Frm[J+1] = Src->Frm[I+1];
         Dst->Frm[J+2] = Src->Frm[I+2];
      }
}

/*                 Read Header

This depreciated routine returns an jpeg image header, returning its
width and height. */

void Read_Header(char *FileName, int *Width, int *Height) {
   struct jpeg_decompress_struct       cinfo;
   struct jpeg_error_mgr               jerr;
   FILE                                *FP;

   cinfo.err = jpeg_std_error(&jerr);
   jpeg_create_decompress(&cinfo);
   FP = fopen(FileName, "rb");
   if (FP == NULL) {
      fprintf(stderr, "ERROR: %s cannot be opened\n", FileName);
      exit(1);
   }
   jpeg_stdio_src(&cinfo, FP);
   jpeg_read_header(&cinfo, TRUE);
   jpeg_start_decompress(&cinfo);
   *Width = cinfo.output_width;
   *Height = cinfo.output_height;
   if (cinfo.out_color_components != 3) {
      fprintf(stderr, "ERROR: %d color components; wrong image type\n", cinfo.out_color_components);
      exit(1);
   }
   jpeg_destroy_decompress(&cinfo);
   fclose(FP);
}

/*               New Point

This routine unitizes and returns a new point object. If none are
available, it allocates a block from the heap and adds them to the
free points list. */

Point *New_Point(int X, int Y) {

   Point                *NewPoint;

   if (FreePoints == NULL) {                        // allocate block of points for free list
      FreePoints = (Point *) malloc(POINTSBLOCKSIZE * sizeof(Point));
      if (FreePoints == NULL) {
         fprintf(stderr, "Unable to allocate points\n");
         exit (1);
      }
      for (NewPoint = FreePoints; NewPoint < &(FreePoints[POINTSBLOCKSIZE - 1]); NewPoint++)
         NewPoint->Next = NewPoint + 1;
      (FreePoints[POINTSBLOCKSIZE - 1]).Next = NULL;
   }
   NewPoint = FreePoints;                          // new point is head of free point list
   FreePoints = FreePoints->Next;                  // free point list is updated
   NewPoint->X = X;                                // set point X
   NewPoint->Y = Y;                                // set point Y
   NewPoint->Next = NULL;                          // next ptr set to null
   return (NewPoint);
}

/*                Add Point

This routine creates a new point and adds it to linked list (a
line). The new line is returned. */

Point *Add_Point(Point *Line, int X, int Y) {

   Point                *Pt = New_Point(X,Y);

   Pt->Next = Line;
   return (Pt);
}

/*               Free Point

This routine frees a point by adding it to the free list. */

void Free_Point (Point *Pt) {

   if (Pt) {
      Pt->Next = FreePoints;
      FreePoints = Pt;
   }
}

/*               Free Line

This routine frees a list of points (a line). */

void Free_Line(Point *Line) {

   Point                *End = Line;

   if (Line) {
      while (End->Next)
	 End = End->Next;
      End->Next = FreePoints;
      FreePoints = Line;
   }
}

/*               Print Point

This routine prints the contents of a marker. */

void Print_Point(Point *P) {
   printf("(%d,%d)", P->X, P->Y);
}

/*               Print Line

This routine prints a point list. */

void Print_Line(Point *P) {

   while (P) {                          // while more points
      Print_Point(P);                   // print the next point
      if (P->Next)                      // if more points to come
	 printf(",");                   // print connecting text
      P = P->Next;                      // then move to next one
   }
}

/*               Mark Pixel

This routine marks a pixel with a specified value. If the
FATLINE flag is set, the line is drawn thicker. */

void Mark_Pixel(int X, int Y, Pixel Color, FrmBuf *FB) {

   int                  I;

   I = 3 * (Y * FB->Width + X);
   FB->Frm[I]   = Color.R;
   FB->Frm[I+1] = Color.G;
   FB->Frm[I+2] = Color.B;
   if (FATLINE) {
      I -= 3;
      if (X > 0) {
	 FB->Frm[I]   = Color.R;
	 FB->Frm[I+1] = Color.G;
	 FB->Frm[I+2] = Color.B;
      }
      I += 6;
      if (X + 1 < FB->Width) {
	 FB->Frm[I]   = Color.R;
	 FB->Frm[I+1] = Color.G;
	 FB->Frm[I+2] = Color.B;
      }
      I -= 3 * FB->Width + 3;
      if (Y > 0) {
	 FB->Frm[I]   = Color.R;
	 FB->Frm[I+1] = Color.G;
	 FB->Frm[I+2] = Color.B;
      }
      I += 6 * FB->Width;
      if (Y + 1 < FB->Height) {
	 FB->Frm[I]   = Color.R;
	 FB->Frm[I+1] = Color.G;
	 FB->Frm[I+2] = Color.B;
      }
   }
}

/*               Draw Horizontal Line

This routine draws a horizontal line in a specified color on a frame
buffer. */

void Draw_Hline(int Xmin, int Xmax, int Y, Pixel Color, FrmBuf *FB) {

  int                 X;

  if (Xmin > Xmax) {
     X = Xmin;
     Xmin = Xmax;
     Xmax = X;
  }
  for (X = Xmin; X <= Xmax; X++)
     Mark_Pixel(X, Y, Color, FB);
}

/*               Draw Vertical Line

This routine draws a vertical line in a specified color on a frame
buffer. */

void Draw_Vline(int Ymin, int Ymax, int X, Pixel Color, FrmBuf *FB) {

  int                 Y;

  if (Ymin > Ymax) {
     Y = Ymin;
     Ymin = Ymax;
     Ymax = Y;
  }
  for (Y = Ymin; Y <= Ymax; Y++)
     Mark_Pixel(X, Y, Color, FB);
}

/*               Draw Line

An implementation of Bresenham's line drawing algorithm (from textbook). */

void Draw_Line(int X1, int Y1, int X2, int Y2, Pixel Color, FrmBuf *FB) {

   int                 DeltaX, DeltaY, IncX, IncY, Error;

   if (X1 < 0 || X2 < 0 || Y1 < 0 || Y2 < 0 ||
       X1 >= FB->Width || X2 >= FB->Width ||
       Y1 >= FB->Height || Y2 >= FB->Height)
      printf("Error: line (%d,%d),(%d,%d) falls out of frame\n", X1, Y1, X2, Y2);
   else {
      if (X2 > X1)                          // compute X increment
	 IncX = 1;
      else
	 IncX = -1;
      if (Y2 > Y1)                          // compute Y increment
	 IncY = 1;
      else
	 IncY = -1;
      DeltaX = abs(X2 - X1);                // compute X delta
      DeltaY = abs(Y2 - Y1);                // compute Y delta
      if (DeltaX >= DeltaY) {               // if slope < 1
	 DeltaY <<= 1;                      // divide Y delta in half
	 Error = DeltaY - DeltaX;           // initialize error
	 DeltaX <<= 1;                      // divide X delta in half
	 while (X1 != X2) {                 // until endpoint is reached
	    Mark_Pixel (X1, Y1, Color, FB); // plot point
	    if (Error >= 0) {               // if error goes positive
	       Y1 += IncY;                  // step Y
	       Error -= DeltaX;             // and subtract X delta
	    }
	    Error += DeltaY;                // on each step, add y delta
	    X1 += IncX;                     // and keep stepping towards X2
	 }
      } else {                              // else if slope >- 1
	 DeltaX <<= 1;                      // divide X delta in half
	 Error = DeltaX - DeltaY;           // initialize error
	 DeltaY <<= 1;                      // divide Y delta in half
	 while (Y1 != Y2) {                 // until endpoint is reached
	    Mark_Pixel (X1, Y1, Color, FB); // plot point
	    if (Error >= 0) {               // if errors goes positive
	       X1 += IncX;                  // step X
	       Error -= DeltaY;             // and subtract y delta
	    }
	    Error += DeltaX;                // on each step, add X delta
	    Y1 += IncY;                     // and keep stepping towards Y2
	 }
      }
      Mark_Pixel (X1, Y1, Color, FB);       // plot endpoint
   }
}

/*            Draw Multi-Segment Line

This routine draws a multi segment line in a frame buffer. The line
is specified by a list of two or more point objects. */

void Draw_Multi_Seg_Line(Point *Points, Pixel Color, FrmBuf *FB) {

   Point               *P1, *P2; 

   while (Points && Points->Next) {
      P1 = Points;
      P2 = Points->Next;
      Draw_Line(P1->X, P1->Y, P2->X, P2->Y, Color, FB);
      Points = Points->Next;
   }
}

/*               Draw Rectangle Line

This routine draws a rectangle in a specified color on a frame
buffer. */

void Draw_Rectangle(int Xmin, int Ymin, int Xmax, int Ymax, Pixel Color, FrmBuf *FB) {

   Draw_Hline(Xmin, Xmax, Ymin, Color, FB);
   Draw_Hline(Xmin, Xmax, Ymax, Color, FB);
   Draw_Vline(Ymin, Ymax, Xmin, Color, FB);
   Draw_Vline(Ymin, Ymax, Xmax, Color, FB);
}

/*               Draw Circle

This routine draws a filled circle, centered at (X0, Y0) using
Bresenham's algorithm. The fill color is specified by a pixel. */

void Draw_Circle(int X0, int Y0, int Radius, Pixel Color, FrmBuf *FB) {

   int                 Error = 1 - Radius;
   int                 DeltaX = 0;
   int                 DeltaY = -2 * Radius;
   int                 X = 0;
   int                 Y = Radius;

   Draw_Vline(Y0 - Radius, Y0 + Radius, X0, Color, FB);
   Draw_Hline(X0 - Radius, X0 + Radius, Y0, Color, FB);
   while(X < Y) {
      if(Error >= 0) {
         Y -= 1;
         DeltaY += 2;
         Error += DeltaY;
      }
      X += 1;
      DeltaX += 2;
      Error += DeltaX + 1;    
      Draw_Hline(X0 - X, X0 + X, Y0 + Y, Color, FB);
      Draw_Hline(X0 - X, X0 + X, Y0 - Y, Color, FB);
      Draw_Hline(X0 - Y, X0 + Y, Y0 + X, Color, FB);
      Draw_Hline(X0 - Y, X0 + Y, Y0 - X, Color, FB);
   }
}

/*                    Rainbow

This routine translates an unsigned char into a full intensity rainbow
spectrum from blue (X=0)to red (X=255). Purple is not included to
avoid wrap-around ambiguity. */

void Rainbow(unsigned char X, Pixel *P) {

   if (X < 64) {
      P->R = 0;
      P->G = X << 2;
      P->B = 255;
   } else if (X < 128) {
      P->R = 0;
      P->G = 255;
      P->B = 255 - ((X % 64) << 2);
   } else if (X < 192) {
      P->R = (X % 64) << 2;
      P->G = 255;
      P->B = 0;
   } else {
      P->R = 255;
      P->G = 255 - ((X % 64) << 2);
      P->B = 0;
   }
}

/*                 RainbowMod

This routine translates two unsigned chars into a modulated
rainbow spectrum (hue and intensity) at full saturation. Hue ranges
from blue (X=0) to red (X=255). Purple is not included to avoid
wrap-around ambiguity. Intensity ranges from black (Y=0) to maximum
(Y=255). */

void RainbowMod(unsigned char X, unsigned char Y, Pixel *P) {

   Rainbow(X, P);
   P->R = (P->R * Y) << 8;
   P->G = (P->G * Y) << 8;
   P->B = (P->B * Y) << 8;
}

/* This routine checks whether a file/subdirectory is in a specified directory/ */

int InDir(char *Name, char *Dir) {

   struct dirent                        **NameList;
   int                                  N, Match = FALSE;

   N = scandir(Dir, &NameList, 0, alphasort);
   if (N < 0)
      perror("scandir");
   else {
      while(N--) {
         if (strcmp(NameList[N]->d_name, Name) == 0)
	    Match = TRUE;
         free(NameList[N]);
      }
      free(NameList);
   }
   return (Match);
}
