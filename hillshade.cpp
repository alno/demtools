/****************************************************************************
 * hillshade.cpp
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

 * calculates a shaded relief image from a gdal-supported raster DEM
 
 CHANGELOG
 * updated nodata handling so that 0 is nodata and 1 - 255 are the shade values
 ****************************************************************************/

#include <iostream>
#include <stdlib.h>
#include <math.h>
#include "gdal_priv.h"

int main(int nArgc, char ** papszArgv)
{
    GDALDataset *poDataset;
    const float radiansToDegrees = 180.0 / 3.14159;
    const float degreesToRadians = 3.14159 / 180.0;
    double      adfGeoTransform[6];
    float       *win;
    float       *shadeBuf;
    float  	x;
    float	y;
    float       aspect;
    float	slope;
    float       cang;
    int         i;
    int         j;
    int         n;
    int         containsNull;
    const char *pszFormat = "GTiff";
    float       z = 1.0;
    float       scale = 1.0;
    float       az = 315.0;
    float       alt = 45.0;

    /* -----------------------------------
     * Parse Input Arguments
     */
    if (nArgc < 2)
    {
        printf( " \n Generates a shaded relief map from any GDAL-supported elevation raster\n"
                " Usage: \n"
                "   hillshade input_dem output_hillshade \n"
                "                 [-z ZFactor (default=1)] [-s scale* (default=1)] \n"
                "                 [-az Azimuth (default=315)] [-alt Altitude (default=45)]\n\n"
                " Notes : \n"
                "   Scale for Feet:Latlong use scale=370400, for Meters:LatLong use scale=111120 \n\n");
        exit(1);
    }

    const char  *pszFilename = papszArgv[1];
    const char  *pszShadeFilename = papszArgv[2];

    for ( int iArg = 3; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-z") )
            z = atof(papszArgv[iArg+1]);
        if( EQUAL(papszArgv[iArg],"-s") ||
                EQUAL(papszArgv[iArg],"-scale"))
            scale = atof(papszArgv[iArg+1]);
        if( EQUAL(papszArgv[iArg],"-az") ||
                EQUAL(papszArgv[iArg],"-azimuth"))
            az = atof(papszArgv[iArg+1]);
        if( EQUAL(papszArgv[iArg],"-alt") ||
                EQUAL(papszArgv[iArg],"-altitude"))
            alt = atof(papszArgv[iArg+1]);
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

    /* -------------------------------------
    * Get variables from input dataset
    */
    const double   nsres = adfGeoTransform[5];
    const double   ewres = adfGeoTransform[1];
    const float    inputNullValue = (float) poBand->GetNoDataValue( );
    const float    nullValue = 0.0;
    const int      nXSize = poBand->GetXSize();
    const int      nYSize = poBand->GetYSize();
    shadeBuf       = (float *) CPLMalloc(sizeof(float)*nXSize);
    win            = (float *) CPLMalloc(sizeof(float)*9);

    /* -----------------------------------------
     * Create the output dataset and copy over relevant metadata
     */
    GDALDriver *poDriver;
    poDriver = GetGDALDriverManager()->GetDriverByName(pszFormat);
    GDALDataset      *poShadeDS;
    GDALRasterBand   *poShadeBand;
    char **papszOptions = NULL;

    poShadeDS = poDriver->Create(pszShadeFilename,nXSize,nYSize,1,GDT_Byte,
                                 papszOptions );
    poShadeDS->SetGeoTransform( adfGeoTransform );
    poShadeDS->SetProjection( poDataset->GetProjectionRef() );
    poShadeBand = poShadeDS->GetRasterBand(1);
    poShadeBand->SetNoDataValue( nullValue );


    /* ------------------------------------------
     * Move a 3x3 window over each cell 
     * (where the cell in question is #4)
     *
     *                 0 1 2
     *                 3 4 5
     *                 6 7 8
    *
     */
    for ( i = 0; i < nYSize; i++) {
        for ( j = 0; j < nXSize; j++) {
            containsNull = 0;

            // Exclude the edges
            if (i == 0 || j == 0 || i == nYSize-1 || j == nXSize-1 )
            {
                // We are at the edge so write nullValue and move on
                shadeBuf[j] = nullValue;
                continue;
            }

            // Read in 3x3 window
            poBand->RasterIO( GF_Read, j-1, i-1, 3, 3,
                              win, 3, 3, GDT_Float32,
                              0, 0 );

            // Check if window has null value
            for ( n = 0; n <= 8; n++) {
                if(win[n] == inputNullValue) {
                    containsNull = 1;
                    break;
                }
            }

            if (containsNull == 1) {
                // We have nulls so write nullValue and move on
                shadeBuf[j] = nullValue;
                continue;
            } else {
                // We have a valid 3x3 window.

                /* ---------------------------------------
                * Compute Hillshade
                */

                // First Slope ...
                x = ((z*win[0] + z*win[3] + z*win[3] + z*win[6]) -
                     (z*win[2] + z*win[5] + z*win[5] + z*win[8])) /
                    (8.0 * ewres * scale);

                y = ((z*win[6] + z*win[7] + z*win[7] + z*win[8]) -
                     (z*win[0] + z*win[1] + z*win[1] + z*win[2])) /
                    (8.0 * nsres * scale);

                slope = 90.0 - atan(sqrt(x*x + y*y))*radiansToDegrees;

                // ... then aspect...
                aspect = atan2(x,y);

                // ... then the shade value
                cang = sin(alt*degreesToRadians) * sin(slope*degreesToRadians) +
                       cos(alt*degreesToRadians) * cos(slope*degreesToRadians) *
                       cos((az-90.0)*degreesToRadians - aspect);

                if (cang <= 0.0) 
                    cang = 1.0;
                else
                    cang = 1.0 + (254.0 * cang);

                shadeBuf[j] = cang;

            }
        }

        /* -----------------------------------------
         * Write Line to Raster
         */
        poShadeBand->RasterIO( GF_Write, 0, i, nXSize, 1,
                               shadeBuf, nXSize, 1, GDT_Float32, 0, 0 );

    }

    delete poShadeDS;

    return 0;

}
