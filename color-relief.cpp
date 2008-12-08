//=============================================================================
// color-relief.cxx
// Author  : Paul Surgeon
// Date    : 2005-12-22
// License : 
/*
 Copyright 2005 Paul Surgeon
 Licensed under the Apache License, Version 2.0 (the "License"); 
 you may not use this file except in compliance with the License. 
 You may obtain a copy of the License at 
 
 http://www.apache.org/licenses/LICENSE-2.0 
 
 Unless required by applicable law or agreed to in writing, software 
 distributed under the License is distributed on an "AS IS" BASIS, 
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 See the License for the specific language governing permissions and 
 limitations under the License.
 */
//
// Calculates a color relief image from a gdal-supported raster DEM and a
// color scale file.
// Modified from hillshade.cpp by Matthew Perry
//
// CHANGELOG
// - Updated nodata values to 0. RGB of 0,0,0 = null cell. This implies that 
//   no true black can be used in color ramps.
//=============================================================================

#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <list>
#include "gdal_priv.h"
#include "stringtok.h"

using namespace std;

struct SColor
{
  int Red;
  int Green;
  int Blue;
};

struct SColorPoint
{
  int Elevation;
  SColor Color;
};

vector<SColorPoint*> ColorPointList;

//=============================================================================
void ReadColorScale(const string& ScaleFileName)
{
  ifstream ScaleFile;
  string Buffer;
  list<string> StringList;
  SColorPoint* TempColorPoint;

  ScaleFile.open(ScaleFileName.c_str(), ios::in);

  if (!ScaleFile.is_open())
  {
    cout << "Error opening color scale file : " << ScaleFileName << endl;
    exit (1);
  }

  while (!ScaleFile.eof())
  {
    StringList.clear();
    getline(ScaleFile, Buffer);

    // Strip spaces in case we have a blank line
    while (Buffer[0] == ' ')
    {
      Buffer.erase(0);
    }

    // If not a blank line
    if (Buffer != "")
    {
      TempColorPoint = new SColorPoint;
      stringtok(StringList, Buffer, " ");
      list<string>::iterator i = StringList.begin();
      TempColorPoint->Elevation = atoi(string(*i).c_str());
      i++;
      TempColorPoint->Color.Red = atoi(string(*i).c_str());
      i++;
      TempColorPoint->Color.Green = atoi(string(*i).c_str());
      i++;
      TempColorPoint->Color.Blue = atoi(string(*i).c_str());

      ColorPointList.push_back(TempColorPoint);
    }
  }

  ScaleFile.close();
}

//=============================================================================
// Given an elevation calculate a color based on the color points table
// At the moment we're only doing linear color gradients
//=============================================================================
SColor GetColor(float Elevation)
{
  SColor Color = {0,0,0};
  SColorPoint* LowerColorPoint = NULL;
  SColorPoint* UpperColorPoint = NULL;
  SColorPoint* TempColorPoint;
  float TempElev;
  float DiffFactor;

  // Find closest pair of color points that the elevation falls between
  // Lower color point
  TempElev = -64000;
  for (unsigned int i = 0; i < ColorPointList.size(); i++)
  {
    TempColorPoint = ColorPointList[i];
    if ((TempColorPoint->Elevation <= Elevation) && (TempElev < TempColorPoint->Elevation))
    {
      TempElev = TempColorPoint->Elevation;
      LowerColorPoint = TempColorPoint;
    }
  }
  // Upper color point
  TempElev = 64000;
  for (unsigned int i = 0; i < ColorPointList.size(); i++)
  {
    TempColorPoint = ColorPointList[i];
    if ((TempColorPoint->Elevation >= Elevation) && (TempElev > TempColorPoint->Elevation))
    {
      TempElev = TempColorPoint->Elevation;
      UpperColorPoint = TempColorPoint;
    }
  }

  // I should change the following clipping logic to use the color scale instead
  // Return blue
  if (LowerColorPoint == NULL)
  {
    Color.Red = 150;
    Color.Green = 150;
    Color.Blue = 255;
    return Color;
  }

  // Return white
  if (UpperColorPoint == NULL)
  {
    Color.Red = 255;
    Color.Green = 255;
    Color.Blue = 255;
    return Color;
  }

  // Work out the factor the elevation is between the lower and upper color point elevations
  // If the upper and lower color points point to the same color point object then
  // it means that the elevation falls exactly on a color point
  if (LowerColorPoint != UpperColorPoint)
  {
    DiffFactor = (Elevation - LowerColorPoint->Elevation) / (UpperColorPoint->Elevation - LowerColorPoint->Elevation);
    Color.Red   = (int)((UpperColorPoint->Color.Red - LowerColorPoint->Color.Red) * DiffFactor) + LowerColorPoint->Color.Red;
    Color.Green = (int)((UpperColorPoint->Color.Green - LowerColorPoint->Color.Green) * DiffFactor) + LowerColorPoint->Color.Green;
    Color.Blue  = (int)((UpperColorPoint->Color.Blue - LowerColorPoint->Color.Blue) * DiffFactor) + LowerColorPoint->Color.Blue;
  }
  else
  {
    Color.Red   = LowerColorPoint->Color.Red;
    Color.Green = LowerColorPoint->Color.Green;
    Color.Blue  = LowerColorPoint->Color.Blue;
  }

  return Color;
}

