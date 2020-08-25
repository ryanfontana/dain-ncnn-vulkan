// dain implemented with ncnn library

#include <stdio.h>
#include <algorithm>
#include <queue>
#include <vector>
#include <clocale>

// image decoder and encoder with stb
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
// #define STBI_NO_STDIO
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#if _WIN32
#include <wchar.h>
static wchar_t* optarg = NULL;
static int optind = 1;
static wchar_t getopt(int argc, wchar_t* const argv[], const wchar_t* optstring)
{
    if (optind >= argc || argv[optind][0] != L'-')
        return -1;

    wchar_t opt = argv[optind][1];
    const wchar_t* p = wcschr(optstring, opt);
    if (p == NULL)
        return L'?';

    optarg = NULL;

    if (p[1] == L':')
    {
        optind++;
        if (optind >= argc)
            return L'?';

        optarg = argv[optind];
    }

    optind++;

    return opt;
}

static std::vector<int> parse_optarg_int_array(const wchar_t* optarg)
{
    std::vector<int> array;
    array.push_back(_wtoi(optarg));

    const wchar_t* p = wcschr(optarg, L',');
    while (p)
    {
        p++;
        array.push_back(_wtoi(p));
        p = wcschr(p, L',');
    }

    return array;
}
#else // _WIN32
#include <unistd.h> // getopt()

static std::vector<int> parse_optarg_int_array(const char* optarg)
{
    std::vector<int> array;
    array.push_back(atoi(optarg));

    const char* p = strchr(optarg, ',');
    while (p)
    {
        p++;
        array.push_back(atoi(p));
        p = strchr(p, ',');
    }

    return array;
}
#endif // _WIN32

// ncnn
#include "cpu.h"
#include "gpu.h"
#include "platform.h"

#include "dain.h"

#include "filesystem_utils.h"

static void print_usage()
{
    fprintf(stderr, "Usage: dain-ncnn-vulkan -0 infile -1 infile1 -o outfile [options]...\n\n");
    fprintf(stderr, "  -h                   show this help\n");
    fprintf(stderr, "  -0 input0-path       input image0 path (jpg/png)\n");
    fprintf(stderr, "  -1 input1-path       input image1 path (jpg/png)\n");
    fprintf(stderr, "  -o output-path       output image path (jpg/png)\n");
}

void load(const path_t& imagepath, ncnn::Mat& image)
{
    unsigned char* pixeldata = 0;
    int w;
    int h;
    int c;

    pixeldata = stbi_load(imagepath.c_str(), &w, &h, &c, 3);
    if (pixeldata)
    {
        image = ncnn::Mat(w, h, (void*)pixeldata, (size_t)3, 3);
    }
}

void save(const path_t& imagepath, const ncnn::Mat& image)
{
    path_t ext = get_file_extension(imagepath);

    if (ext == PATHSTR("png") || ext == PATHSTR("PNG"))
    {
        stbi_write_png(imagepath.c_str(), image.w, image.h, image.elempack, image.data, 0);
    }
    else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") || ext == PATHSTR("JPEG"))
    {
        stbi_write_jpg(imagepath.c_str(), image.w, image.h, image.elempack, image.data, 100);
    }
}

#if _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    path_t input0path;
    path_t input1path;
    path_t outputpath;

#if _WIN32
    setlocale(LC_ALL, "");
    wchar_t opt;
    while ((opt = getopt(argc, argv, L"0:1:o:h")) != (wchar_t)-1)
    {
        switch (opt)
        {
        case L'0':
            input0path = optarg;
            break;
        case L'1':
            input1path = optarg;
            break;
        case L'o':
            outputpath = optarg;
            break;
        case L'h':
        default:
            print_usage();
            return -1;
        }
    }
#else // _WIN32
    int opt;
    while ((opt = getopt(argc, argv, "0:1:o:h")) != -1)
    {
        switch (opt)
        {
        case '0':
            input0path = optarg;
            break;
        case '1':
            input1path = optarg;
            break;
        case 'o':
            outputpath = optarg;
            break;
        case 'h':
        default:
            print_usage();
            return -1;
        }
    }
#endif // _WIN32

    if (input0path.empty() || input1path.empty() || outputpath.empty())
    {
        print_usage();
        return -1;
    }

#if _WIN32
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

    ncnn::Mat in0image;
    ncnn::Mat in1image;
    load(input0path, in0image);
    load(input1path, in1image);

    if (in0image.empty() || in1image.empty())
    {
        fprintf(stderr, "decode image failed\n");
        return -1;
    }

    ncnn::Mat outimage(in0image.w, in0image.h, (size_t)3, 3);

    ncnn::create_gpu_instance();

    {
        DAIN* dain = new DAIN();

        dain->load();

        dain->process(in0image, in1image, outimage);

        delete dain;
    }

    save(outputpath, outimage);

    ncnn::destroy_gpu_instance();

    return 0;
}
