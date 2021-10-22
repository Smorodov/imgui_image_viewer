#pragma once
#include <cstdio>
#include <iostream>
#include <set>
#include <map>
#include <vector>
#include <array>
#include <algorithm>
#include <codecvt>

#include <filesystem>
namespace fs = std::filesystem;

#include <fstream>
#include <sstream>

#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;

inline void toUpper(std::string& str)
{
    transform(
        str.begin(), str.end(),
        str.begin(),
        toupper);
}

inline void parseFoldersRecursive(fs::path root_path, std::set<std::string>& extensions, std::vector<fs::path>& filepaths)
{
    // Копируем вложенные папки в папку результата
    for (const auto& entry : fs::directory_iterator(root_path))
    {
        std::string name = entry.path().string();
        if (entry.is_directory())
        {
            parseFoldersRecursive(entry.path(), extensions, filepaths);
        }
        else
        {
            std::string ext = entry.path().extension().string();
            toUpper(ext);
            if (extensions.find(ext) != extensions.end())
            {
                filepaths.push_back(entry.path());
            }
        }
    }
}


union
{
    char c[4];
    short s[2];
    int i;
}b32;

union
{
    char c[8];
    int i[2];
}b64;

// http://www.cplusplus.com/forum/general/268937/

inline bool readbyte(int& byte, std::istream& ifs)
{
    return (byte = ifs.get()) >= 0;
}

inline bool readword(int& word, std::istream& ifs)
{
    int cc = 0, dd = 0;
    if ((cc = ifs.get()) < 0 || (dd = ifs.get()) < 0)
    {
        return false;
    }
    word = (cc << 8) + dd;
    return true;
}

inline bool getJpegSize(std::istream& ifs, int& image_width, int& image_height)
{
    int marker = 0, dummy = 0;
    if (ifs.get() != 0xFF || ifs.get() != 0xD8)
    {
        return 0;
    }

    while (true)
    {
        int discarded_bytes = 0;
        readbyte(marker, ifs);

        while (marker != 0xFF)
        {
            ++discarded_bytes;
            readbyte(marker, ifs);
        }

        do readbyte(marker, ifs); while (marker == 0xFF);

        if (discarded_bytes != 0)
        {
            return 0;
        }

        switch (marker)
        {
        case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC5:
        case 0xC6: case 0xC7: case 0xC9: case 0xCA: case 0xCB:
        case 0xCD: case 0xCE: case 0xCF:
        {
            readword(dummy, ifs);    // usual parameter length count
            readbyte(dummy, ifs);
            readword(image_height, ifs);
            readword(image_width, ifs);
            readbyte(dummy, ifs);
            return true;
        }

        case 0xDA: case 0xD9:
            return false;

        default:
        {
            int length;
            readword(length, ifs);
            if (length < 2)
            {
                return 0;
            }
            length -= 2;

            for (; length > 0; --length)
            {
                readbyte(dummy, ifs);
            }
        }
        }
    }
}

inline bool getGifSize(std::istream& ifs, int& image_width, int& image_height)
{    
    std::vector<char> buf(10);
    std::vector<char> magic_string1 = { 'G','I','F','8','7','a' };
    std::vector<char> magic_string2 = { 'G','I','F','8','9','a' };
    ifs.read(buf.data(), 10); 
    std::vector<char> magic;
    magic.assign(buf.begin(), buf.begin() + 6);
    if (magic == magic_string1 || magic == magic_string2)
    {
        //  GIFs
        memcpy(b32.c, buf.data() + 6, 4);
        image_width = b32.s[0];
        image_height = b32.s[1];
    }
    return true;
}

inline bool getPngSize(std::istream& ifs, int& image_width, int& image_height)
{
    std::vector<char> buf(24);
    std::vector<char> magic_string = { '\211','P','N','G','\r','\n','\032','\n'};
    std::vector<char> ihdr_string = { 'I','H','D','R'};
    ifs.read(buf.data(), 24);
    std::vector<char> magic;
    magic.assign(buf.begin(), buf.begin() + 8);
    std::vector<char> ihdr;
    ihdr.assign(buf.begin()+12, buf.begin() + 16);
    if (magic == magic_string)
    {
        if (ihdr == ihdr_string)
        {
            std::reverse(buf.begin() + 16, buf.begin() + 24);            
            memcpy(b64.c, buf.data() + 16, 8);
            image_width = b64.i[1];
            image_height = b64.i[0];
        }
        else
        {
            std::reverse(buf.begin() + 8, buf.begin() + 16);
            memcpy(b64.c, buf.data() + 8, 8);
            image_width = b64.i[1];
            image_height = b64.i[0];
        }
    }
    return true;
}