//=============================================================================
int main(int argc, char* argv[])
{
  GDALDataset* poDataset;
  double       adfGeoTransform[6];
  float*       RowRed;
  float*       RowGreen;
  float*       RowBlue;
  float        InPixel;
  int          i;
  int          j;
  const char*  Format = "GTiff";
  SColor       TempColor;

  if (argc < 3)
  {
    cout << "color-relief generates a color relief map from any GDAL-supported elevation raster." << endl;
    cout << endl << "Usage:" << endl;
    cout << "color-relief <input_dem> <input_color_scale> <output_relief_map>" << endl << endl;
    cout << "The input color scale is a file containing a set of elevation points (in meters)" << endl;
    cout << "and colors. Typically only a small number of elevation and color sets will be needed" << endl;
    cout << "and the rest will be interpolated by color-relief." << endl;
    cout << "Example color scale file with 4000 meters set to white and 0 meters set to green:" << endl;
    cout << "4000 255 255 255" << endl;
    cout << "0 0 255 0" << endl << endl;
    cout << "Using true black (0 0 0) as your RGB values will yield blank/null cells." << endl;
    cout << "Note that to remove nodata from the output, set the DEM's nodata value to rgb of 0 0 0:" << endl;
    cout << "-32767 0 0 0" << endl << endl;
    cout << "See the accompanying \"scale.txt\" file for a decent example." << endl;
    exit(1);
  }

  const char* InFilename = argv[1];
  const string ScaleFilename = argv[2];
  const char* OutFilename = argv[3];

  // Open and read color scale file
  ReadColorScale(ScaleFilename);

  GDALAllRegister();

  // Open dataset and get raster band
  poDataset = (GDALDataset*) GDALOpen(InFilename, GA_ReadOnly);
  if(poDataset == NULL)
  {
    cout << "Couldn't open dataset " << InFilename << endl;
  }

  GDALRasterBand *poBand;
  poBand = poDataset->GetRasterBand(1);
  poDataset->GetGeoTransform(adfGeoTransform);

  // Get variables from input dataset
  const int nXSize = poBand->GetXSize();
  const int nYSize = poBand->GetYSize();
  RowRed    = (float *) CPLMalloc(sizeof(float)*nXSize);
  RowGreen  = (float *) CPLMalloc(sizeof(float)*nXSize);
  RowBlue   = (float *) CPLMalloc(sizeof(float)*nXSize);

  // Create the output dataset and copy over relevant metadata
  GDALDriver *poDriver;
  poDriver = GetGDALDriverManager()->GetDriverByName(Format);
  GDALDataset      *poDS;
  GDALRasterBand   *poBandRed;
  GDALRasterBand   *poBandGreen;
  GDALRasterBand   *poBandBlue;
  char**           Options = NULL;

  poDS = poDriver->Create(OutFilename,nXSize,nYSize,3,GDT_Byte,Options);
  poDS->SetGeoTransform(adfGeoTransform);
  poDS->SetProjection(poDataset->GetProjectionRef());

  poBandRed = poDS->GetRasterBand(1);
  poBandRed->SetNoDataValue(0);
  poBandGreen = poDS->GetRasterBand(2);
  poBandGreen->SetNoDataValue(0);
  poBandBlue = poDS->GetRasterBand(3);
  poBandBlue->SetNoDataValue(0);

  // Run through each pixel in an image
  for (i = 0; i < nYSize; i++)
  {
    for (j = 0; j < nXSize; j++)
    {
      poBand->RasterIO(GF_Read, j, i, 1, 1, &InPixel, 1, 1, GDT_Float32, 0, 0);

      TempColor = GetColor(InPixel);
      RowRed[j]   = TempColor.Red;
      RowGreen[j] = TempColor.Green;
      RowBlue[j]  = TempColor.Blue;
     }

    // Write lines to output raster
    poBandRed->RasterIO(GF_Write, 0, i, nXSize, 1, RowRed, nXSize, 1, GDT_Float32, 0, 0);
    poBandGreen->RasterIO(GF_Write, 0, i, nXSize, 1, RowGreen, nXSize, 1, GDT_Float32, 0, 0);
    poBandBlue->RasterIO(GF_Write, 0, i, nXSize, 1, RowBlue, nXSize, 1, GDT_Float32, 0, 0);
  }

  delete poDS;

  return 0;
}
