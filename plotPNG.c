/*===========================================================================*/
/* An ANSI standard C program that creates plots of mathematical functions   */
/* in files in the Portable Network Graphics (PNG) format.                   */
/*===========================================================================*/

/*===========================================================================*/
/* Includes                                                                  */
/*===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <png.h>
#include <math.h>
#include "tinyexpr.h"

/*===========================================================================*/
/* Structure definition                                                      */
/*===========================================================================*/
struct png_struct
   {
      short int   imgWidth;
      short int   imgHeight;
      png_byte    colourType;
      png_byte    bitDepth;
   };
typedef struct png_struct PNG;

/*===========================================================================*/
/* Function prototypes                                                       */
/*===========================================================================*/
void makeImageData       (PNG *,short int,png_bytep **, char[]);
void allocateImageMemory (PNG *,png_bytep **,png_structp *,png_infop *);
void freeImageMemory     (PNG *,png_bytep **);
void writePngFileHeader  (FILE **,char *,PNG *,png_structp *,png_infop *);
void writePngFileData    (png_structp *,png_infop *,png_bytep **);
void writePngFileTrailer (FILE **,png_structp *);
void abortProgram        (const char *, ...);

/*===========================================================================*/
/* main function                                                             */
/*===========================================================================*/
int main ( int      argc,
           char   **argv )
{
   FILE         *fp;
   PNG           pngData;
   png_structp   pngPtr;
   png_infop     infoPtr;
   png_bytep    *rowPointers;

   pngData.imgWidth   = 300;   /* pixels */
   pngData.imgHeight  = 300;   /* pixels */
   pngData.colourType = PNG_COLOR_TYPE_RGB;
   pngData.bitDepth   = 8;

   /* error trapping */
   if ( argc != 3 ){
     fprintf(stdout, "Program aborted. See stderr for more information.\n\n");
     abortProgram("Error: Incorrect number of arguments given.\nUsage:"
                  " <program_name> <file_out> <math_expr>\n");
   }

   if ( strstr(argv[1], ".png") == NULL ){
     fprintf(stdout, "Program aborted. See stderr for more information.\n\n");
     abortProgram("Error: Invalid file name given in second argument.\nValid"
                  " file names require the \".png\" extension.\ne.g."
                  " \"file.png\" rather than \"file\"\n");
   }

   if ( strchr(argv[2], '=') != NULL ){
     fprintf(stdout, "Program aborted. See stderr for more information.\n\n");
     abortProgram("Error: Invalid expression given in third argument.\n"
                  "Expressions of the form y=f(x) or z=f(x,y) should be written"
                  " f(x) or f(x,y) respectively.\ne.g. to plot y=x^2, provide"
                  " \"x^2\" as third argument.\n");
   }

   if ( strchr(argv[2], 'y') != NULL && strchr(argv[2], 'x') == NULL ){
     fprintf(stderr, "Warning: No x variable provided in third argument"
                     " (expression). Will assume expression is of the form"
                     " f(x,y).\nUnivariable expression should be given in terms"
                     " of x. e.g. \"y^2\" should be written \"x^2\", else it"
                     " will be treated as \"0*x + y^2\".\n\n");
   }

   if ( pngData.imgHeight > 1400 || pngData.imgWidth > 1400 ){
     fprintf(stderr, "Warning: Potential unexpected behaviour at dimensions"
                     " greater than 1400.\nIf segmentation fault, try lowering"
                     " resolution (hard-coded).\n\n");
   }

   writePngFileHeader(&fp,argv[1],&pngData,&pngPtr,&infoPtr);
   allocateImageMemory(&pngData,&rowPointers,&pngPtr,&infoPtr);
   makeImageData(&pngData,png_get_channels(pngPtr,infoPtr),&rowPointers, argv[2]);
   writePngFileData(&pngPtr,&infoPtr,&rowPointers);
   writePngFileTrailer(&fp,&pngPtr);
   freeImageMemory(&pngData,&rowPointers);

   fprintf(stdout, "File %s successfully created.\n", argv[1]);
   return 0;
}

