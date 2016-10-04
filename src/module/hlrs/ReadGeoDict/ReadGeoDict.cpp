/* This file is part of COVISE.

   You can use it under the terms of the GNU Lesser General Public License
   version 2.1 or later, see lgpl-2.1.txt.

 * License: LGPL 2+ */

// +++++++++++++++++++++++++++++++++++++++++
// MODULE ReadGeoDict
//
// This module Reads GeoDict files
//

#include "ReadGeoDict.h"

#ifndef _WIN32
#include <inttypes.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include <do/coDoData.h>
#include <do/coDoSet.h>
#include <do/coDoUnstructuredGrid.h>
#include <do/coDoPoints.h>
#include <do/coDoUniformGrid.h>

// remove  path from filename
inline const char *coBasename(const char *str)
{
    const char *lastslash = strrchr(str, '/');
    const char *lastslash2 = strrchr(str, '\\');
    if (lastslash2 > lastslash)
        lastslash = lastslash2;
    if (lastslash)
        return lastslash + 1;
    else
    {
        return str;
    }
}

// Module set-up in Constructor
ReadGeoDict::ReadGeoDict(int argc, char *argv[])
    : coReader(argc, argv, "Read GeoDict data")
{
    d_dataFile = NULL;
}

ReadGeoDict::~ReadGeoDict()
{
}

// param callback read header again after all changes
void
ReadGeoDict::param(const char *paramName, bool inMapLoading)
{

    FileItem *fii = READER_CONTROL->getFileItem(TXT_BROWSER);

    string txtBrowserName;
    if (fii)
    {
        txtBrowserName = fii->getName();
    }

    /////////////////  CALLED BY FILE BROWSER  //////////////////
    if (txtBrowserName == string(paramName))
    {
        FileItem *fi(READER_CONTROL->getFileItem(string(paramName)));
        if (fi)
        {
            coFileBrowserParam *bP = fi->getBrowserPtr();

            if (bP)
            {
                string dataNm(bP->getValue());
                if (dataNm.empty())
                {
                    cerr << "ReadGeoDict::param(..) no data file found " << endl;
                }
                else
                {
                    if (d_dataFile != NULL)
                        fclose(d_dataFile);
                    fileName = dataNm;
                    d_dataFile = fopen(dataNm.c_str(), "r");
                    int result = STOP_PIPELINE;

                    if (d_dataFile != NULL)
                    {
                        result = readHeader();
                        fseek(d_dataFile, 0, SEEK_SET);
                    }

                    if (result == STOP_PIPELINE)
                    {
                        cerr << "ReadGeoDict::param(..) could not read file: " << dataNm << endl;
                    }
                    else
                    {

                        // lists for Choice Labels
                        vector<string> dataChoices;

                        // fill in NONE to READ no data
                        string noneStr("NONE");
                        dataChoices.push_back(noneStr);

                        // fill in all species for the appropriate Ports/Choices
                        for (int i = 0; i < varInfos.size(); i++)
                        {
                            dataChoices.push_back(varInfos[i].name);
                        }
                        if (inMapLoading)
                            return;
                        READER_CONTROL->updatePortChoice(DPORT1_3D, dataChoices);
                        READER_CONTROL->updatePortChoice(DPORT2_3D, dataChoices);
                        READER_CONTROL->updatePortChoice(DPORT3_3D, dataChoices);
                        READER_CONTROL->updatePortChoice(DPORT4_3D, dataChoices);
                        READER_CONTROL->updatePortChoice(DPORT5_3D, dataChoices);
                    }
                }
                return;
            }

            else
            {
                cerr << "ReadGeoDict::param(..) BrowserPointer NULL " << endl;
            }
        }
    }
}

int ReadGeoDict::nextLine()
{
    currentLine = nextLineBuf;
    char *c = currentLine;
    while(*c!='\0' && (*c != '\r' &&  *c != '\n'))
    {
        c++;
    }
    if(*c!='\0')
    {
        *c = '\0';
        c++;
        nextLineBuf = c;
    }
    else
    {
        currentLine = c;
        return false;
    }
    return 1;
}

int ReadGeoDict::readHeader()
{
    if(d_dataFile==NULL)
    {
        d_dataFile = fopen(fileName.c_str(), "rb");
    }
    rewind(d_dataFile);
    size_t headerSize;
    if(headerSize = fread(buf,1,1024,d_dataFile)!=1024)
    {
        coModule::sendWarning("Error reading header %d", headerSize);
        return STOP_PIPELINE;
    }
    else
    {
        nextLineBuf = buf;
    }
    nextLine();

    buf[1024] = '\0';

    std::vector<std::string> names;
    varInfos.clear();
    names.clear();
    
            int lastImageNumber=0;
    while (strlen(currentLine)>0)
    {
        if(strncmp(currentLine,"Image",5)==0)
        {
            int imageNumber=0;
            sscanf(currentLine+5,"%d",&imageNumber);
            if(imageNumber != lastImageNumber)
            {
                VarInfo vi;
                vi.read = false;
                vi.vector = false;
                vi.imageNumber = imageNumber;
                varInfos.push_back(vi);
                lastImageNumber = imageNumber;
            }
            char *c = currentLine+5;
            while(*c!='\0' && (*c != ':'))
            {
                c++;
            }
            if(*c!='\0')
            {
                c++;
                if(strncmp(c,"Names",5) == 0)
                {
                    varInfos[imageNumber-1].name = c+6;
                }
                if(strncmp(c,"Meaning vector",14) == 0)
                {
                    varInfos[imageNumber-1].vector = true;
                }
            }
        }
        if(strncmp(currentLine,"Nx",2)==0)
        {
            sscanf(currentLine+3,"%d",&Nx);
        }
        if(strncmp(currentLine,"Ny",2)==0)
        {
            sscanf(currentLine+3,"%d",&Ny);
        }
        if(strncmp(currentLine,"Nz",2)==0)
        {
            sscanf(currentLine+3,"%d",&Nz);
        }
        if(strncmp(currentLine,"VoxelLength",11)==0)
        {
            sscanf(currentLine+12,"%f,%f,%f",&sx,&sy,&sz);
        }
        
        nextLine();
    }

}

