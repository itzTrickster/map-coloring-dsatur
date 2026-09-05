Pixel colorByIndex(int colorIndex)
{
    switch (colorIndex)
    {
    case 1:  return (Pixel) { 255, 212, 140 };
    case 2:  return (Pixel) { 142, 214, 158 };
    case 3:  return (Pixel) { 149, 194, 255 };
    case 4:  return (Pixel) { 250, 176, 255 };
    case 5:  return (Pixel) { 255, 239, 135 };
    default: return (Pixel) { 255, 180, 140 };
    }
}

const char* getFileNamePart(const char* path)
{
    if (!path || path[0] == '\0') return "map";

    const char* slash1 = strrchr(path, '\\');
    const char* slash2 = strrchr(path, '/');
    const char* slash = slash1;
    if (!slash || (slash2 && slash2 > slash)) slash = slash2;

    return slash ? (slash + 1) : path;
}

void stripExtension(char* name)
{
    if (!name) return;

    char* dot = strrchr(name, '.');
    if (dot) *dot = '\0';
}

int resolveProjectRoot(char* projectRoot, size_t projectRootSize)
{
    if (!projectRoot || projectRootSize == 0) return 0;

    char exePath[MAX_PATH];
    DWORD exePathLen = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (exePathLen == 0 || exePathLen >= MAX_PATH)
    {
        DWORD cwdLen = GetCurrentDirectoryA((DWORD)projectRootSize, projectRoot);
        return cwdLen > 0 && cwdLen < projectRootSize;
    }

    exePath[MAX_PATH - 1] = '\0';
    char* slash1 = strrchr(exePath, '\\');
    char* slash2 = strrchr(exePath, '/');
    char* slash = slash1;
    if (!slash || (slash2 && slash2 > slash)) slash = slash2;
    if (!slash) return 0;
    *slash = '\0';

    for (int level = 0; level < 12; level++)
    {
        char testPath[MAX_PATH];
        int testPathLen = snprintf(testPath, sizeof(testPath), "%s\\map_reading.sln", exePath);
        if (testPathLen > 0 && testPathLen < (int)sizeof(testPath))
        {
            DWORD attrs = GetFileAttributesA(testPath);
            if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY))
            {
                int outLen = snprintf(projectRoot, projectRootSize, "%s", exePath);
                return outLen > 0 && outLen < (int)projectRootSize;
            }
        }

        slash1 = strrchr(exePath, '\\');
        slash2 = strrchr(exePath, '/');
        slash = slash1;
        if (!slash || (slash2 && slash2 > slash)) slash = slash2;
        if (!slash) break;
        *slash = '\0';
    }

    DWORD cwdLen = GetCurrentDirectoryA((DWORD)projectRootSize, projectRoot);
    return cwdLen > 0 && cwdLen < projectRootSize;
}

int ensureColoredMapsDirectory(char* outputDir, size_t outputDirSize)
{
    char projectRoot[MAX_PATH];
    if (!resolveProjectRoot(projectRoot, sizeof(projectRoot))) return 0;

    int outputDirLen = snprintf(outputDir, outputDirSize, "%s\\colored_maps", projectRoot);
    if (outputDirLen <= 0 || outputDirLen >= (int)outputDirSize) return 0;

    if (!CreateDirectoryA(outputDir, NULL))
    {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) return 0;
    }

    return 1;
}

