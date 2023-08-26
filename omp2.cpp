#include <stdio.h> 
#include <malloc.h>
#include <omp.h>
#include <math.h>

class Image {
public:
    unsigned char format[2];
    unsigned int width, height, d;
    unsigned char* data;
    unsigned int dataSize;
    bool readImage(FILE* file);
    void writeImage(FILE* file);
    void addСontrastImage(float ignoredPexe, int numThreads);
private:
    int* getBarChart(unsigned int pos, unsigned int count, int numThreads);
    void getMax(unsigned int countIgnoredPixels, int& max, unsigned int pos, unsigned int count, int* brightPixels);
    void getMin(unsigned int countIgnoredPixels, int& min, unsigned int pos, unsigned int count, int* brightPixels);
};

bool Image::readImage(FILE* file) {

    fread(format, sizeof(unsigned char), 2, file);

    if (!(fscanf(file, "%u%u%u", &width, &height, &d))) {
        fprintf(stderr, "Invalid value.\n");
        return false;
    }

    if (format[1] == '5') dataSize = width * height;
    else if (format[1] == '6') dataSize = width * height * 3;
    else {
        fprintf(stderr, "Invalid format.\n");
    }

    if (!(data = (unsigned char*)malloc(dataSize * sizeof(unsigned char)))) {
        fprintf(stderr, "Failed to allocate mamory.\n");
        return false;
    }
    fgetc(file);

    if (fread(data, sizeof(unsigned char), dataSize, file) != dataSize) {
        fprintf(stderr, "Invalid count of element.\n");
        return false;
    }

    return true;
}

void Image::writeImage(FILE* file) {
    
    fwrite(format, sizeof(unsigned char), 2, file);
    fprintf(file, "\n%u %u\n%u\n", width, height, d);
    fwrite(data, sizeof(unsigned char), dataSize, file);
}

/*Построение гистограммы яркости изображения*/
int* Image::getBarChart(unsigned int pos, unsigned int count, int numThreads) {
    
    int* brightPixels = (int*)calloc(256, sizeof(int));

#pragma omp parallel num_threads(numThreads) if(numThreads>1)
    {
        int* bp = (int*)calloc(256, sizeof(int));

#pragma omp  for schedule(static)
        for (unsigned int i = pos; i < dataSize; i += count)
            bp[data[i]]++;

        for (unsigned int i = 0; i < 256; i++)
        {
#pragma omp atomic
            brightPixels[i] += bp[i];
        }
    }

    return brightPixels;
}

void Image::getMax(unsigned int countIgnoredPixels, int& max, unsigned int pos, unsigned int count, int* brightPixels) {

    for (unsigned int i = 255; i >= 0; i--) {
        if (brightPixels[i] > 0) {
            max = i;
            if (countIgnoredPixels == 0)break;
            else if (brightPixels[i] > countIgnoredPixels) break;
            else countIgnoredPixels -= brightPixels[i];
        }
    }
}

void Image::getMin(unsigned int countIgnoredPixels, int& min, unsigned int pos, unsigned int count, int* brightPixels) {

    for (unsigned int i = 0; i < 256; i++) {
        if (brightPixels[i] > 0) {
            min = i;
            if (countIgnoredPixels == 0)break;
            else if (brightPixels[i] > countIgnoredPixels) break;
            else countIgnoredPixels -= brightPixels[i];
        }
    }
}

