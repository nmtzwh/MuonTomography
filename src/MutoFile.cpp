#include "MutoFile.h"

// get current working directory 
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    char winpath[ MAX_PATH ]; 
    std::string MutoFile::fCwd = std::string( _getcwd( winpath, MAX_PATH ) );
#endif

#ifdef __unix__
    #include <unistd.h>
    char unixpath[ PATH_MAX ]; 
    std::string MutoFile::fCwd = std::string( getcwd( unixpath, PATH_MAX ) );
#endif


// muon ray data file handling (process data)
MutoMuonData MutoFile::getMuonData(std::vector<std::string> flist, std::vector<MTfloat> zlayers, int nMiddle, int fPrecision) {
    // read all the data into memory 
    MutoMuonData data;
    std::ifstream file;

    // set precision of floating numbers in binary file
    bool fDouble = false;

    if (fPrecision == 4) {
        fDouble = false;
    } else if (fPrecision == 8) {
        fDouble = true;
    } else {
        std::cerr << "Floating point precision not supported. " << std::endl;
    }

    // at least two layers of detectors for both up and down area
    if (nMiddle < 2 || zlayers.size() - nMiddle < 2) {
        std::cerr << "Error: at least two layers of detectors for both up and down area. " << std::endl;
    }

    // loop through file list
    for (std::string filename : flist) {
        file.open(filename.c_str(), std::ios::in | std::ios::binary);

        // error handling
        if (file.fail()) {
            std::cout << "Reading muon data error for : " << filename << std::endl;
            file.close();
            continue;
        }

        // get all raw data at once and then process as RayData
        MTindex nEach = 2 * zlayers.size() + 2; // two extra data for in and out energy
        file.seekg(0, std::ios::end);
        MTindex filesize = file.tellg();
        file.seekg(0, std::ios::beg);
        MTindex nRay = filesize / nEach / fPrecision;
 
        std::vector<MTfloat> dRaw (nEach * nRay, 0.0);
        if (fDouble) {
            std::vector<double> dTemp (nEach * nRay, 0.0); // temp data
            file.read(reinterpret_cast<char*>(&dTemp[0]), filesize);
            std::copy(dTemp.begin(), dTemp.end(), dRaw.begin());
        } else {
            std::vector<float> dTemp (nEach * nRay, 0.0); // temp data
            file.read(reinterpret_cast<char*>(&dTemp[0]), filesize);
            std::copy(dTemp.begin(), dTemp.end(), dRaw.begin());
        }

        // process each ray and push into output data object
        for (auto it=dRaw.begin(); it != dRaw.end(); it+=nEach) {
            // check nan before process ray information
            if (std::all_of(it, it+nEach, [](MTfloat i){return !std::isnan(i);})) {
                // process data
                data.push_back(getRayDataFromTemp(it, zlayers, nMiddle));
            }
        }

        file.close();
    }
    
    return data;
}

// write image data to file
bool MutoFile::saveImage(const Image& img, const ImageHeader& header, std::string filename) {
    std::ofstream file;
    file.open(filename.c_str(), std::ios::out | std::ios::binary);
    if (file.fail()) {
        file.close();
        std::cout << "Writing image to file: "  << filename << " failed. " << std::endl;
        return false;
    }

    // write header to front of file
    unsigned char length = 64, version = 1;
    file.write((char*)&length, sizeof(unsigned char));
    file.write((char*)&version, sizeof(unsigned char));
    file.write(header.method.c_str(), 16);
    file.write((char*)&header.nRay, sizeof(unsigned int));
    file.write((char*)&header.nIter, sizeof(unsigned short));
    file.write((char*)&header.fPrecision, sizeof(unsigned short));

    file.write((char*)& header.grid.nx, sizeof(unsigned short));
    file.write((char*)& header.grid.ny, sizeof(unsigned short));
    file.write((char*)& header.grid.nz, sizeof(unsigned short));
    float xmin = static_cast<float>(header.grid.x_min);
    float ymin = static_cast<float>(header.grid.y_min);
    float zmin = static_cast<float>(header.grid.z_min);
    float dx = static_cast<float>(header.grid.dx);
    float dy = static_cast<float>(header.grid.dy);
    float dz = static_cast<float>(header.grid.dz);
    file.write((char*)& xmin, sizeof(float));
    file.write((char*)& ymin, sizeof(float));
    file.write((char*)& zmin, sizeof(float));
    file.write((char*)& dx, sizeof(float));
    file.write((char*)& dy, sizeof(float));
    file.write((char*)& dz, sizeof(float));
    std::vector<char> zeropad(8, 0);
    file.write(&zeropad.front(), 8);

    // loop and write the image
    if (header.fPrecision == 8) {
        file.write((char*)img.data(), img.size() * header.fPrecision);
    } else if (header.fPrecision == 4) {
        Tensor<float, 3> fImg = img.cast<float>();
        file.write((char*)fImg.data(), img.size() * header.fPrecision);
    } else {
        std::cout << "Floating point precision not supported. " << std::endl;
        return false;
    }

    return true;
}

