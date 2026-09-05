int buildRegionLookup()
{
    if (!loadedMapImage || !mapGraph) return 0;

    const int regionCount = mapGraph->region_count;
    const int width = (int)loadedMapImage->width;
    const int height = loadedMapImage->height;
    if (width <= 0 || height <= 0) return 0;

    if ((size_t)width > SIZE_MAX / (size_t)height) return 0;
    size_t pixelCountSize = (size_t)width * (size_t)height;
    if (pixelCountSize > (size_t)INT_MAX) return 0;
    const int pixelCount = (int)pixelCountSize;

    freeRegionLookup();
    regionLookupCount = regionCount;

    regionPixelCounts = (int*)calloc(regionCount, sizeof(int));
    regionPixels = (int**)calloc(regionCount, sizeof(int*));
    if (!regionPixelCounts || !regionPixels)
    {
        freeRegionLookup();
        return 0;
    }

    for (int i = 0; i < pixelCount; i++)
    {
        int regionId = mapGraph->region_map[i];
        if (regionId >= 0 && regionId < regionCount) regionPixelCounts[regionId]++;
    }

    for (int regionId = 0; regionId < regionCount; regionId++)
    {
        if (regionPixelCounts[regionId] > 0)
        {
            regionPixels[regionId] = (int*)malloc((size_t)regionPixelCounts[regionId] * sizeof(int));
            if (!regionPixels[regionId])
            {
                freeRegionLookup();
                return 0;
            }
        }
    }

    int* cursor = (int*)calloc(regionCount, sizeof(int));
    if (!cursor)
    {
        freeRegionLookup();
        return 0;
    }

    for (int i = 0; i < pixelCount; i++)
    {
        int regionId = mapGraph->region_map[i];
        if (regionId >= 0 && regionId < regionCount)
        {
            int writeIndex = cursor[regionId]++;
            int x = i % width;
            int y = i / width;
            int textureIndex = (height - 1 - y) * width + x;
            regionPixels[regionId][writeIndex] = textureIndex;
        }
    }

    free(cursor);
    return 1;
}

void startColoringAnimation()
{
    if (!loadedMapImage || !mapRgbaBuffer) return;

    freeColoringData();

    mapGraph = build_graph(loadedMapImage);
    if (!mapGraph)
    {
        logGraphBuildFailureDetails();
        return;
    }
    logGraphBuildStats(mapGraph);

    double algStart = glfwGetTime();
    mapColors = dsatur(mapGraph);
    algorithmTime = glfwGetTime() - algStart;

    if (!mapColors)
    {
        logMessage("Coloring failed (DSATUR, exceeded 5-color limit or inconsistent graph)\n");
        freeColoringData();
        return;
    }

    if (mapGraph->region_count <= 0)
    {
        logMessage("No regions found\n");
        freeColoringData();
        return;
    }

    if (!buildRegionLookup())
    {
        logMessage("Region lookup failed\n");
        freeColoringData();
        return;
    }

    fillRgbaFromImage(loadedMapImage, mapRgbaBuffer);
    glBindTexture(GL_TEXTURE_2D, mapTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, loadedMapImage->width, loadedMapImage->height, GL_RGBA, GL_UNSIGNED_BYTE, mapRgbaBuffer);

    animationRegionIndex = 0;
    animationInProgress = 1;
    nextAnimationTick = glfwGetTime();
    lastProgressPercent = -1;
    progressBarVisible = 1;
    progressBarDone = 0;
    reportProgressPercent(0);
    logMessage("Algorithm: DSATUR\n");
    totalStartTime = glfwGetTime();
}

