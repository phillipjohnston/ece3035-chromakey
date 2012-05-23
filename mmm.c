/*                     Multi Modal Mean

This library implements a multimodal mean foreground/background
separator. It operates on packed RGB images.

(c) 2008-2011 Scott & Linda Wills

Documentation:

This library provides foreground/background analysis support using a
pixel-based multimodal background model. Below is a usage example.

References:
Apewokin, S., Valentine, B., Wills, L., Wills, S., and Gentile, A.,
"Multimodal Mean Adaptive Backgrounding for Embedded Real-time Video
Surveillance," Proceedings of the Embedded Computer Vision Workshop
(ECVW07), Minneapolis, Minnesota, June 2007. (available online at:
http://users.ece.gatech.edu/~scotty/8893/readings/apewokin07.pdf)

Key Model Parameters:

Maximum Component Difference Threshold (MCDth): this parameter sets
maximum allowable color component difference for a background
match. Smaller values demands closer matches (and additional
modes). Larger values allow more distance matches (and more aliasing).

Cell Threshold (Cth): Minimum cell count for viability. A cell much
reach this value (and maintain it during decimation) or it is removed
and deallocated. This parameter affects short-term
adaptability. Larger values mean slower transference from foreground to
background. Smaller values mean fast background absorption and greater
risk of aperture effect. This parameter is best set based on relative
motion speed of foreground relative to frame rate.

Decimation Rate (DecRate): This rate defines long term background
adaption rate. By logarithmically scaling ratiometric cell values, older
cells are diminished fast than younger cells. A larger value means
faster adaption of new cells, especially affecting cell
predominance. A smaller value means slower adaptation.

Key Data Structures:

Frame Buffer: a frame buffer is an object that holds a single RGB
image, It includes its height and width plus an unsigned char array
large enough to hold RGB values for each pixel.

Cell: A ratiometric chromatic entry including color component sums and
a count. While not directly allocated by the user, cells are allocated
as scene chromatic and luminance variance increase. Heap allocated
and explicitly managed.

BGM: Background model (an array of Cell objects) created by
Create_Initial_BGM function.

Key Usage Functions:

Create_Initial_BGM(): Creates and returns a starting BGM based on an
initial image.

Process_Frame_FG(): Processes a new frame adjusting the BGM for
encountered pixels. Background pixels (i.e., pixels matched in the
BGM) are blacked (i.e., set to (0,0,0). Foreground pixels are
unchanged.

Decimate_BGM(): Moderates BGM adaptively. Divide each cell's color
component sums and count by two. If a cell's count falls below the
cell threshold, it is removed and deallocated.

   FrmBuf               *FB;
   int		        MCDth = MCDTH, Cth = CTH, DecRate = DECRATE;
   Cell                 **BGM;

   ...
   Load_Image(Path, FB);
   ...
   BGM = Create_Initial_BGM(FB);
   for (...) {
      Load_Image(Path, FB);
      Process_Frame_FG(BGM, FB, MCDth, Cth);
      ...
      if (FrameNum % DecRate == 0) {
	 Decimate_BGM(BGM, Cth, Width * Height);
   }
*/

#include <stdlib.h>
#include <stdio.h>
#include "utils.h"
#include "mmm.h"

Cell                    *FreeCells = NULL;

/*            Allocate Cell

This routine returns a new cell. If none are available, it allocates a
block from the heap and adds them to the free list. */

Cell *Allocate_Cell() {

   Cell                 *NewCell;

   if (FreeCells == NULL) {
      FreeCells = (Cell *) malloc(FREECELLSBLOCKSIZE * sizeof(Cell));
      if (FreeCells == NULL) {
         fprintf(stderr, "Unable to allocate cell\n");
         exit (1);
      }
      for (NewCell = FreeCells; NewCell < &(FreeCells[FREECELLSBLOCKSIZE - 1]); NewCell++)
 	 NewCell->Next = (NewCell + 1);
      (FreeCells[FREECELLSBLOCKSIZE - 1]).Next = NULL;
   }
   NewCell = FreeCells;
   FreeCells = FreeCells->Next;
   return (NewCell);
}

/*           Last Cell

This routine returns a pointer to the last cell in the set. */