// write image data to file without header
bool MutoFile::saveImagePure(const Image& img, std::string filename, MTindex fPrecision) {
    std::ofstream file;
    file.open(filename.c_str(), std::ios::out | std::ios::binary);
    if (file.fail()) {
        file.close();
        std::cout << "Writing image to file: "  << filename << " failed. " << std::endl;
        return false;
    }

    // loop and write the image
    if (fPrecision == 8) {
        file.write((char*)img.data(), img.size() * fPrecision);
    } else if (fPrecision == 4) {
        Tensor<float, 3> fImg = img.cast<float>();
        file.write((char*)fImg.data(), img.size() * fPrecision);
    } else {
        std::cout << "Floating point precision not supported. " << std::endl;
        return false;
    }

    return true;
}

// read image data from file
std::pair<Image, ImageHeader> MutoFile::loadImage(std::string filename) {
    std::ifstream file;
    file.open(filename.c_str(), std::ios::in | std::ios::binary);
    if (file.fail()) {
        file.close();
        std::cout << "Reading image file: "  << filename << " failed. " << std::endl;
        return std::pair<Image, ImageHeader>{};
    }

    // read header information
    unsigned char length, version;
    char method[16];
    unsigned int nRay;
    unsigned short nIter, fPrecision, nx, ny, nz;
    float xmin, ymin, zmin, dx, dy, dz;
    file.read(reinterpret_cast<char*>(&length), 1);
    file.read(reinterpret_cast<char*>(&version), 1);
    file.read(method, 16);
    file.read(reinterpret_cast<char*>(&nRay), sizeof(unsigned int));
    file.read(reinterpret_cast<char*>(&nIter), sizeof(unsigned short));
    file.read(reinterpret_cast<char*>(&fPrecision), sizeof(unsigned short));
    file.read(reinterpret_cast<char*>(&nx), sizeof(unsigned short));
    file.read(reinterpret_cast<char*>(&ny), sizeof(unsigned short));
    file.read(reinterpret_cast<char*>(&nz), sizeof(unsigned short));
    file.read(reinterpret_cast<char*>(&xmin), sizeof(float));
    file.read(reinterpret_cast<char*>(&ymin), sizeof(float));
    file.read(reinterpret_cast<char*>(&zmin), sizeof(float));
    file.read(reinterpret_cast<char*>(&dx), sizeof(float));
    file.read(reinterpret_cast<char*>(&dy), sizeof(float));
    file.read(reinterpret_cast<char*>(&dz), sizeof(float));
    file.seekg(length); // seek to end of header

    // construct header
    ImageHeader header;
    header.method = method;
    header.nRay = nRay;
    header.fPrecision = fPrecision;
    header.nIter = nIter;    
    VoxelGrid grid {nx, ny, nz, xmin, ymin, zmin, dx, dy, dz};
    header.grid = grid;

    Image img(grid.nx, grid.ny, grid.nz);

    if (header.fPrecision == 8) {        
        file.read(reinterpret_cast<char*>(img.data()), img.size() * header.fPrecision);
    } else if (header.fPrecision == 4) {
        Tensor<float, 3> fImg (grid.nx, grid.ny, grid.nz);
        file.read(reinterpret_cast<char*>(fImg.data()), fImg.size() * header.fPrecision);
        img = fImg.cast<MTfloat>();
    }
    return std::pair<Image, ImageHeader>{img, header};
}