void updateColoringAnimation()
{
    if (!animationInProgress || !loadedMapImage || !mapGraph || !mapColors || !mapRgbaBuffer || !regionPixels || !regionPixelCounts)
        return;

    double now = glfwGetTime();
    if (now < nextAnimationTick) return;
    nextAnimationTick = now + ANIMATION_STEP_DELAY;

    for (int step = 0; step < ANIMATION_REGIONS_PER_STEP && animationRegionIndex < mapGraph->region_count; step++)
    {
        int regionId = animationRegionIndex++;
        Pixel fillColor = colorByIndex(mapColors[regionId]);

        for (int i = 0; i < regionPixelCounts[regionId]; i++)
        {
            int pixelIndex = regionPixels[regionId][i];
            int base = pixelIndex * 4;
            mapRgbaBuffer[base + 0] = fillColor.r;
            mapRgbaBuffer[base + 1] = fillColor.g;
            mapRgbaBuffer[base + 2] = fillColor.b;
            mapRgbaBuffer[base + 3] = 255;
        }
    }

    glBindTexture(GL_TEXTURE_2D, mapTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, loadedMapImage->width, loadedMapImage->height, GL_RGBA, GL_UNSIGNED_BYTE, mapRgbaBuffer);

    if (mapGraph->region_count > 0)
    {
        int progress = (animationRegionIndex * 100) / mapGraph->region_count;
        reportProgressPercent(progress);
    }

    if (animationRegionIndex >= mapGraph->region_count)
    {
        animationInProgress = 0;

        int maxColor = 0;
        for (int i = 0; i < mapGraph->region_count; i++)
            if (mapColors[i] > maxColor) maxColor = mapColors[i];

        double totalTime = glfwGetTime() - totalStartTime;
        progressBarDone = 1;
        reportProgressPercent(100);
        logMessage("=== Coloring finished ===\n");
        logMessage("Algorithm: DSATUR\n");
        logMessage("Colors used: %d\n", maxColor);
        logMessage("Total time: %.3f sec\n", totalTime);
        logMessage("Algorithm time: %.3f sec\n", algorithmTime);
        saveColoredMapToProject();
    }
}