void Image::addСontrastImage(float ignoredPexel, int numThreads)
{
    if (format[1] == '5') {

        unsigned int countIgnoredPixels = (dataSize * ignoredPexel); //пиксели для игнорирования
        int min = 256, max = 0;
        int* brightPixels = (int*)malloc(256 * sizeof(int));

        brightPixels = getBarChart(0, 1, numThreads);

#pragma omp parallel num_threads(2) if(numThreads > 1) 
        {
#pragma omp sections 
            {
#pragma omp section
                {
                    getMax(countIgnoredPixels, max, 0, 1, brightPixels);
                }
#pragma omp section
                {
                    getMin(countIgnoredPixels, min, 0, 1, brightPixels);
                }
            }
        }
        free(brightPixels);
        
        if (min == max) return; //не меняем изображение если оно сотоит из одного значения

#pragma omp parallel for schedule(static) num_threads(numThreads) if(numThreads>1)
        for (unsigned int i = 0; i < dataSize; i++) {
            if (data[i] < min) data[i] = 0;
            else if (data[i] > max) data[i] = 255;
            else {
                data[i] = (data[i] - min) * 255 / (max - min);
                if (data[i] < 0) data[i] = 0;
                if (data[i] > 255)data[i] = 255;
            }
        }
    }
    else {
        unsigned int countIgnoredPixels = (dataSize/3 * ignoredPexel);
        int minR = 0, minG = 0, minB = 0;
        int maxR = 255, maxG = 255, maxB = 255;
        int* brightPixelsR = (int*)malloc(256 * sizeof(int));
        int* brightPixelsG = (int*)malloc(256 * sizeof(int));
        int* brightPixelsB = (int*)malloc(256 * sizeof(int));


        brightPixelsR = getBarChart(0, 3, numThreads);
        brightPixelsG = getBarChart(1, 3, numThreads);
        brightPixelsB = getBarChart(2, 3, numThreads);

#pragma omp parallel num_threads(numThreads) if(numThreads > 1)
        {
#pragma omp sections         
            {
#pragma omp section
                {
                    getMin(countIgnoredPixels, minR, 0, 3, brightPixelsR);
                }
#pragma omp section
                {
                    getMin(countIgnoredPixels, minG, 1, 3, brightPixelsG);
                }
#pragma omp section
                {
                    getMin(countIgnoredPixels, minB, 2, 3, brightPixelsB);
                }
#pragma omp section
                {
                    getMax(countIgnoredPixels, maxR, 0, 3, brightPixelsR);
                }
#pragma omp section
                {
                    getMax(countIgnoredPixels, maxG, 1, 3, brightPixelsG);
                }
#pragma omp section
                {
                    getMax(countIgnoredPixels, maxB, 2, 3, brightPixelsB);
                }
            }
        }
        free(brightPixelsR);
        free(brightPixelsG);
        free(brightPixelsB);

        if (minR == maxR && minG == maxG && minB == maxB)return;
        int min = minR;
        int max = maxR;

        if (minG < min) min = minG;
        if (minB < min) min = minB;
        if (maxG > max) max = maxG;
        if (maxB > max) max = maxB;

#pragma omp parallel for schedule(static) num_threads(numThreads) if(numThreads>1)
        for (unsigned int i = 0; i < dataSize; i++) {

            if (data[i] < min) data[i] = 0;

            else if (data[i] > max) data[i] = 255;

            else {
                data[i] = (data[i] - min) * 255 / (max - min);
                if (data[i] < 0) data[i] = 0;
                if (data[i] > 255)data[i] = 255;
            }
        }
    }
}

//<имя_входного_файла> <имя_выходного_файла> <кол - во потоков> < коэффициент(float, [0.0, 0.5)) >
int main(int argc, char* argv[])
{
    /* проверка количества поданых аргументов*/
    if (argc != 5) {
        fprintf(stderr, "Incorrect number of arguments.\n");
        return 1;
    }
    int numThreads = atoi(argv[3]);
    float ignoredPixel = atof(argv[4]);

    /* проверка допустимого значения количества тредов и количества пикселей для игнорирования*/
    if (numThreads > omp_get_max_threads() || numThreads < -1) {
        fprintf(stderr, "Invalid number of threads\n");
        return 1;
    }

    if (ignoredPixel < 0.0f || ignoredPixel >= 0.5f) {
        fprintf(stderr, "Invalid coefficient value\n");
        return 1;
    }

    if (numThreads == 0) numThreads = omp_get_max_threads();
    if (numThreads == -1) numThreads = 1;

    FILE* file, * outfile;

    /* проверка открытия файла*/
    if ((file = fopen(argv[1], "rb"))) {

        if (!(outfile = fopen(argv[2], "wb"))) {
            fclose(file);
            fprintf(stderr, "Failed to open out file.\n");
            return 1;
        }

        Image img;
        if (!img.readImage(file)) {
            fclose(file);
            fclose(outfile);
            return 1;
        }

        double start = omp_get_wtime();
        img.addСontrastImage(ignoredPixel, numThreads);
        double end = omp_get_wtime();

        printf("Time (%i thread(s)): %g ms\n", numThreads, (end - start) * 1000);
        img.writeImage(outfile);
        free(img.data);
        fclose(file);
        fclose(outfile);
        return 0;
    }
    else {
        fprintf(stderr, "Failed to open input file.\n");
        return 1;
    }
}