// taken from old ReadGeoDict module: 2-Pass reading
int ReadGeoDict::readData()
{
    int res = readHeader();
    int varNumber=0;
    if (res != STOP_PIPELINE)
    {
        std::string objNameBase = READER_CONTROL->getAssocObjName(MESHPORT3D);
        coDoUniformGrid *grid = new coDoUniformGrid(objNameBase.c_str(), Nx,Ny,Nz,0,Nx*sx*1000000,0,Ny*sy*1000000,0,Nz*sz*1000000);
        int numValues = Nx*Ny*Nz;
        while(varNumber < varInfos.size())
        {

            int portID = 0;
            for (int n = 0; n < 5; n++)
            {
                int pos = READER_CONTROL->getPortChoice(DPORT1_3D + n);
                //printf("%d %d\n",pos,n);
                if (pos > 0)
                {
                    if (varInfos[pos - 1].imageNumber == varNumber+1 && varInfos[pos - 1].read == false) // this port needs want to have this data
                    {
                        portID = DPORT1_3D + n;
                        if(varInfos[pos - 1].vector)
                        {
                            coDoVec3 *dataObj = new coDoVec3(READER_CONTROL->getAssocObjName(DPORT1_3D + n).c_str(), numValues);
                            dataObj->getAddresses(&varInfos[pos - 1].x_d,&varInfos[pos - 1].y_d,&varInfos[pos - 1].z_d);
                            float *dataBuf = new float[numValues * 3];
                            fread(dataBuf,sizeof(float),numValues*3,d_dataFile);
                            for(int u=0;u<Nx;u++)
                            for(int v=0;v<Ny;v++)
                            for(int w=0;w<Nz;w++)
                            {
                                varInfos[pos - 1].x_d[u*Ny*Nz+v*Nz+w]=dataBuf[w*Ny*Nx*3+v*Nx*3+u*3];
                                varInfos[pos - 1].y_d[u*Ny*Nz+v*Nz+w]=dataBuf[w*Ny*Nx*3+v*Nx*3+u*3+1];
                                varInfos[pos - 1].z_d[u*Ny*Nz+v*Nz+w]=dataBuf[w*Ny*Nx*3+v*Nx*3+u*3+2];
                            }
                            delete[] dataBuf;
                        }
                        else
                        {
                            coDoFloat *dataObj = new coDoFloat(READER_CONTROL->getAssocObjName(DPORT1_3D + n).c_str(), numValues);
                            dataObj->getAddress(&varInfos[pos - 1].x_d);
                            
                            float *dataBuf = new float[numValues];
                            fread(dataBuf,sizeof(float),numValues,d_dataFile);
                            for(int u=0;u<Nx;u++)
                            for(int v=0;v<Ny;v++)
                            for(int w=0;w<Nz;w++)
                            {
                                varInfos[pos - 1].x_d[u*Ny*Nz+v*Nz+w]=dataBuf[w*Ny*Nx+v*Nx+u];
                            }
                            delete[] dataBuf;
                        }
                        varInfos[pos - 1].read = true;
                    }
                }
            }
        varNumber++;
        }

    }
    return CONTINUE_PIPELINE;
}
int ReadGeoDict::compute(const char *)
{
    if (d_dataFile == NULL)
    {
        if (fileName.empty())
        {
            cerr << "ReadGeoDict::param(..) no data file found " << endl;
        }
        else
        {

            d_dataFile = fopen(fileName.c_str(), "rb");
        }
    }
    int result = STOP_PIPELINE;

    if (d_dataFile != NULL)
    {
        result = readData();
        fclose(d_dataFile);
        d_dataFile = NULL;
    }
    return result;
}

int main(int argc, char *argv[])
{

    // define outline of reader
    READER_CONTROL->addFile(TXT_BROWSER, "data_file_path", "Data file path", ".", "*.vap;*.GeoDict");

    READER_CONTROL->addOutputPort(MESHPORT3D, "geoOut_3D", "structured Grid", "Mesh", false);

    READER_CONTROL->addOutputPort(DPORT1_3D, "data1_3D", "Float|Vec3", "data1-3d");
    READER_CONTROL->addOutputPort(DPORT2_3D, "data2_3D", "Float|Vec3", "data2-3d");
    READER_CONTROL->addOutputPort(DPORT3_3D, "data3_3D", "Float|Vec3", "data3-3d");
    READER_CONTROL->addOutputPort(DPORT4_3D, "data4_3D", "Float|Vec3", "data4-3d");
    READER_CONTROL->addOutputPort(DPORT5_3D, "data5_3D", "Float|Vec3", "data5-3d");

    // create the module
    coReader *application = new ReadGeoDict(argc, argv);

    // this call leaves with exit(), so we ...
    application->start(argc, argv);

    // ... never reach this point
    return 0;
}