int saveColoredMapToProject()
{
    if (!loadedMapImage || !mapRgbaBuffer) return 0;

    char outputDir[MAX_PATH];
    if (!ensureColoredMapsDirectory(outputDir, sizeof(outputDir)))
    {
        logMessage("Failed to prepare output directory for colored maps\n");
        return 0;
    }

    char mapName[128];
    int mapNameLen = snprintf(mapName, sizeof(mapName), "%s", getFileNamePart(selectedMap));
    if (mapNameLen <= 0 || mapNameLen >= (int)sizeof(mapName)) snprintf(mapName, sizeof(mapName), "map");
    stripExtension(mapName);
    if (mapName[0] == '\0') snprintf(mapName, sizeof(mapName), "map");

    SYSTEMTIME timestamp;
    GetLocalTime(&timestamp);

    char outputPath[MAX_PATH];
    int outputPathLen = snprintf(
        outputPath, sizeof(outputPath),
        "%s\\%s_colored_%04u%02u%02u_%02u%02u%02u.bmp",
        outputDir, mapName,
        (unsigned)timestamp.wYear, (unsigned)timestamp.wMonth, (unsigned)timestamp.wDay,
        (unsigned)timestamp.wHour, (unsigned)timestamp.wMinute, (unsigned)timestamp.wSecond);

    if (outputPathLen <= 0 || outputPathLen >= (int)sizeof(outputPath))
    {
        logMessage("Output path is too long, cannot save map\n");
        return 0;
    }

    const int width = (int)loadedMapImage->width;
    const int height = loadedMapImage->height;
    if (width <= 0 || height <= 0) return 0;
    if ((size_t)width > SIZE_MAX / (size_t)height) return 0;
    const size_t pixelCount = (size_t)width * (size_t)height;
    if (pixelCount > SIZE_MAX / sizeof(Pixel)) return 0;

    Pixel* pixels = (Pixel*)malloc(pixelCount * sizeof(Pixel));
    if (!pixels)
    {
        logMessage("Not enough memory to save colored map\n");
        return 0;
    }

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int imageIndex = y * width + x;
            int textureIndex = (height - 1 - y) * width + x;
            int base = textureIndex * 4;

            pixels[imageIndex].r = mapRgbaBuffer[base + 0];
            pixels[imageIndex].g = mapRgbaBuffer[base + 1];
            pixels[imageIndex].b = mapRgbaBuffer[base + 2];
        }
    }

    Image outputImage;
    outputImage.width = loadedMapImage->width;
    outputImage.height = loadedMapImage->height;
    outputImage.data = pixels;

    int saved = write_bmp(outputPath, &outputImage);
    free(pixels);

    if (saved) logMessage("Colored map saved: %s\n", outputPath);
    else logMessage("Failed to save colored map: %s\n", outputPath);

    return saved;
}

void freeRegionLookup()
{
    if (regionPixels)
    {
        for (int i = 0; i < regionLookupCount; i++)
            free(regionPixels[i]);
        free(regionPixels);
        regionPixels = NULL;
    }

    if (regionPixelCounts)
    {
        free(regionPixelCounts);
        regionPixelCounts = NULL;
    }

    regionLookupCount = 0;
}

void freeColoringData()
{
    freeRegionLookup();

    if (mapColors)
    {
        free(mapColors);
        mapColors = NULL;
    }

    if (mapGraph)
    {
        free_graph(mapGraph);
        mapGraph = NULL;
    }

    animationRegionIndex = 0;
    animationInProgress = 0;
    progressBarVisible = 0;
    progressBarDone = 0;
}

void freeLoadedMapData()
{
    freeColoringData();

    if (mapRgbaBuffer)
    {
        free(mapRgbaBuffer);
        mapRgbaBuffer = NULL;
    }

    if (loadedMapImage)
    {
        free_image(loadedMapImage);
        loadedMapImage = NULL;
    }
}

void fillRgbaFromImage(const Image* image, unsigned char* rgbaBuffer)
{
    if (!image || !rgbaBuffer) return;

    const int width = image->width;
    const int height = image->height;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int srcIndex = y * width + x;
            int dstIndex = (height - 1 - y) * width + x;

            rgbaBuffer[dstIndex * 4 + 0] = image->data[srcIndex].r;
            rgbaBuffer[dstIndex * 4 + 1] = image->data[srcIndex].g;
            rgbaBuffer[dstIndex * 4 + 2] = image->data[srcIndex].b;
            rgbaBuffer[dstIndex * 4 + 3] = 255;
        }
    }
}

int rebuildMapTextureFromLoadedImage()
{
    if (!loadedMapImage) return 0;

    const int width = (int)loadedMapImage->width;
    const int height = loadedMapImage->height;
    if (width <= 0 || height <= 0) return 0;

    if ((size_t)width > SIZE_MAX / (size_t)height) return 0;
    const size_t pixelCount = (size_t)width * (size_t)height;
    if (pixelCount > SIZE_MAX / 4u) return 0;
    const size_t rgbaSize = (size_t)pixelCount * 4;

    if (mapRgbaBuffer)
    {
        free(mapRgbaBuffer);
        mapRgbaBuffer = NULL;
    }

    mapRgbaBuffer = (unsigned char*)malloc(rgbaSize);
    if (!mapRgbaBuffer) return 0;

    fillRgbaFromImage(loadedMapImage, mapRgbaBuffer);

    if (mapTexture)
    {
        glDeleteTextures(1, &mapTexture);
    }

    glGenTextures(1, &mapTexture);
    glBindTexture(GL_TEXTURE_2D, mapTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, mapRgbaBuffer);
    return 1;
}
