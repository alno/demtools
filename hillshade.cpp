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
    float   s;
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
    int         winDist = 1;
    float       sharp = 2;

    /* -----------------------------------
     * Parse Input Arguments
     */
    if (nArgc < 2)
    {
        printf( " \n Generates a shaded relief map from any GDAL-supported elevation raster\n"
                " Usage: \n"
                "   hillshade input_dem output_hillshade \n"
                "                 [-z ZFactor (default=1)] [-s scale* (default=1)] \n"
                "                 [-az Azimuth (default=315)] [-alt Altitude (default=45)]\n"
                "                 [-wd Halfsize of window (default=1)] [-sh Sharpness coeff (default=2.0)]\n\n"
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
        if( EQUAL(papszArgv[iArg],"-wd") ||
                EQUAL(papszArgv[iArg],"-windist"))
            winDist = atoi(papszArgv[iArg+1]);
        if( EQUAL(papszArgv[iArg],"-sh") ||
                EQUAL(papszArgv[iArg],"-sharpness"))
            sharp = atof(papszArgv[iArg+1]);
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

    const int winSize = 2 * winDist + 1;
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
    win            = (float *) CPLMalloc(sizeof(float)*winSize*winSize);

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
     * Move a SxS window over each cell
     * (where the cell in question is (winSize + 1) * winDist)
     */
    for ( i = 0; i < nYSize; i++) {
        for ( j = 0; j < nXSize; j++) {
            containsNull = 0;

            // Exclude the edges
            if (i < winDist || j < winDist || i >= nYSize-winDist || j >= nXSize-winDist )
            {
                // We are at the edge so write nullValue and move on
                shadeBuf[j] = nullValue;
                continue;
            }

            // Read in 3x3 window
            poBand->RasterIO( GF_Read, j-winDist, i-winDist, winSize, winSize,
                              win, winSize, winSize, GDT_Float32,
                              0, 0 );

            // Check if window has null value
            for ( n = 0; n < winSize*winSize; n++) {
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
                // We have a valid SxS window.

                /* ---------------------------------------
                * Compute Hillshade
                */

                x = 0;
                y = 0;
                s = 0;

                for (int i = 1; i <= winDist; ++ i) {
                    const int wd = winDist;
                    const int ws = winSize;

                    for (int j = 1; j <= winDist; ++ j ) {
                        double c = pow(sharp, 2*winDist - i - j);

                        s += c * 4;
                        x += (win[wd - i + (wd - j)*ws ] + win[wd - i + (wd + j)*ws] - win[wd + i + (wd - j)*ws] - win[wd + i + (wd + j)*ws]) * c;
                        y += (win[wd - j + (wd + i)*ws ] + win[wd + j + (wd + i)*ws] - win[wd - j + (wd - i)*ws] - win[wd + j + (wd - i)*ws]) * c;
                    }

                    double c = pow(sharp, 2*winDist - i);

                    s += c * 2;
                    x += (win[wd - i + wd*ws ] - win[wd + i + wd*ws]) * c;
                    y += (win[wd + (wd+i)*ws ] - win[wd + (wd-i)*ws]) * c;
                }

                x /= s * ewres * scale;
                y /= s * nsres * scale;

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