/*===========================================================================*/
/* Function: makeImageData                                                   */
/* Puts data into the pixels to construct the image, based on the expression */
/* given in the fourth parameter: "char expression[30]".                     */
/*===========================================================================*/
void makeImageData ( PNG          *pngData,
                     short int     valuesPerPixel,
                     png_bytep   **rowPointers,
                     char          expression[30] )
{
   /* for evaluation */
   double      x;
   double      y;

   /* for iteration */
   short int   i;
   short int   j;

   /* for calculations */
   double      result;
   float       z_values[pngData->imgWidth][pngData->imgHeight];
   float       x_incr = 0;
   float       y_incr = 0;
   float       max = 0;
   float       min = 0;
   float       p;
   char       *fxy_check = strchr(expression, 'y');

   /* for plotting */
   long int    x_pixel;
   long int    y_pixel;
   png_byte   *ptr;

   /* for tinyexpr */
   int         err;
   te_expr    *n;
   te_variable vars[] = {{"x", &x}, {"y", &y}};

   n = te_compile(expression, vars, 2, &err);


   /* error trapping */
   if ( !n ){
     fprintf(stdout, "Program aborted. See stderr for more information.\n");
     abortProgram("Fatal error: Failed to compile math expression.\n\nProbably"
                  " invalid expression given in third argument:\n\n"
                  "     Expressions should be written in terms of x and y only."
                  "\n     x should be used for univariable expressions, or both"
                  " x and y for multivariate expressions.\n     e.g. \"k^2\" is"
                  " invalid, and should be written \"x^2\".\n\n     Equations"
                  " of the form y=f(x) or z=f(x,y) are invalid, and should be"
                  " written f(x) or f(x,y) respectively.\n     e.g. \"y=x^2\""
                  " is invalid, and should be written \"x^2\".\n");
   }

   if ( fxy_check != NULL && pngData->imgHeight != pngData->imgWidth ){
     abortProgram("Error: Invalid dimensions (hard-coded).\n\nExpressions of"
                  " the form f(x,y) can only be written to PNG files with"
                  " square dimension.\ne.g. 200x300 is invalid, but"
                  " 300x300 or 200x200 are valid.\n");
   }

   /* plotting the expression */
   if ( fxy_check == NULL ){                    /* if its of the form f(x) */
     /* colouring background white */
     for ( i=0; i<pngData->imgHeight; i++ )
     {
        png_byte *row = (*rowPointers)[i];
        for ( j=0; j<pngData->imgWidth; j++ )
        {
          ptr = &(row[j*valuesPerPixel]);
          ptr[0] = 255; ptr[1] = 255; ptr[2] = 255;
        }
     }

     /* calculating and plotting y values (colouring over white background)*/
     for (i=0; i<(pngData->imgWidth)*50; i++){
       x = x_incr;
       result = te_eval(n);

       /* if coordinate is in range, plot it */
       if ( result < 1 ){
         x_pixel = x_incr*(pngData->imgWidth);
         y_pixel = result*(pngData->imgHeight);

         png_byte *ptr;
         png_byte *current_row;

         current_row = (*rowPointers)[((pngData->imgHeight)-1)-y_pixel];
         ptr = &(current_row[x_pixel*valuesPerPixel]);
         ptr[0] = 0; ptr[1] = 0; ptr[2] = 255;
       }
       x_incr += 1.0/((pngData->imgWidth)*50);
     }
     te_free(n);
   }

  else {                                    /* else its of the form f(x,y) */
    /* calculating z values */
    for (i=0; i<pngData->imgWidth; i++){
      y_incr = 0.0;
      for (j=0; j<pngData->imgHeight; j++){
        x = x_incr;
        y = y_incr;
        result = te_eval(n);

        /* initialising max and min values */
        if ( i==0 && j==0 ){
          max = result;
          min = result;
        }

        /* updating max and min */
        else if ( result > max ){
          max = result;
        } else if ( result < min ){
          min = result;
        }

        /* storing the result */
        z_values[i][j] = result;

        y_incr += 1.0 / pngData->imgWidth;
      }
      x_incr += 1.0 / pngData->imgHeight;
    }
    te_free(n);

    /* plotting colours */
    for (i=0; i<pngData->imgWidth; i++){
      for (j=0; j<pngData->imgHeight; j++){
        png_byte *ptr;
        png_byte *current_row;

        p = (z_values[i][j] - min)/(max - min);

        current_row = (*rowPointers)[((pngData->imgWidth)-1)-i];
        ptr = &(current_row[j*valuesPerPixel]);
        ptr[0] = 255 * (1 - p); ptr[1] = 0; ptr[2] = 255 * p;

      }
    }
  }
}


