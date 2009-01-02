/****************************************************************************
 * aspect.cpp
 * Author: Matthew Perry
 * License : 
 Copyright 2005 Matthew T. Perry
 Licensed under the Apache License, Version 2.0 (the "License"); 
 you may not use this file except in compliance with the License. 
 You may obtain a copy of the License at 
 
 http://www.apache.org/licenses/LICENSE-2.0 
 
 Unless required by applicable law or agreed to in writing, software 
 distributed under the License is distributed on an "AS IS" BASIS, 
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 See the License for the specific language governing permissions and 
 limitations under the License.

 * calculates aspect for a gdal-supported raster DEM
 *
 * References:
 * Burrough, P.A. and McDonell, R.A., 1998. Principles of Geographical Information Systems. 
 ****************************************************************************/

#include <iostream>
#include <stdlib.h>
#include <math.h>
#include "gdal_priv.h"

int main(int nArgc, char ** papszArgv) 
{ 
    GDALDataset *poDataset;     
    const float degrees_to_radians = 3.14159 / 180.0;
    const float radians_to_degrees = 180.0 / 3.14159;           
    double      adfGeoTransform[6];
    float       *win;
    float       *aspectBuf;
    float       *slopeBuf;
    float       dx;
    float       dy;
    float       key;
    float       slopePct;
    float       aspect;
    int         i;
    int         j;
    int         n;
    int         containsNull;

    /* -----------------------------------
     * Defaults
     */
    const char *pszFormat = "GTiff";

    /* -----------------------------------
     * Parse Input Arguments
     */
    if (nArgc < 2)
    {
        printf( " \n Generates an aspect map from any GDAL-supported elevation raster\n"
                " Outputs a 32-bit tiff with pixel values from 0-360 indicating azimuth\n"
                " Usage: \n"
                "   aspect input_dem output_aspect_map \n");
        exit(1);
    }

    const char  *pszFilename = papszArgv[1];
    const char  *pszAspectFilename = papszArgv[2];

    for ( int iArg = 3; iArg < nArgc; iArg++ )
    {
        // TO DO : Add Output Format, min slope for aspect
    }


    GDALAllRegister(); 

    /*---------------------------------------
     * Open Dataset and get raster band (assuming it is band #1)
     */
    poDataset = (GDALDataset *) GDALOpen( pszFilename, GA_ReadOnly );
    if( poDataset == NULL )
    {
        printf( "Couldn't open dataset %s\n", 
                pszFilename );
    }
    GDALRasterBand  *poBand;       
    poBand = poDataset->GetRasterBand( 1 );
    poDataset->GetGeoTransform( adfGeoTransform );

    // Variables related to input dataset
    const double cellsizeY = adfGeoTransform[5];
    const double cellsizeX = adfGeoTransform[1];
    const float nullValue = (float) poBand->GetNoDataValue( );
    const float aspectNullValue = -9999.;
    const int   nXSize = poBand->GetXSize();
    const int   nYSize = poBand->GetYSize();
    aspectBuf    = (float *) CPLMalloc(sizeof(float)*nXSize); 
    win         = (float *) CPLMalloc(sizeof(float)*9);

    /* -----------------------------------------
     * Open up the output datasets and copy over relevant metadata
     */
    GDALDriver *poDriver;
    poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
    GDALDataset      *poAspectDS;    
    GDALRasterBand   *poAspectBand; 
    char **papszOptions = NULL;

    /*
     * Open slope output map
     */
    poAspectDS = poDriver->Create(pszAspectFilename,nXSize,nYSize,1,GDT_Float32, 
                                papszOptions );
    poAspectDS->SetGeoTransform( adfGeoTransform );    
    poAspectDS->SetProjection( poDataset->GetProjectionRef() );
    poAspectBand = poAspectDS->GetRasterBand(1);
    poAspectBand->SetNoDataValue(aspectNullValue);   


    /* ------------------------------------------
     * Get 3x3 window around each cell 
     * (where the cell in question is #4)
     *
     *                 0 1 2
     *                 3 4 5
     *                 6 7 8
     *  and calculate slope and aspect
     */
    for ( i = 0; i < nYSize; i++) 
    {
        for ( j = 0; j < nXSize; j++) 
        {
            containsNull = 0;

            // Exclude the edges 
            if (i == 0 || j == 0 || i == nYSize-1 || j == nXSize-1 ) 
            {
                // We are at the edge so write nullValues and move on
                aspectBuf[j] = nullValue;
                continue;
            }

            // Read in 3x3 window
            poBand->RasterIO( GF_Read, j-1, i-1, 3, 3,
                          win, 3, 3, GDT_Float32, 
                          0, 0 ); 

            // Check if window has null value
            for ( n = 0; n <= 8; n++) 
            {
               if(win[n] == nullValue) 
               {
                   containsNull = 1;
                   break;
               }
            }

            if (containsNull == 1) 
            {
                // We have nulls so write nullValues and move on
                 aspectBuf[j] = nullValue;
                continue;
            } 
            else 
            {
                // We have a valid 3x3 window to compute aspect

                dx = ((win[2] + win[5] + win[5] + win[8]) -
                      (win[0] + win[3] + win[3] + win[6]));

                dy = ((win[6] + win[7] + win[7] + win[8]) - 
                      (win[0] + win[1] + win[1] + win[2]));

                aspect = atan2(dy/8.0,-1.0*dx/8.0) / degrees_to_radians;

                if (dx == 0)
                {
                    if (dy > 0) 
                        aspect = 0.0;
                    else if (dy < 0)
                        aspect = 180.0;
                    else
                        aspect = aspectNullValue;
                } 
                else 
                {
                    if (aspect > 90.0) 
                        aspect = 450.0 - aspect;
                    else
                        aspect = 90.0 - aspect;
                }

                if (aspect == 360.0) 
                    aspect = 0.0;
           
                aspectBuf[j] = aspect;

            }
        }

        /* -----------------------------------------
         * Write Line to File
         */

         poAspectBand->RasterIO( GF_Write, 0, i, nXSize, 1, 
                      aspectBuf, nXSize, 1, GDT_Float32, 0, 0 ); 
    }

    delete poAspectDS;

    return 0;
}