Cell *Last_Cell(Cell *ThisCell) {

   while (ThisCell->Next != NULL)
      ThisCell = ThisCell->Next;
   return (ThisCell);
}

/*           Length

This routine returns the number of cells in the set. */

int Length(Cell *ThisCell) {
   int                  Length = 0;     

   while (ThisCell != NULL) {
      ThisCell = ThisCell->Next;
      Length += 1;
   }
   return (Length);
}

/*           Print Cell

This routine prints the contents of a cell including both ratiometric
and reduced values. */

void Print_Cell(Cell *ThisCell) {

   if (ThisCell->Count > 0)
      printf("%u: [%8d (%3d), %8d (%3d), %8d (%3d), %6d, %u]\n",
             (int) ThisCell,
	     ThisCell->R, ThisCell->R / ThisCell->Count,
	     ThisCell->G, ThisCell->G / ThisCell->Count,
	     ThisCell->B, ThisCell->B / ThisCell->Count,
	     ThisCell->Count, (int) ThisCell->Next);
   else
      printf("%u: [(%3d, %3d, %3d), %6d, %u]\n",
	     (int) ThisCell,
             ThisCell->R, ThisCell->G, ThisCell->B,
	     -ThisCell->Count, (int) ThisCell->Next);
}

/*           Print Set

This routine prints a set composed of a cell list. */

void Print_Set(int Index, Cell *ThisCell) {

   printf("Set %d:\n", Index);
   while (ThisCell != NULL) {
      printf("   ");
      Print_Cell(ThisCell);
      ThisCell = ThisCell->Next;
   }
}

/*           Print Free Cells

This debugging routine prints the free list. */

void Print_Free_Cells() {

   Cell                 *ThisCell;

   printf("Free Cells\n\n");
   for (ThisCell = FreeCells; ThisCell != NULL; ThisCell = ThisCell->Next)
      Print_Cell(ThisCell);
   printf("\n");
}

/*           Free Cell

This routine adds the current cell to the free list. It returns the
Cell's next pointer. */

Cell *Free_Cell(Cell *ThisCell) {
   Cell                 *Next;

   Next = ThisCell->Next;
   ThisCell->Next = FreeCells;
   FreeCells = ThisCell;
   return(Next);   
}

/*              TrimSort

This routine sorts and prunes a Cell list based on count. The returned
list is sorted in decreasing order. The TrimLength parameter limits
the cell list length, freeing the pruned cells. If the TrimLength is
-1, no pruning occurs (sort-only). */

Cell *TrimSort(Cell *List, int TrimLength) {
  Cell                 *SortList = NULL, *ThisCell, *LastCell, **Trail;
   int                  Depth;

   while (List != NULL) {
      ThisCell = List;
      List = List->Next;
      Depth = TrimLength;
      Trail = &SortList;
      while (*Trail != NULL && Depth != 0 && abs(ThisCell->Count) < abs((*Trail)->Count)) {
	 Trail = &((*Trail)->Next);
	 Depth -= 1;
      }
      if (Depth == 0)
	 ThisCell = Free_Cell(ThisCell);
      else {
	 ThisCell->Next = (*Trail);
         *Trail = ThisCell;
      }
   }
   if (TrimLength > 0) {
      Depth = TrimLength;
      LastCell = SortList;
      while (LastCell != NULL && Depth != 1) {
	 LastCell = LastCell->Next;
         Depth -= 1;
      }
      if (LastCell != NULL) {
	 ThisCell = LastCell->Next;
         while (ThisCell != NULL)
            ThisCell = Free_Cell(ThisCell);
         LastCell->Next = NULL;
      }
   }
   return (SortList);
}

/*           Create Initial BGM

This routine creates the initial background model using a single frame
from the sequence. */

Cell **Create_Initial_BGM(FrmBuf *FB) {

   Cell                 **BGM;
   int                  I;

   if (DEBUG)
      printf("   building %d entry BGM ...\n", FB->Width * FB->Height);
   BGM = (Cell **) malloc(FB->Width * FB->Height * sizeof(Cell *));
   if (BGM == NULL) {
      fprintf(stderr, "Unable to allocate BGM memory\n");
      exit (1);
   }
   for (I = 0; I < FB->Width * FB->Height; I++) {
      BGM[I] = Allocate_Cell();
      BGM[I]->R = (int) FB->Frm[3*I];
      BGM[I]->G = (int) FB->Frm[3*I + 1];
      BGM[I]->B = (int) FB->Frm[3*I + 2];
      BGM[I]->Count = 1;
      BGM[I]->Next = NULL;
   }
   return (BGM);
}   

