/*                     Multi Modal Mean

This library implements a multimodal mean foreground/background
separator. It operates on packed RGB images.

(c) 2008-2011 Scott & Linda Wills                         */

typedef struct          Cell {
   int                  R, G, B, Count;
   struct Cell          *Next;
}  Cell;

#define                 FREECELLSBLOCKSIZE 100

extern Cell             *FreeCells;

extern Cell **Create_Initial_BGM(FrmBuf *FB);
extern void Process_Frame_FG(Cell **BGM, FrmBuf *FB, int Epsilon, int Cth);
extern void Process_Frame_BG(Cell **BGM, FrmBuf *FB, int Epsilon, int Cth);
extern void Process_Frame_PD_Map(Cell **BGM, FrmBuf *FB, int Epsilon, int Cth);
extern void Create_BG_Frame(Cell **BGM, FrmBuf *FB);
extern void Create_PD_Map(Cell **BGM, FrmBuf *FB);
extern int Decimate_BGM(Cell **BGM, int Cth, int NumSets);
extern Pixel Rainbow_Lookup(int Index);
extern Cell *Allocate_Cell();
extern Cell *Last_Cell(Cell *ThisCell);
extern int  Length(Cell *ThisCell);
extern Cell *Free_Cell(Cell *ThisCell);
extern Cell *TrimSort(Cell *List, int Length);
extern void Print_Cell(Cell *ThisCell);
extern void Print_Set(int Index, Cell *ThisCell);
extern Cell *Ratio_Match_Pixel(Pixel *P, Cell *Cells, int Epsilon);
extern Cell *Scalar_Match_Pixel(Pixel *P, Cell *Cells, int Epsilon);
extern Cell *Match_Cell(Cell *NewCell, Cell *Cells, int Epsilon);
extern void Compute_Set_Demographics(FILE *Log, int N, Cell **BGM, int NumSets);
extern void Color_Lock(Cell *Cells, int Clear);
