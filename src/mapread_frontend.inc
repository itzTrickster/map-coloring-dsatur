unsigned int createCloseButtonTexture(int size)
{
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) return 0;

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = size;
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
    RECT rect = { 0, 0, size, size };
    HBRUSH fillBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(dc, &rect, fillBrush);

    HPEN borderPen = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
    HPEN oldPen = (HPEN)SelectObject(dc, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, 1, 1, size - 1, size - 1);

    HPEN crossPen = CreatePen(PS_SOLID, 8, RGB(220, 0, 0));
    HPEN restorePen = (HPEN)SelectObject(dc, crossPen);
    const int margin = size / 4;
    MoveToEx(dc, margin, margin, NULL);
    LineTo(dc, size - margin, size - margin);
    MoveToEx(dc, size - margin, margin, NULL);
    LineTo(dc, margin, size - margin);

    SelectObject(dc, restorePen);
    DeleteObject(crossPen);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);

    SelectObject(dc, oldBitmap);
    DeleteObject(dib);
    DeleteDC(dc);
    return texture;
}

void openFileDialog()
{
    OPENFILENAME ofn;
    char fileName[MAX_PATH] = "";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Bitmap Files (*.bmp)\0*.bmp\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn))
    {
        Image* newImage = read_bmp(fileName);
        if (!newImage)
        {
            logMessage("Failed to load image: %s\n", fileName);
            return;
        }

        freeLoadedMapData();
        loadedMapImage = newImage;
        strcpy(selectedMap, fileName);
        saveAppState();

        if (!rebuildMapTextureFromLoadedImage())
        {
            logMessage("Failed to build texture for: %s\n", selectedMap);
            freeLoadedMapData();
            selectedMap[0] = '\0';
            return;
        }

        resetMapView();
        screenState = SCREEN_MAP;
    }
}