/*             Ratiometric Match Pixel

This routine compares the input RGB pixel value to each Cell in the
set. If it is within epsilon in all three ratiometric color
components, it matches and the cell is assimilated into the matched
cell. When matched, a pointer the matching cell is
returned. Otherwise, a NULL pointer is returned. */

Cell *Ratio_Match_Pixel(Pixel *P, Cell *Cells, int Epsilon) {

   Cell                *ThisCell;

   for (ThisCell = Cells; ThisCell != NULL; ThisCell = ThisCell->Next)
      if (abs(P->R - ThisCell->R / ThisCell->Count) <= Epsilon &&
          abs(P->G - ThisCell->G / ThisCell->Count) <= Epsilon &&
          abs(P->B - ThisCell->B / ThisCell->Count) <= Epsilon) {
         ThisCell->R += P->R;
         ThisCell->G += P->G;
         ThisCell->B += P->B;
         ThisCell->Count += 1;
         return(ThisCell);
      }
   return(NULL);
}      

/*             Scalar Match Pixel

This routine compares the input RGB pixel value to each Cell in the
set. If it is within epsilon in all three scalar color components, it
matches and match count is incremented. When matched, a pointer the
matching cell is returned. Otherwise, a NULL pointer is returned. */

Cell *Scalar_Match_Pixel(Pixel *P, Cell *Cells, int Epsilon) {

   Cell                *ThisCell;

   for (ThisCell = Cells; ThisCell != NULL; ThisCell = ThisCell->Next)
      if (abs(P->R - ThisCell->R) <= Epsilon &&
          abs(P->G - ThisCell->G) <= Epsilon &&
          abs(P->B - ThisCell->B) <= Epsilon) {
         ThisCell->Count -= 1;
         return(ThisCell);
      }
   return(NULL);
}      

/*             Match Cell

This routine compares a new Cell to each Cell in the set. If it is
within epsilon in all three color components, it matches and the cell
is assimilated into the matched cell. When matched, a pointer the
matching cell is returned. Otherwise, a NULL pointer is returned. */

Cell *Match_Cell(Cell *NewCell, Cell *Cells, int Epsilon) {

   Cell                *ThisCell;

   for (ThisCell = Cells; ThisCell != NULL; ThisCell = ThisCell->Next)
      if (abs(NewCell->R / NewCell->Count - ThisCell->R / ThisCell->Count) <= Epsilon &&
          abs(NewCell->G / NewCell->Count - ThisCell->G / ThisCell->Count) <= Epsilon &&
          abs(NewCell->B / NewCell->Count - ThisCell->B / ThisCell->Count) <= Epsilon) {
         ThisCell->R += NewCell->R;
         ThisCell->G += NewCell->G;
         ThisCell->B += NewCell->B;
         ThisCell->Count += NewCell->Count;
         return(ThisCell);
      }
   return(NULL);
}      

/*                       Add Cell

This routine creates a new cell in given set. First, the last cell in
the set is tested against the minimum cell lifetime. If the last cell
meets the minimum cell life, a new cell object is allocated and
appended to the cell. Otherwise the last cell data is overwritten with
the new pixel data. A pointer to the new cell is then returned. */

Cell *Add_Cell(Pixel *P, Cell *Set, int Cth) {

   Cell                 *LastCell, *NewCell;

   LastCell = Last_Cell(Set);
   if (LastCell->Count >= Cth) {           /* if previous last cell is old enough */
      NewCell = Allocate_Cell();           /* then make and append a new cell */
      NewCell->R = (int) P->R;
      NewCell->G = (int) P->G;
      NewCell->B = (int) P->B;
      NewCell->Count = 1;
      NewCell->Next = NULL;
      LastCell->Next = NewCell;
      return (NewCell);
   } else {                                /* else replace previous last cell */
      LastCell->R = (int) P->R;
      LastCell->G = (int) P->G;
      LastCell->B = (int) P->B;
      LastCell->Count = 1;
      return (LastCell);
   }
}

