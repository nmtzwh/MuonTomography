/* Provide file read and write method for muon data and image data.
   This class is designed as static class, e.g. should be access with class not object.
   Members are stateless except for the string representing current path. 
*/

/* ----- image header design ------ 
    | Length     | version    | 0-15 bits
    ---------------------------
    | method                  | 16 x char
    |                         |
    ---------------------------
    | number of rays          | unsigned int
    |                         |
    ---------------------------
    | number of iteration     | usigned short
    | floating point prec.    |
    ---------------------------
    | Grid information        | 3 x unsigned short for nx,ny,nz
    | ...                     | 6 x float for x_min, dx, ...
*/

#include <iostream>
#include <fstream>

#include "MutoTypes.h"

class MutoFile {
public:
    MutoFile() {}
    ~MutoFile() {}

    // muon ray data file handling (process data)
    // inputs:
    //    + vector of file names
    //    + height of each detector layer
    //    + id of lowest Upper detectors
    //    + precision of stored float data (default to 4, i.e. float in C++)  
    // output:
    //    + vector contains all the muon data (RayData structure)
    static MutoMuonData getMuonData(std::vector<std::string> flist, std::vector<MTfloat> zlayers, int nMiddle, int fPrecision=4);

    // write image data to file
    // inputs:
    //    + Image (Eigen/Tensor)
    //    + header structure of output image
    //    + desired file name
    // output:
    //    + whether the operation succeed
    static bool saveImage(const Image& img, const ImageHeader& header, std::string filename);

    // write image data to file (w/o header)
    static bool saveImagePure(const Image& img, std::string filename, MTindex fPrecision=4);

    // read image data from file
    // inputs:
    //    + target file name
    // output:
    //    + a pair contains Image and its header
    static std::pair<Image, ImageHeader> loadImage(std::string filename);

    // load image without header
    static Image loadImagePure(std::string filename, VoxelGrid grid, MTindex fPrecision=4);

    // get current working directory
    static std::string getCurrentPath() { return fCwd; }
private:
    static std::string fCwd;
    static RayData getRayDataFromTemp(std::vector<MTfloat>::iterator, const std::vector<MTfloat>&, int);
};