int main(int argc, char** argv)
{
    openLog();
    loadAppState();

    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        printCliHelp(argv[0]);
        closeLog();
        return 0;
    }

    if (argc == 3)
    {
        int code = runCommandLineMode(argv[1], argv[2]);
        closeLog();
        return code;
    }

    if (argc > 1)
    {
        printCliHelp(argv[0]);
        closeLog();
        return 1;
    }

    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1200, 800, "map-coloring", NULL, NULL);
    mainWindow = window;
    if (!window)
    {
        glfwTerminate();
        closeLog();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glewInit();
    updateWindowTitleByProgress(-1);
    resetMapView();

    float mapVertices[] = {
        -0.9f, -0.7f, 0.0f, 0.0f, 0.0f,
         0.65f, -0.7f, 0.0f, 1.0f, 0.0f,
         0.65f,  0.7f, 0.0f, 1.0f, 1.0f,
        -0.9f,  0.7f, 0.0f, 0.0f, 1.0f
    };

    float loadButtonVertices[] = {
        -0.5f, -0.25f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.25f, 0.0f, 1.0f, 0.0f,
         0.5f,  0.25f, 0.0f, 1.0f, 1.0f,
        -0.5f,  0.25f, 0.0f, 0.0f, 1.0f
    };

    float aboutButtonVertices[] = {
        -0.5f, -0.56f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.56f, 0.0f, 1.0f, 0.0f,
         0.5f, -0.34f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.34f, 0.0f, 0.0f, 1.0f
    };

    float helpButtonVertices[] = {
        -0.5f, -0.86f, 0.0f, 0.0f, 0.0f,
         0.5f, -0.86f, 0.0f, 1.0f, 0.0f,
         0.5f, -0.64f, 0.0f, 1.0f, 1.0f,
        -0.5f, -0.64f, 0.0f, 0.0f, 1.0f
    };

    float colorButtonVertices[] = {
         0.70f, -0.2f, 0.0f, 0.0f, 0.0f,
         0.98f, -0.2f, 0.0f, 1.0f, 0.0f,
         0.98f,  0.2f, 0.0f, 1.0f, 1.0f,
         0.70f,  0.2f, 0.0f, 0.0f, 1.0f
    };

    float closeButtonVertices[] = {
         0.86f, 0.78f, 0.0f, 0.0f, 0.0f,
         0.98f, 0.78f, 0.0f, 1.0f, 0.0f,
         0.98f, 0.98f, 0.0f, 1.0f, 1.0f,
         0.86f, 0.98f, 0.0f, 0.0f, 1.0f
    };

    float progressBarVertices[] = {
        -0.9f,  -0.98f, 0.0f, 0.0f, 0.0f,
         0.65f, -0.98f, 0.0f, 1.0f, 0.0f,
         0.65f, -0.76f, 0.0f, 1.0f, 1.0f,
        -0.9f,  -0.76f, 0.0f, 0.0f, 1.0f
    };

    unsigned int mapVAO, mapVBO;
    unsigned int loadButtonVAO, loadButtonVBO;
    unsigned int aboutButtonVAO, aboutButtonVBO;
    unsigned int helpButtonVAO, helpButtonVBO;
    unsigned int colorButtonVAO, colorButtonVBO;
    unsigned int closeButtonVAO, closeButtonVBO;
    unsigned int progressBarVAO, progressBarVBO;

    glGenVertexArrays(1, &mapVAO);
    glGenBuffers(1, &mapVBO);
    glBindVertexArray(mapVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mapVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mapVertices), mapVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &loadButtonVAO);
    glGenBuffers(1, &loadButtonVBO);
    glBindVertexArray(loadButtonVAO);
    glBindBuffer(GL_ARRAY_BUFFER, loadButtonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(loadButtonVertices), loadButtonVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &aboutButtonVAO);
    glGenBuffers(1, &aboutButtonVBO);
    glBindVertexArray(aboutButtonVAO);
    glBindBuffer(GL_ARRAY_BUFFER, aboutButtonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(aboutButtonVertices), aboutButtonVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &helpButtonVAO);
    glGenBuffers(1, &helpButtonVBO);
    glBindVertexArray(helpButtonVAO);
    glBindBuffer(GL_ARRAY_BUFFER, helpButtonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(helpButtonVertices), helpButtonVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &colorButtonVAO);
    glGenBuffers(1, &colorButtonVBO);
    glBindVertexArray(colorButtonVAO);
    glBindBuffer(GL_ARRAY_BUFFER, colorButtonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(colorButtonVertices), colorButtonVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &closeButtonVAO);
    glGenBuffers(1, &closeButtonVBO);
    glBindVertexArray(closeButtonVAO);
    glBindBuffer(GL_ARRAY_BUFFER, closeButtonVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(closeButtonVertices), closeButtonVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &progressBarVAO);
    glGenBuffers(1, &progressBarVBO);
    glBindVertexArray(progressBarVAO);
    glBindBuffer(GL_ARRAY_BUFFER, progressBarVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(progressBarVertices), progressBarVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    loadButtonTexture = createTextButtonTexture(LOAD_BUTTON_TEXT, 1024, 512, 96);
    aboutButtonTexture = createTextButtonTexture(ABOUT_BUTTON_TEXT, 1024, 240, 72);
    helpButtonTexture = createTextButtonTexture(HELP_BUTTON_TEXT, 1024, 240, 72);
    colorButtonTexture = createTextButtonTexture(COLOR_BUTTON_TEXT, 640, 320, 64);
    closeButtonTexture = createCloseButtonTexture(256);

    const char* vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "out vec2 TexCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "}\n";

    const char* fragmentShaderSource =
        "#version 330 core\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D texture1;\n"
        "uniform vec2 uvOffset;\n"
        "uniform float uvZoom;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        " vec2 uv = (TexCoord - vec2(0.5, 0.5)) / uvZoom + vec2(0.5, 0.5) + uvOffset;\n"
        " if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)\n"
        "    FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
        " else\n"
        "    FragColor = texture(texture1, uv);\n"
        "}\n";

    unsigned int VertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(VertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(VertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, fragmentShader);
    glAttachShader(shaderProgram, VertexShader);
    glLinkProgram(shaderProgram);
    glUseProgram(shaderProgram);
    glDeleteShader(fragmentShader);

    int uvOffsetLocation = glGetUniformLocation(shaderProgram, "uvOffset");
    int uvZoomLocation = glGetUniformLocation(shaderProgram, "uvZoom");
    glUniform2f(uvOffsetLocation, 0.0f, 0.0f);
    glUniform1f(uvZoomLocation, 1.0f);

    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth < 1) framebufferWidth = 1;
    if (framebufferHeight < 1) framebufferHeight = 1;
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    while (!glfwWindowShouldClose(window))
    {
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth < 1) framebufferWidth = 1;
        if (framebufferHeight < 1) framebufferHeight = 1;
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glUniform2f(uvOffsetLocation, 0.0f, 0.0f);
        glUniform1f(uvZoomLocation, 1.0f);

        if (screenState == SCREEN_MENU)
        {
            glBindVertexArray(loadButtonVAO);
            glBindTexture(GL_TEXTURE_2D, loadButtonTexture);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            glBindVertexArray(aboutButtonVAO);
            glBindTexture(GL_TEXTURE_2D, aboutButtonTexture);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            glBindVertexArray(helpButtonVAO);
            glBindTexture(GL_TEXTURE_2D, helpButtonTexture);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }

        if (screenState == SCREEN_MAP)
        {
            updateColoringAnimation();

            glUniform2f(uvOffsetLocation, mapUvOffsetX, mapUvOffsetY);
            glUniform1f(uvZoomLocation, mapUvZoom);
            glBindVertexArray(mapVAO);
            glBindTexture(GL_TEXTURE_2D, mapTexture);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            glUniform2f(uvOffsetLocation, 0.0f, 0.0f);
            glUniform1f(uvZoomLocation, 1.0f);
            glBindVertexArray(colorButtonVAO);
            glBindTexture(GL_TEXTURE_2D, colorButtonTexture);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            glBindVertexArray(closeButtonVAO);
            glBindTexture(GL_TEXTURE_2D, closeButtonTexture);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

            if (progressBarVisible && progressBarTexture)
            {
                glBindVertexArray(progressBarVAO);
                glBindTexture(GL_TEXTURE_2D, progressBarTexture);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
        }

        glfwSwapBuffers(window);

        double mouseX, mouseY;
        int windowWidth, windowHeight;
        glfwPollEvents();
        glfwGetCursorPos(window, &mouseX, &mouseY);
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        if (windowWidth < 1) windowWidth = 1;
        if (windowHeight < 1) windowHeight = 1;

        if (screenState == SCREEN_MAP)
        {
            float panStep = 0.01f / mapUvZoom;

            if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
                zoomMapView(MAP_ZOOM_STEP);
            if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
                zoomMapView(1.0f / MAP_ZOOM_STEP);

            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                panMapView(-panStep, 0.0f);
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                panMapView(panStep, 0.0f);
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                panMapView(0.0f, panStep);
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                panMapView(0.0f, -panStep);

            if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
                resetMapView();
        }

        float x = (float)((mouseX / (double)windowWidth) * 2.0 - 1.0);
        float y = (float)(1.0 - (mouseY / (double)windowHeight) * 2.0);

        int leftMousePressed = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        int leftMouseClicked = (leftMousePressed && !leftMouseWasPressed);

        if (leftMouseClicked)
        {
            if (screenState == SCREEN_MENU && x >= -0.5f && x <= 0.5f && y >= -0.25f && y <= 0.25f)
                openFileDialog();

            if (screenState == SCREEN_MENU && x >= -0.5f && x <= 0.5f && y >= -0.56f && y <= -0.34f)
                showAboutDialog();

            if (screenState == SCREEN_MENU && x >= -0.5f && x <= 0.5f && y >= -0.86f && y <= -0.64f)
                showHelpDialog();

            if (screenState == SCREEN_MAP && x >= 0.70f && x <= 0.98f && y >= -0.2f && y <= 0.2f)
                startColoringAnimation();

            if (screenState == SCREEN_MAP && x >= 0.86f && x <= 0.98f && y >= 0.78f && y <= 0.98f)
            {
                screenState = SCREEN_MENU;
                animationInProgress = 0;
                progressBarVisible = 0;
                progressBarDone = 0;
                updateWindowTitleByProgress(-1);
                resetMapView();
            }
        }

        leftMouseWasPressed = leftMousePressed;
        if (leftMousePressed) continue;
    }

    freeLoadedMapData();

    glDeleteVertexArrays(1, &mapVAO); glDeleteBuffers(1, &mapVBO);
    glDeleteVertexArrays(1, &loadButtonVAO); glDeleteBuffers(1, &loadButtonVBO);
    glDeleteVertexArrays(1, &aboutButtonVAO); glDeleteBuffers(1, &aboutButtonVBO);
    glDeleteVertexArrays(1, &helpButtonVAO); glDeleteBuffers(1, &helpButtonVBO);
    glDeleteVertexArrays(1, &colorButtonVAO); glDeleteBuffers(1, &colorButtonVBO);
    glDeleteVertexArrays(1, &closeButtonVAO); glDeleteBuffers(1, &closeButtonVBO);
    glDeleteVertexArrays(1, &progressBarVAO); glDeleteBuffers(1, &progressBarVBO);

    if (mapTexture) glDeleteTextures(1, &mapTexture);
    if (loadButtonTexture) glDeleteTextures(1, &loadButtonTexture);
    if (aboutButtonTexture) glDeleteTextures(1, &aboutButtonTexture);
    if (helpButtonTexture) glDeleteTextures(1, &helpButtonTexture);
    if (colorButtonTexture) glDeleteTextures(1, &colorButtonTexture);
    if (closeButtonTexture) glDeleteTextures(1, &closeButtonTexture);
    if (progressBarTexture) glDeleteTextures(1, &progressBarTexture);

    saveAppState();
    closeLog();
    glfwTerminate();
    return 0;
}
