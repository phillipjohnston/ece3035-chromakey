/*
  Sampe code uses JPEG library.
  This program reads a jpeg image, processes it and output to a jpeg image.
  It is tested on Linux 2.4.
  To compile: gcc -o jpeg2jpeg jpeg2jpeg.c -ljpeg
  
  JPEG library can be obtained from http://www.ijg.org
*/

#include <stdio.h>
#include "jpeglib.h"

unsigned char **pic_calloc(unsigned char **, int, int);
void pic_free(unsigned char **, int);
unsigned char **process(unsigned char**, unsigned char **, int, int);


int main()
{
  FILE *jpegin, *jpegout;
  char file_name[50];
  unsigned char **pic, *scan_line, **out_pic;
  int i, j, in_width, in_height, out_width, out_height, row_stride, quality;
  struct jpeg_decompress_struct cinfo;
  struct jpeg_compress_struct cinfo1;
  struct jpeg_error_mgr jerr;
  JSAMPROW row_pointer[1];

  sprintf(file_name, "%s.jpg", "testimg");
  if((jpegin = fopen(file_name, "rb")) == NULL){
    fprintf(stderr, "cannot open %s\n", file_name);
    return 1;
  }

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, jpegin);
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);
  row_stride = cinfo.output_width * cinfo.output_components;

  in_width = cinfo.output_width;
  in_height = cinfo.output_height;

  pic = pic_calloc(pic, in_height, in_width*3);
 
  /* 
     you might have to change the out_width and out_height 
     if the output image size is different from the original.
  */
  out_width = in_width;
  out_height = in_height;
  out_pic = pic_calloc(out_pic, out_height, out_width*3);

  while (cinfo.output_scanline < in_height) {
    jpeg_read_scanlines(&cinfo, &pic[cinfo.output_scanline], 1);
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(jpegin);

  /* Simple Processing (negate the picture) */
  out_pic = process(pic,out_pic, out_height, out_width);

  sprintf(file_name, "%s.jpg", "output");
  if((jpegout = fopen(file_name, "wb")) == NULL){
    fprintf(stderr, "cannot open %s\n", file_name);
    return 1;
  }

  cinfo1.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo1);
  jpeg_stdio_dest(&cinfo1, jpegout);

  cinfo1.image_width = out_width;
  cinfo1.image_height = out_height;
  cinfo1.input_components = 3;
  cinfo1.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo1);

  quality = 70; 
  jpeg_set_quality(&cinfo1, quality, TRUE);
  jpeg_start_compress(&cinfo1, TRUE);
  row_stride = out_width * 3;

  while(cinfo1.next_scanline < cinfo1.image_height){
    row_pointer[0] = &out_pic[cinfo1.next_scanline][0];
    (void) jpeg_write_scanlines(&cinfo1, row_pointer, 1);
  }
  jpeg_finish_compress(&cinfo1);
  fclose(jpegout);
  jpeg_destroy_compress(&cinfo1);

  pic_free(pic, cinfo.output_height);
  pic_free(out_pic, cinfo.output_height);

  return 0;
}

unsigned char **pic_calloc(unsigned char **p, int row, int col) 
{
  int i;
  
  p = (unsigned char **) calloc(row,sizeof(unsigned char *));
  if(!p){
    fprintf(stderr,"memory allocation error\n");
    exit(1);
  }
  for(i=0;i<row;i++){
    p[i] = (unsigned char *) calloc(col,sizeof(unsigned char));
    if(!p[i]){
      fprintf(stderr,"memory allocation error\n");
      exit(1);
    }
  }
  return p;
}

void pic_free(unsigned char **p, int row)
{
  int i;

  for (i=0; i<row; i++){
    free(p[i]);
  }

  free(p);
}

unsigned char **process(unsigned char **pic, unsigned char **out_pic, int height, int width)
{
  int i, j;
  
  for(i=0; i<height; i++){
    for(j=0; j<width; j++){
      out_pic[i][3*j] = -pic[i][3*j];
      out_pic[i][3*j+1] = -pic[i][3*j +1];
      out_pic[i][3*j+2] = -pic[i][3*j +2];
    }
  }
 
  return out_pic;
}