/*===========================================================================*/
/* Function: allocateImageMemory                                             */
/* Allocate memory into the image storage; the image is stored as rows in    */
/* rowPointers, and each element in rowPointers is a pointer to a single     */
/* pixel.  Each pixel is an RGB triple in the range 0 to 255.                */
/*===========================================================================*/
void allocateImageMemory ( PNG            *pngData,
                           png_bytep     **rowPointers,
                           png_structp    *pngPtr,
                           png_infop      *infoPtr )
{
   short int   y;
   *rowPointers = (png_bytep *)malloc(sizeof(png_bytep)*pngData->imgHeight);
   for (y=0; y<pngData->imgHeight; y++)
      (*rowPointers)[y] = (png_byte *)malloc(png_get_rowbytes(*pngPtr,*infoPtr));
}

/*===========================================================================*/
/* Function: freeImageMemory                                                 */
/* Free the memory used to store the image.                                  */
/*===========================================================================*/
void freeImageMemory ( PNG          *pngData,
                       png_bytep   **rowPointers )
{
   short int   y;
   for (y=0; y<pngData->imgHeight; y++)
      free((*rowPointers)[y]);
   free(*rowPointers);
}

/*===========================================================================*/
/* Function: writePngFileHeader                                              */
/* Open the output file and intialise it as a PNG image file.`               */
/*===========================================================================*/
void writePngFileHeader ( FILE         **fp,
                          char          *fileName,
                          PNG           *pngData,
                          png_structp   *pngPtr,
                          png_infop     *infoPtr )
{
   /* create file */
   *fp = fopen(fileName, "wb");
   if ( !*fp )
      abortProgram("[write_png_file] File %s could not be opened for writing", fileName);

   /* initialize stuff */
   *pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

   if ( !*pngPtr )
      abortProgram("[write_png_file] png_create_write_struct failed");

   *infoPtr = png_create_info_struct(*pngPtr);
   if ( !*infoPtr )
      abortProgram("[write_png_file] png_create_info_struct failed");

   if ( setjmp(png_jmpbuf(*pngPtr)) )
      abortProgram("[write_png_file] Error during init_io");

   png_init_io(*pngPtr, *fp);


   /* write header */
   if ( setjmp(png_jmpbuf(*pngPtr)) )
      abortProgram("[write_png_file] Error during writing header");

   /*png_set_compression_level(pngPtr,0);*/
   png_set_IHDR(*pngPtr, *infoPtr, pngData->imgWidth, pngData->imgHeight,
                pngData->bitDepth, pngData->colourType, PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
}

/*===========================================================================*/
/* Function: writePngFileData                                                */
/* Output the image data to the output file.                                 */
/*===========================================================================*/
void writePngFileData ( png_structp    *pngPtr,
                        png_infop      *infoPtr,
                        png_bytep     **rowPointers )
{
   png_write_info(*pngPtr,*infoPtr);

   if ( setjmp(png_jmpbuf(*pngPtr)) )
      abortProgram("[write_png_file] Error during writing bytes");

   png_write_image(*pngPtr,*rowPointers);
}

/*===========================================================================*/
/* Function: writePngFileTrailer                                             */
/* End the PNG file and close it.                                            */
/*===========================================================================*/
void writePngFileTrailer ( FILE          **fp,
                           png_structp    *pngPtr )
{
   if ( setjmp(png_jmpbuf(*pngPtr)) )
      abortProgram("[write_png_file] Error during end of write");

   png_write_end(*pngPtr,NULL);

   fclose(*fp);
}

/*===========================================================================*/
/* Function: abortProgram                                                    */
/* Output an error message and abort the program.                            */
/*===========================================================================*/
void abortProgram (const char *s, ...)
{
   va_list args;
   va_start(args, s);
   vfprintf(stderr, s, args);
   fprintf(stderr, "\n");
   va_end(args);
   abort();
}