/*             Predominant Cell

This routine finds the cell with the largest count in a set. It also
returns the sum all all cell counts within the set.  This can be used
to compute the predominance rate (i.e., the percentage of sum of
counts of each cell represented by the largest cell count). */

Cell *Predominant_Cell(Cell *Cells, int *TotalCount) {

   Cell                *MaxCell = Cells;
   int                 MaxCount = Cells->Count;

   *TotalCount = 0;
   while (Cells != NULL) {
      *TotalCount += Cells->Count;
      if (Cells->Count > MaxCount) {
         MaxCell = Cells;
         MaxCount = Cells->Count;
      }
      Cells = Cells->Next;
   }
   return(MaxCell);
}

/*             Compute Set Demographics

This routine computes a set demographic study including number of
cells and predominant rate. It also includes a histogram of these
parameters. */

void Compute_Set_Demographics(FILE *Log, int N, Cell **BGM, int NumSets) {

   Cell                *ThisCell;
   int                 I, L, P, Cmax, Ctot;
   int                 Ltot = 0, Ptot = 0;
   int                 Lhisto[10], Phisto[11];

   for (I = 0; I < 10; I++)
      Lhisto[I] = Phisto[I] = 0;
   Phisto[10] = 0;
   for (I = 0; I < NumSets; I++) {
      Cmax = BGM[I]->Count;
      L = Ctot = 0;
      for (ThisCell = BGM[I]; ThisCell != NULL; ThisCell = ThisCell->Next) {
	 L += 1;
         Ctot += ThisCell->Count;
         if (ThisCell->Count > Cmax)
	    Cmax = ThisCell->Count;
      }
      Ltot += L;
      if (L < 10)
	 Lhisto[L-1] += 1;
      else
	 Lhisto[9] += 1;
      Ptot += P = Cmax * 100 / Ctot;
      P /= 5;                              /* group in 50% to 100% with 5% steps */
      if (P > 10)
	 Phisto[P-10] += 1;
      else
	 Phisto[0] += 1;
   }
   fprintf(Log, "length avg= %d.%d\n", Ltot/NumSets, (10*Ltot/NumSets)%10); /* get first decimal of average */
   for (I = 0; I < 9; I++)
      fprintf(Log, " %d: %d (%d%%),", I+1, Lhisto[I], Lhisto[I]*100/NumSets);
   fprintf(Log, " 10+: %d (%d%%)\n", Lhisto[9], Lhisto[9]*100/NumSets);
   fprintf(Log, "predominance avg= %d\n", Ptot/NumSets);
   for (I = 10; I > 0; I--)
      fprintf(Log, " %d%%: %d (%d%%),", 5*I+50,Phisto[I], 100*Phisto[I]/NumSets);
   fprintf(Log, " <50%%: %d (%d%%)\n\n", Phisto[0], 100*Phisto[0]/NumSets);

}

/*             Color Lock

This routine transforms the color component representation of cells in
a cell list from ratiometric to scalar. If the Clear flag is set, the
count is also zeroed. */

void Color_Lock(Cell *Cells, int Clear) {

   while (Cells != NULL) {
      Cells->R /= Cells->Count;
      Cells->G /= Cells->Count;
      Cells->B /= Cells->Count;
      if (Clear)
 	 Cells->Count = 0;
      else
	 Cells->Count = -Cells->Count;
      Cells = Cells->Next;
   }
}

/*              Decimate BGM

This routine decimates (divided by two) every cell value and count in
the background model. If the cell falls below the cell threshold, it
is removed from the set's cell list and added to the free cell
list. The number of removed cells is returned.

It would be a good idea to preserve young, growing cells if birthdates
where available. */