// read image data from file
Image MutoFile::loadImagePure(std::string filename, VoxelGrid grid, MTindex fPrecision) {
    std::ifstream file;
    file.open(filename.c_str(), std::ios::in | std::ios::binary);
    if (file.fail()) {
        file.close();
        std::cout << "Reading image file: "  << filename << " failed. " << std::endl;
        return Image{};
    }

    // read file size
    file.seekg(0, std::ios::end); // seek to end of file
    int filesize = file.tellg(); 

    if (grid.nx * grid.ny * grid.nz * fPrecision != filesize) {
        std::cout << "Wrong size of the image file and grid: "  << filename << std::endl;
        return Image{}; 
    }

    Image img(grid.nx, grid.ny, grid.nz);

    if (fPrecision == 8) {        
        file.read(reinterpret_cast<char*>(img.data()), img.size() * fPrecision);
    } else if (fPrecision == 4) {
        Tensor<float, 3> fImg (grid.nx, grid.ny, grid.nz);
        file.read(reinterpret_cast<char*>(fImg.data()), fImg.size() * fPrecision);
        img = fImg.cast<MTfloat>();
    } else {
        std::cout << "Floating point precision not supported. " << std::endl;
        return Image{};
    }
    return img;
}


RayData MutoFile::getRayDataFromTemp(std::vector<MTfloat>::iterator it0, const std::vector<MTfloat>& z, int m){
    RayData ray;
    MTfloat xm, ym, zm, t;
    Matrix<MTfloat, -1, -1> data;

    ray.ein = *it0;

    // upper layers
    data.resize(m, 3);

    for (MTindex i=0; i<m; ++i) {
        data(i, 0) = *(it0 + i*2 + 1);
        data(i, 1) = *(it0 + i*2 + 2);
        data(i, 2) = z[i];
    }
    xm = data.col(0).mean();
    ym = data.col(1).mean();
    zm = data.col(2).mean();
    for (MTindex i=0; i<m; ++i) {
        data(i, 0) -= xm;
        data(i, 1) -= ym;
        data(i, 2) -= zm;
    }

    // use SVD in Eigen package to fit in/out rays
    JacobiSVD<MatrixXd> svdUp(data, ComputeThinU | ComputeThinV);
    ray.din = svdUp.matrixV().col(0).transpose();
    t = (z[m-1] - zm) / ray.din(2);
    ray.pin = Vector3(t*ray.din(0) + xm, t*ray.din(1) + ym, z[m-1]);

    // bottom layers
    data.resize(z.size() - m, 3);
    xm = ym = zm = 0.0;

    for (MTindex i=0; i<z.size()-m; ++i) {
        data(i, 0) = *(it0 + (i+m)*2 + 1);
        data(i, 1) = *(it0 + (i+m)*2 + 2);
        data(i, 2) = z[i+m];
    }
    xm = data.col(0).mean();
    ym = data.col(1).mean();
    zm = data.col(2).mean();
    for (MTindex i=0; i<z.size()-m; ++i) {
        data(i, 0) -= xm;
        data(i, 1) -= ym;
        data(i, 2) -= zm;
    }
    JacobiSVD<MatrixXd> svdBot(data, ComputeThinU | ComputeThinV);
    ray.dout = svdBot.matrixV().col(0).transpose();
    t = (z[m] - zm) / ray.dout(2);
    ray.pout = Vector3(t*ray.dout(0) + xm, t*ray.dout(1) + ym, z[m]);
    
    // energy of out going muon
    ray.eout = *(it0 + z.size()*2 + 1);

    return ray;
}
