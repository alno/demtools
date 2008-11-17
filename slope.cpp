/****************************************************************************
 * slope.cpp
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

 * calculates slope for a gdal-supported raster DEM
 * TBD:
 * - Command line args
 * - handle latlong (horz distance in degrees so slope is wrong)
 * - handle missing transform, nullval, metadata
 * - write all metadata to output rasters
 * - progress meter
 *
 * References:
 * Burrough, P.A. and McDonell, R.A., 1998. Principles of Geographical Information Systems. p. 190.
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
    // 0 = 'percent' or 1 = 'degrees'
    int slopeFormat = 1; 
    // vertical units per horizontal unit (for slope calc)
    float scale = 1.0; 
    char *pszFormat = "GTiff";

    /* -----------------------------------
     * Parse Input Arguments
     */
    if (nArgc < 2)
    {
        printf( " \n Generates a slope map from any GDAL-supported elevation raster\n"
                " Usage: \n"
                "   slope input_dem output_slope_map \n"
                "                 [-p use percent slope (default=degrees)] [-s scale* (default=1)]\n\n"
                " Notes : \n"
                "   Scale is the ratio of vertical units to horizontal\n"
                "     for Feet:Latlong try scale=370400, for Meters:LatLong try scale=111120 \n\n");
        exit(1);
    }

    const char  *pszFilename = papszArgv[1];
    const char  *pszSlopeFilename = papszArgv[2];

    for ( int iArg = 3; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-p") )
            slopeFormat = 0;
        if( EQUAL(papszArgv[iArg],"-s") ||
            EQUAL(papszArgv[iArg],"-scale"))
            scale = atof(papszArgv[iArg+1]);
        // TO DO : Add Output Format
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
    const int   nXSize = poBand->GetXSize();
    const int   nYSize = poBand->GetYSize();
    slopeBuf    = (float *) CPLMalloc(sizeof(float)*nXSize); 
    win         = (float *) CPLMalloc(sizeof(float)*9);

    /* -----------------------------------------
     * Open up the output datasets and copy over relevant metadata
     */
    GDALDriver *poDriver;
    poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
    GDALDataset      *poSlopeDS;    
    GDALRasterBand   *poSlopeBand; 
    char **papszOptions = NULL;

    /*
     * Open slope output map
     */
    poSlopeDS = poDriver->Create(pszSlopeFilename,nXSize,nYSize,1,GDT_Float32, 
                                papszOptions );
    poSlopeDS->SetGeoTransform( adfGeoTransform );    
    poSlopeDS->SetProjection( poDataset->GetProjectionRef() );
    poSlopeBand = poSlopeDS->GetRasterBand(1);
    poSlopeBand->SetNoDataValue(-9999);   


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
                slopeBuf[j] = nullValue;
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
                 slopeBuf[j] = nullValue;
                continue;
            } 
            else 
            {
                // We have a valid 3x3 window to compute slope
                dx = ((win[0] + win[3] + win[3] + win[6]) - 
                      (win[2] + win[5] + win[5] + win[8]));

                dy = ((win[6] + win[7] + win[7] + win[8]) - 
                      (win[0] + win[1] + win[1] + win[2]));

                key = ((dx/(8*cellsizeX*scale)) * (dx/(8*cellsizeX*scale))) + 
                      ((dy/(8*cellsizeY*scale)) * (dy/(8*cellsizeY*scale)));

                slopePct = 100*sqrt(key);
                if (slopeFormat == 1) 
                    slopeBuf[j] = atan(sqrt(key)) * radians_to_degrees;
                else
                    slopeBuf[j] = slopePct;

            }
        }

        /* -----------------------------------------
         * Write Line to File
         */

         poSlopeBand->RasterIO( GF_Write, 0, i, nXSize, 1, 
                      slopeBuf, nXSize, 1, GDT_Float32, 0, 0 ); 
    }

    delete poSlopeDS;

    return 0;
}