inline bool getBmpSize(std::istream& ifs, int& image_width, int& image_height)
{
    std::vector<char> buf(26);
    std::vector<char> magic_string = { 'B','M'};    
    ifs.read(buf.data(), 26);
    std::vector<char> magic;
    magic.assign(buf.begin(), buf.begin() + 2);
    if (magic == magic_string)
    { 
        memcpy(b32.c, buf.data() + 14, 4);
        int header_size = b32.i;
        if (header_size == 12)
        {
            memcpy(b32.c, buf.data() + 18, 4);
            image_width = b32.s[0];
            image_height = b32.s[1];
        }
        if (header_size >= 40)
        {
            memcpy(b64.c, buf.data() + 18, 8);
            image_width = b64.i[0];
            image_height = b64.i[1];
        }
    }
    return true;
}


// tiff type    size    name
//     1         1      BYTE
//     2         1      ASCII
//     3         2      SHORT
//     4         4      LONG
//     5         8      RATIONAL
//     6         1      SBYTE
//     7         1      UNDEFINED
//     8         2      SSHORT
//     9         4      SLONG
//     10        8      SRATIONAL
//     11        4      FLOAT
//     12        8      DOUBLE

inline bool getTiffSize(std::istream& ifs, int& image_width, int& image_height)
{
    std::vector<char> buf(26);
    std::vector<char> magic_string1 = { 'M','M',0,42};
    std::vector<char> magic_string2 = { 'I','I',42,0};
    ifs.read(buf.data(), 8);
    std::vector<char> magic;
    magic.assign(buf.begin(), buf.begin() + 4);

    // Standard TIFF, big - or little - endian
     // BigTIFF and other different but TIFF - like formats are not
     // supported currently
    if (magic == magic_string1 || magic == magic_string2)
    {
        std::vector<char> byteOrder;
        std::vector<char> byte_order_reverse_string = { 'M','M'};
        byteOrder.assign(buf.begin(), buf.begin() + 2);
        bool needReverse = false;
        if (byteOrder == byte_order_reverse_string)
        {
            needReverse = true;
        }
        if (needReverse)
        {
            std::reverse(buf.begin() + 4, buf.begin() + 8);
            memcpy(b32.c, buf.data() + 4, 4);
        }
        else
        {
            memcpy(b32.c, buf.data() + 4, 4);
        }
        int ifdOffset = b32.i;

        ifs.seekg(ifdOffset);

        unsigned short ifdEntryCount;
        ifs.read((char*)&ifdEntryCount, 2);
        if (needReverse)
        {
            std::swap(((char*)(&ifdEntryCount))[0], ((char*)(&ifdEntryCount))[1]);
        }
        // 2 bytes: TagId + 2 bytes : type + 4 bytes : count of values + 4
        // bytes : value offset
        int ifdEntrySize = 12;
        for (int i = 0; i < ifdEntryCount; ++i)
        {
            int entryOffset = ifdOffset + 2 + i * ifdEntrySize;
            ifs.seekg(entryOffset);
            short tag;
            ifs.read((char*)&tag, 2);
            if (needReverse)
            {
                std::swap(((char*)(&tag))[0], ((char*)(&tag))[1]);
            }
            if (tag == 256 || tag == 257)
            {
                // if type indicates that value fits into 4 bytes, value
                // offset is not an offset but value itself
                short type;
                ifs.read((char*)&type, 2);
                if (needReverse)
                {
                    std::swap(((char*)(&type))[0], ((char*)(&type))[1]);
                }
                ifs.seekg(entryOffset + 8);
                int val = 0;
                if (type == 4) // int
                {
                    int value;
                    ifs.read((char*)&value, 4);
                    if (needReverse)
                    {
                        char* c = (char*)&value;
                        std::reverse(c, c + 4);
                        memcpy(b32.c, c, 4);
                        val = b32.i;                        
                    }
                }

                if (type == 3) // short
                {
                    short value;
                    ifs.read((char*)&value, 2);
                    if (needReverse)
                    {
                        std::swap(((char*)&value)[0], ((char*)&value)[1]);
                    }
                    val = value;
                }

                if (tag == 256)
                {
                    image_width = val;
                }
                else
                {
                    image_height = val;
                }
                if (image_height > 0 && image_width > 0)
                {
                    return true;
                }
            }

        }
        //std::cout << "tiff " << " size=" << size << " byte order=" << byteOrder << "offset=" << ifdOffset << std::endl;
    }
    return false;
}