void drawOutlinedText(HDC dc, const wchar_t* text, RECT rect)
{
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    DrawTextW(dc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

HFONT createUiFont(int fontSize)
{
    HFONT font = CreateFontW(-fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_SWISS, L"Segoe UI");
    if (!font)
    {
        font = CreateFontW(-fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, FF_SWISS, L"Arial");
    }
    return font;
}

unsigned int createTextButtonTexture(const wchar_t* text, int width, int height, int fontSize)
{
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) return 0;

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pixels = NULL;
    HBITMAP dib = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (!dib || !pixels)
    {
        if (dib) DeleteObject(dib);
        DeleteDC(dc);
        return 0;
    }

    HBITMAP oldBitmap = (HBITMAP)SelectObject(dc, dib);

    RECT rect = { 0, 0, width, height };
    HBRUSH fillBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(dc, &rect, fillBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
    HPEN oldPen = (HPEN)SelectObject(dc, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, 1, 1, width - 1, height - 1);

    HFONT font = createUiFont(fontSize);
    BOOL deleteCreatedFont = TRUE;
    if (!font)
    {
        font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        deleteCreatedFont = FALSE;
    }
    HFONT oldFont = (HFONT)SelectObject(dc, font);
    drawOutlinedText(dc, text, rect);

    if (oldFont) SelectObject(dc, oldFont);
    if (deleteCreatedFont && font) DeleteObject(font);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(borderPen);
    DeleteObject(fillBrush);

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);

    SelectObject(dc, oldBitmap);
    DeleteObject(dib);
    DeleteDC(dc);
    return texture;
}

void printCliHelp(const char* exeName)
{
    const char* name = (exeName && exeName[0]) ? exeName : "map_reading";
    logMessage("Usage:\n");
    logMessage("  %s INPUT.bmp OUTPUT.bmp\n", name);
    logMessage("  %s --help\n", name);
    logMessage("\n");
    logMessage("CLI output:\n");
    logMessage("- Progress in %%\n");
    logMessage("- Colors count\n");
    logMessage("- Total time\n");
    logMessage("- Coloring algorithm time\n");
}

int runCommandLineMode(const char* inputPath, const char* outputPath)
{
    if (!inputPath || !outputPath) return 1;

    logMessage("CLI mode: input=%s output=%s\n", inputPath, outputPath);
    lastProgressPercent = -1;
    reportProgressPercent(0);

    clock_t totalStart = clock();
    Image* inputImage = read_bmp(inputPath);
    if (!inputImage)
    {
        logMessage("Cannot read input file: %s\n", inputPath);
        return 1;
    }

    Graph* graph = build_graph(inputImage);
    if (!graph)
    {
        logGraphBuildFailureDetails();
        free_image(inputImage);
        return 1;
    }
    logGraphBuildStats(graph);

    clock_t algStart = clock();
    int* colors = dsatur(graph);
    double cliAlgorithmTime = (double)(clock() - algStart) / (double)CLOCKS_PER_SEC;
    if (!colors)
    {
        logMessage("Coloring failed (DSATUR, exceeded 5-color limit or inconsistent graph)\n");
        free_graph(graph);
        free_image(inputImage);
        return 1;
    }

    const int width = (int)inputImage->width;
    const int height = inputImage->height;
    if (width <= 0 || height <= 0) {
        logMessage("Invalid input image dimensions\n");
        free(colors);
        free_graph(graph);
        free_image(inputImage);
        return 1;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        logMessage("Input image is too large\n");
        free(colors);
        free_graph(graph);
        free_image(inputImage);
        return 1;
    }
    const size_t pixelCount = (size_t)width * (size_t)height;
    if (pixelCount > SIZE_MAX / sizeof(Pixel)) {
        logMessage("Input image is too large for output buffer\n");
        free(colors);
        free_graph(graph);
        free_image(inputImage);
        return 1;
    }
    if (pixelCount > (size_t)INT_MAX) {
        logMessage("Input image is too large for processing\n");
        free(colors);
        free_graph(graph);
        free_image(inputImage);
        return 1;
    }
    const int pixelCountInt = (int)pixelCount;

    Pixel* outputPixels = (Pixel*)malloc(pixelCount * sizeof(Pixel));
    if (!outputPixels)
    {
        logMessage("Not enough memory for output image\n");
        free(colors);
        free_graph(graph);
        free_image(inputImage);
        return 1;
    }

    memcpy(outputPixels, inputImage->data, pixelCount * sizeof(Pixel));

    int maxColor = 0;
    for (int i = 0; i < graph->region_count; i++)
        if (colors[i] > maxColor) maxColor = colors[i];

    int lastCliProgress = -1;
    for (int i = 0; i < pixelCountInt; i++)
    {
        int regionId = graph->region_map[i];
        if (regionId >= 0 && regionId < graph->region_count)
            outputPixels[i] = colorByIndex(colors[regionId]);

        int progress = (i * 100) / (pixelCountInt > 0 ? pixelCountInt : 1);
        if (progress != lastCliProgress)
        {
            lastCliProgress = progress;
            reportProgressPercent(progress);
        }
    }

    Image outputImage;
    outputImage.width = inputImage->width;
    outputImage.height = inputImage->height;
    outputImage.data = outputPixels;

    int saved = write_bmp(outputPath, &outputImage);
    reportProgressPercent(100);

    double totalTime = (double)(clock() - totalStart) / (double)CLOCKS_PER_SEC;
    logMessage("=== Coloring finished ===\n");
    logMessage("Algorithm: DSATUR\n");
    logMessage("Colors used: %d\n", maxColor);
    logMessage("Total time: %.3f sec\n", totalTime);
    logMessage("Algorithm time: %.3f sec\n", cliAlgorithmTime);
    logMessage("Output saved: %s\n", saved ? outputPath : "(save failed)");

    free(outputPixels);
    free(colors);
    free_graph(graph);
    free_image(inputImage);
    return saved ? 0 : 1;
}