int Decimate_BGM(Cell **BGM, int Cth, int NumSets) {
   int                  I, Freed = 0;
   Cell                 *ThisCell, **TrailingNext;

   for (I = 0; I < NumSets; I++) {
      ThisCell = BGM[I];
      TrailingNext = &(BGM[I]);
      while (ThisCell != NULL) {
	 if (ThisCell->Count >= Cth) {
	    ThisCell->R >>= 1;
	    ThisCell->G >>= 1;
	    ThisCell->B >>= 1;
            ThisCell->Count >>= 1;
         }
         if (ThisCell->Count < Cth && (ThisCell->Next != NULL || ThisCell != BGM[I])) {
	    *TrailingNext = ThisCell->Next;                      /* splice out invalid cell */
            ThisCell = Free_Cell(ThisCell);
            Freed += 1;
	 } else {
	    TrailingNext = &(ThisCell->Next);                    /* else move to next cell */
            ThisCell = ThisCell->Next;
         }
      }
   }
   return (Freed);
}

/*              Process Frame Foreground

This routine processes an image frame, blacking out background
pixels. Foreground pixels are not modified. */

void Process_Frame_FG(Cell **BGM, FrmBuf *FB, int Epsilon, int Cth) {

   Cell                 *Result;
   Pixel                *P;
   int                  I;


   for (I = 0; I < FB->Width * FB->Height; I++) {
      P = (Pixel *) &(FB->Frm[I * 3]);
      Result = Ratio_Match_Pixel(P, BGM[I], Epsilon);
      if (Result == NULL)
	 Add_Cell(P, BGM[I], Cth);
      else if (Result->Count >= Cth) {
	 P->R = 0;
	 P->G = 0;
	 P->B = 0;
      }
   }
}

/*              Process Frame Background

This routine processes an image frame. The returned frame contain the
predominant cell (highest occurrence) background value. */

void Process_Frame_BG(Cell **BGM, FrmBuf *FB, int Epsilon, int Cth) {

   Cell                 *Result;
   Pixel                *P;
   int                  I, TotalCount;

   for (I = 0; I < FB->Width * FB->Height; I++) {
      P = (Pixel *) &(FB->Frm[I * 3]);
      Result = Ratio_Match_Pixel(P, BGM[I], Epsilon);
      if (Result == NULL)
	 Add_Cell(P, BGM[I], Cth);
      Result = Predominant_Cell(BGM[I], &TotalCount);
      P->R = Result->R / Result->Count;
      P->G = Result->G / Result->Count;
      P->B = Result->B / Result->Count;
   }
}

/*              Process Frame Predominance Map

This routine processes an image frame. It replaces the frame image
with a predominance map indicating the the percentage of total set
count represented by the cell with the top count. */

void Process_Frame_PD_Map(Cell **BGM, FrmBuf *FB, int Epsilon, int Cth) {

   Cell                 *Result;
   Pixel                *P;
   int                  I, TotalCount, PDrate;


   for (I = 0; I < FB->Width * FB->Height; I++) {
      P = (Pixel *) &(FB->Frm[I * 3]);
      Result = Ratio_Match_Pixel(P, BGM[I], Epsilon);
      if (Result == NULL)
	 Add_Cell(P, BGM[I], Cth);
      Result = Predominant_Cell(BGM[I], &TotalCount);
      PDrate = (TotalCount - Result->Count) * 100 / TotalCount;
      Rainbow(PDrate * 255 / 100, P);
   }
}

/*              Create Background Frame

This routine creates a background frame using the value of the
predominate cell for each pixel position. */

void Create_BG_Frame(Cell **BGM, FrmBuf *FB) {

   Cell                 *Result;
   Pixel                *P;
   int                  I, TotalCount;

   for (I = 0; I < FB->Width * FB->Height; I++) {
      P = (Pixel *) &(FB->Frm[I * 3]);
      Result = Predominant_Cell(BGM[I], &TotalCount);
      P->R = Result->R / Result->Count;
      P->G = Result->G / Result->Count;
      P->B = Result->B / Result->Count;
   }
}

/*              Create Predominance Map

This routine creates a predominance map indicating the the percentage
of total set count represented by the cell with the top count. */

void Create_PD_Map(Cell **BGM, FrmBuf *FB) {

   Cell                 *Result;
   Pixel                *P;
   int                  I, TotalCount, PDrate;


   for (I = 0; I < FB->Width * FB->Height; I++) {
      P = (Pixel *) &(FB->Frm[I * 3]);
      Result = Predominant_Cell(BGM[I], &TotalCount);
      PDrate = (TotalCount - Result->Count) * 100 / TotalCount;
      Rainbow(PDrate * 255 / 100, P);
   }
}