// http://paulbourke.net/dataformats/ppm/
inline bool getPpmPgmPbmSize(std::istream& ifs, int& image_width, int& image_height)
{
    std::string magic;
    magic.resize(2);
    ifs.read(magic.data(), 2);
    if (magic == "P1" ||
        magic == "P2" ||
        magic == "P3" ||
        magic == "P4" ||
        magic == "P5" ||
        magic == "P6"
        )
    {   
        std::string line;
        for (;ifs.good();)
        {            
            std::getline(ifs, line);
            auto pos = line.find_first_of("#");
            if (pos != -1)
            {
                line.erase(pos); // trim comments
            }
            line.erase(0, line .find_first_not_of("\t\n\v\f\r ")); // left trim
            line.erase(line.find_last_not_of("\t\n\v\f\r ") + 1); // right trim            
            if (line!=""&&line!="#")
            {
                std::stringstream ss(line);
                if (image_width == -1)
                {
                    ss >> image_width;
                }
                if (image_height == -1)
                {
                    ss >> image_height;
                    break;
                }
            }            
        }
        return true;
    }
    else
    {
        return false;
    }
}

inline void getImageSize(fs::path path, int& image_width, int& image_height)
{
    image_width=-1;
    image_height=-1;
    size_t size = fs::file_size(path);
    std::string ext = path.extension().string();
    toUpper(ext);

    if (ext == ".GIF")
    {
        std::ifstream ifs(path, std::ios::binary);
        getGifSize(ifs, image_width, image_height);
        ifs.close();
    }
    else if (ext == ".PNG")
    {
        std::ifstream ifs(path, std::ios::binary);
        getPngSize(ifs, image_width, image_height);
        ifs.close();
    }
    else if (ext == ".BMP")
    {
        std::ifstream ifs(path, std::ios::binary);
        getBmpSize(ifs, image_width, image_height);
        ifs.close();
    }
    else if (ext == ".TIFF")
    {
        std::ifstream ifs(path, std::ios::binary);
        getTiffSize(ifs, image_width, image_height);
        ifs.close();
    }
    else if (ext == ".JPG" || ext == ".JPEG")
    {
        std::ifstream ifs(path, std::ios::binary);
        getJpegSize(ifs, image_width, image_height);
        ifs.close();
    }
    else if (ext == ".PPM" || ext == ".PGM" || ext == ".PBM")
    {
        std::ifstream ifs(path); // ascii encoded format
        getPpmPgmPbmSize(ifs, image_width, image_height);
        ifs.close();
    }
}
/*
int main(int, char* [])
{
    std::vector<fs::path> filepaths;
    fs::path root_path("F:/ImagesForTest/");
    std::set<std::string> extensions = { ".JPEG",".JPG",".PNG",".BMP",".TIFF",".GIF",".PPM",".PGM",".PBM" };
    // get images file list
    parseFoldersRecursive(root_path, extensions, filepaths);
    int N = filepaths.size();

    auto t1 = high_resolution_clock::now();
    int width = -1;
    int height = -1;
    for (int i = 0; i < N; i++)
    {
        getImageSize(filepaths[i], width, height);
        //std::cout << "w=" << width << " h=" << height << std::endl;
    }
    auto t2 = high_resolution_clock::now();
    auto ms_int = duration_cast<milliseconds>(t2 - t1);
    std::cout << "scanned " << N << " images" << std::endl;
    std::cout << "elapsed " << ms_int.count()<< " ms" << std::endl;
    std::cout << "done!" << std::endl;
}
*/