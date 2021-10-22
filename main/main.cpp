// TODO: fix scale for very long images
// TODO: add mouse support
// TODO: improve y coordinates of visible region search
// TODO: add full size image viiewer

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui_internal.h"

#include <stdio.h>

#include <GL/glew.h>            // Initialize with glewInit()
// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>
#include "icons_font_awesome.h"

#include <chrono>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_resize.h"
#define LAY_IMPLEMENTATION
#include "image_size/image_size.h"

//#include <wingdi.h>
// stb image can read
// png jpg tga bmp psd gif hdr pic


// ----------------------------------------------------------------------
// Resizes image to given size, preserving sides ratio
// ----------------------------------------------------------------------
float fit_scale(int im_w, int im_h, int maxw, int maxh)
{
    float scale = 1;
    if (float(im_w) / float(maxw) > float(im_h) / float(maxh))
    {
        scale = float(maxw) / im_w;
    }
    else
    {
        scale = float(maxh) / im_h;
    }
    return scale;
}

class ImGuiImageList
{
public:
    // shared context
    GLFWwindow* offscreen_context;

    class Image
    {
    public:
        int full_width;
        int full_height;
        GLuint texture;

        // Simple helper function to load an image into a OpenGL texture with common settings
        bool LoadTextureFromFile(const char* filename)
        {
            // Load from file
            full_width = 0;
            full_height = 0;
            if (texture != 0)
            {
                glDeleteTextures(1, &texture);
                texture = 0;
            }

            glGenTextures(1, &texture);

            unsigned char* image_data = stbi_load(filename, &full_width, &full_height, NULL, 4);
            if (image_data == NULL)
            {
                return false;
            }

            float scale = row_height / full_height;
            int resized_width = full_width * scale;
            int resized_height = full_height * scale;
            unsigned char* output_pixels = (unsigned char*)malloc(resized_width * resized_height * 4);

            stbir_resize_uint8(image_data, full_width, full_height, 0, output_pixels, resized_width, resized_height, 0, 4);

            glBindTexture(GL_TEXTURE_2D, texture);

            // Setup filtering parameters for display
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

            // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resized_width, resized_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, output_pixels);
            stbi_image_free(image_data);
            stbi_image_free(output_pixels);
            return true;
        }

        Image()
        {
            full_width = 0;
            full_height = 0;
            texture = 0;
        }

        Image(std::string filename)
        {
            full_width = 0;
            full_height = 0;
            texture = 0;
            LoadTextureFromFile(filename.c_str());
        }

        ~Image()
        {
            glDeleteTextures(1, &texture);
        }
    };

    // imagelist cell
    // where images will be placed
    class Cell
    {
    public:
        // index of dara in sizes and paths sets
        int index;
        int x;
        int y;
        int width;
        int height;
        Cell()
        {
            x = 0;
            y = 0;
            width = 0;
            height = 0;
        }

        void render(float scrollx, float scrolly, Image* image, std::string label)
        {
            auto draw_list = ImGui::GetWindowDrawList();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 p1(x + p0.x - scrollx, y + p0.y - scrolly);
            ImVec2 p2(p1.x + width, p1.y + height);
            draw_list->AddRect(p1, p2, IM_COL32(255, 255, 0, 255));
            if (image != nullptr)
            {
                draw_list->AddImage((void*)(intptr_t)image->texture, p1, p2);
            }
            ImVec4 clip_rect(p1.x, p1.y, p2.x, p2.y);
            draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), p1, IM_COL32(0, 255, 0, 255), label.c_str(), NULL, 0.0f, &clip_rect);
        }

        ~Cell()
        {

        }
    };

    // scroll bar position
    float p_scroll_v;
    // scroll bar width
    int scrollBarWidth;

    bool needResizeFlag = true;
    ImVec2 prevClientSize;


    static float row_height;
    // image sizes
    std::vector<ImVec2> sizes;
    // image file names
    std::vector<fs::path> paths;
    // set of required images
    std::set<std::string> requiredImages;
    std::set<std::string> loadedImages;
    std::map<std::string, Image*> imagesPool;
    // map key: image row number, value: vector of rectangles in this row 
    std::map<size_t, std::vector<Cell> > cells;

    ImVec2 avail_size = ImVec2(0, 0);    
    std::mutex updateMutex;
    std::thread* updateThread = nullptr;
    size_t y_max = 0;
    bool stopUpdateThread = false;

    void render(std::string name)
    {
        ImGui::Begin(name.c_str());
        if (!ImGui::IsWindowCollapsed())
        {
            ImGui::BeginChild((name + "_child").c_str());
            avail_size = ImGui::GetContentRegionAvail();
            
            updateMutex.lock();            
            if (!needResizeFlag)
            {
                needResizeFlag = (avail_size.x != prevClientSize.x) ||
                                 (sizes.size() != paths.size()) ;                
            }

            prevClientSize.x = avail_size.x;
            prevClientSize.y = avail_size.y;

            ImVec2 p0 = ImGui::GetCursorScreenPos();
            auto draw_list = ImGui::GetWindowDrawList();
            draw_list->PushClipRect(p0, ImVec2(p0.x + avail_size.x, p0.y + avail_size.y), true);
            int client_width = avail_size.x;
            int client_height = avail_size.y;

            if (!cells.empty())
            {
                for (auto items_row : cells)
                {
                    for (auto& item : items_row.second)
                    {
                        if (item.y - p_scroll_v >= -row_height && item.y + item.height - p_scroll_v <= client_height + row_height)
                        {
                            //item.fname = paths[item.index].string();                                                        
                            item.render(0, p_scroll_v, imagesPool[paths[item.index].string()], std::to_string(items_row.first) + " " + std::to_string(item.index));
                        }
                    }
                }
                draw_list->PopClipRect();
            }

            ImGuiAxis axis = ImGuiAxis::ImGuiAxis_Y;
            ImGuiContext& g = *GImGui;
            ImGuiWindow* window = g.CurrentWindow;
            const ImGuiID id = ImGui::GetWindowScrollbarID(window, axis);
            ImGui::KeepAliveID(id);
            ImRect bb_frame(p0.x + avail_size.x - 20, p0.y, p0.x + avail_size.x, p0.y + avail_size.y);

            float size_avail_v = client_height;
            float size_contents_v = y_max;
            ImDrawFlags flags = ImDrawFlags_None;
            ImGui::ScrollbarEx(bb_frame, id, axis, &p_scroll_v, size_avail_v, size_contents_v, flags);            
            updateMutex.unlock();            
            ImGui::EndChild();
        }
        ImGui::End();
    }
 
    void loadWorker()
    {
        while (!stopUpdateThread)
        {
            glfwMakeContextCurrent(offscreen_context);

            
            if (needResizeFlag)
            {
                updateMutex.lock();
                ImGuiImageList::arrange();
                needResizeFlag = false;
                updateMutex.unlock();
            }
            
            requiredImages.clear();

            for (auto items_row : cells)
            {
                for (auto& item : items_row.second)
                {
                    if (item.y - p_scroll_v >= -row_height && item.y + item.height - p_scroll_v <= avail_size.y + row_height)
                    {
                        requiredImages.insert(paths[item.index].string());
                    }
                }
            }
            
            std::set<std::string> needToLoad;
            std::set_difference(requiredImages.begin(), requiredImages.end(), loadedImages.begin(), loadedImages.end(), inserter(needToLoad, needToLoad.begin()));
            std::set<std::string> needToRemove;
            std::set_difference(loadedImages.begin(), loadedImages.end(), requiredImages.begin(), requiredImages.end(), inserter(needToRemove, needToRemove.begin()));
            if (!needToLoad.empty())
            {
                Image* img = new Image(*needToLoad.begin());
                if (img != nullptr)
                {
                    if (img->texture != GLU_INVALID_ENUM)
                    {
                        imagesPool[*needToLoad.begin()] = img;
                        std::cout << "loaded " << *needToLoad.begin() << std::endl;
                        loadedImages.insert(*needToLoad.begin());
                    }
                }
            }
            if (!needToRemove.empty())
            {
                delete imagesPool[*needToRemove.begin()];
                imagesPool.erase(*needToRemove.begin());
                loadedImages.erase(*needToRemove.begin());
                std::cout << "image removed!" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    ImGuiImageList()
    {
        p_scroll_v = 0;
        scrollBarWidth = 20;
        row_height = 128;

        offscreen_context = glfwCreateWindow(100, 100, "", NULL, glfwGetCurrentContext());
        glfwHideWindow(offscreen_context);
        updateThread = new std::thread(&ImGuiImageList::loadWorker, this);
    }

    ~ImGuiImageList()
    {
        stopUpdateThread = true;
        if (updateThread != nullptr)
        {
            updateThread->join();
            delete updateThread;
            updateThread = nullptr;
        }
        cells.clear();
    }

    void scanFolder(fs::path root_path)
    {
        paths.clear();
        sizes.clear();
        requiredImages.clear();        
        loadedImages.clear();
        
        for (auto& it = imagesPool.begin(); it != imagesPool.end(); ++it)
        {
            delete it->second;
        }
        imagesPool.clear();

        
        std::vector<fs::path> filepaths;
        //std::set<std::string> extensions = { ".JPEG",".JPG",".PNG",".BMP",".TIFF",".GIF",".PPM",".PGM",".PBM" };
        std::set<std::string> extensions = { ".JPEG",".JPG",".PNG",".BMP",".GIF" };
        // get images file list
        parseFoldersRecursive(root_path, extensions, filepaths);
        int N = filepaths.size();

        auto t1 = high_resolution_clock::now();
        int full_width = -1;
        int full_height = -1;

        for (int i = 0; i < N; i++)
        {
            getImageSize(filepaths[i], full_width, full_height);
            if (full_width > 0 && full_height > 0)
            {
                sizes.push_back(ImVec2(full_width, full_height));
                paths.push_back(filepaths[i]);
            }
        }

        auto t2 = high_resolution_clock::now();
        auto ms_int = duration_cast<milliseconds>(t2 - t1);
        std::cout << "scanned " << N << " images" << std::endl;
        std::cout << "elapsed " << ms_int.count() << " ms" << std::endl;
        std::cout << "done!" << std::endl;
        needResizeFlag = true;
    }

    void arrange()
    {
        // restore client region        
        size_t client_width = avail_size.x - scrollBarWidth - 4;
        size_t client_height = avail_size.y;
        size_t x = 0;
        size_t y = 0;
        size_t row = 0;
        cells.clear();

        cells[0] = std::vector<Cell>(0);
        for (int i = 0; i < sizes.size(); ++i)
        {
            float scale = row_height / sizes[i % sizes.size()].y;
            int w = sizes[i].x * scale;
            int h = sizes[i].y * scale;
            if (x < client_width - w)
            {
                Cell item;
                item.x = x;
                item.y = y;
                item.width = w;
                item.height = h;
                item.index = i;
                cells[row].push_back(item);
                x += w;

            }
            else
            {

                float free_space = client_width - cells[row][cells[row].size() - 1].x - cells[row][cells[row].size() - 1].width;
                // number of gaps
                float n = cells[row].size();
                float additional_margin = free_space / n;
                int ind = 0;
                for (auto& r : cells[row])
                {
                    if (ind > 0 && ind < cells[row].size())
                    {
                        r.x += additional_margin * (ind + 1);
                    }
                    ++ind;
                }

                x = 0;
                y += h;
                row += 1;
                --i;
                cells[row] = std::vector<Cell>(0);
            }
        }
        if (!cells.empty())
        {
            for (auto items_row : cells)
            {
                for (auto& item : items_row.second)
                {
                    if (y_max < item.y + item.width)
                    {
                        y_max = item.y + item.width;
                    }
                }
            }
        }
    }

private:

};
float ImGuiImageList::row_height = 128;

ImGuiImageList* List;
bool resized = false;

void ShowAppDockSpace(bool* p_open);
// Main window
GLFWwindow* window;
bool initialized = false;
int win_width;
int win_height;
int sidePanelWidth;

const float toolbarSize = 50;
const float statusbarSize = 50;
float menuBarHeight = 0;

void DockSpaceUI()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    // Save off menu bar height for later.
    //menuBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();

    ImGui::SetNextWindowPos(viewport->Pos + ImVec2(0, toolbarSize + menuBarHeight));
    ImGui::SetNextWindowSize(viewport->Size - ImVec2(0, toolbarSize + menuBarHeight + statusbarSize));

    ImGui::SetNextWindowViewport(viewport->ID);
    ImGuiWindowFlags window_flags = 0
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("Master DockSpace", NULL, window_flags);
    ImGuiID dockMain = ImGui::GetID("MyDockspace");
    ImGui::DockSpace(dockMain, ImVec2(0.0f, 0.0f));
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void ToolbarUI()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, toolbarSize + menuBarHeight));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGuiWindowFlags window_flags = 0
        | ImGuiWindowFlags_MenuBar
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings
        ;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.5f, 1.0f));
    ImGui::Begin("TOOLBAR", NULL, window_flags);
    ImGui::PopStyleVar();
    menuBarHeight = ImGui::GetCurrentWindow()->MenuBarHeight();
    ImGui::Button(ICON_FA_FILE, ImVec2(0, 37));
    ImGui::SameLine();
    ImGui::Button(ICON_FA_FOLDER_OPEN_O, ImVec2(0, 37));
    ImGui::SameLine();
    ImGui::Button(ICON_FA_FLOPPY_O, ImVec2(0, 37));


    // ----------------------------------------------------
    // Этрисовка меню
    // ----------------------------------------------------
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load", "", false)) { ; }
            if (ImGui::MenuItem("Save", "", false)) { ; }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "", false)) { ; }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }


    ImGui::End();
    ImGui::PopStyleColor();
}

void StatusbarUI()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - statusbarSize));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, statusbarSize));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGuiWindowFlags window_flags = 0
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings
        ;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.5f, 1.0f));
    ImGui::Begin("STATUSBAR", NULL, window_flags);
    ImGui::PopStyleVar();
    ImGui::Text("Status bar message.");
    ImGui::End();
    ImGui::PopStyleColor();
}
// -----------------------------
// Organize our dockspace
// -----------------------------
void ProgramUI()
{
    DockSpaceUI();
    ToolbarUI();
    StatusbarUI();
}



std::chrono::steady_clock::time_point prev = std::chrono::steady_clock::now();
void render()
{
    ImGui::GetIO().WantCaptureMouse = true;
    glfwPollEvents();
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // Render our dock (menu, toolbar, status bar).
    ProgramUI();
    // Rendering user content
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    // std::cout << "Elapseed = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[µs]" << std::endl;
    // std::cout << "Elapseed (frame)= " << std::chrono::duration_cast<std::chrono::microseconds>(end - prev).count() << "[µs]" << std::endl;
    prev = end;

    ImGui::Begin(u8"Control", nullptr);
    if (ImGui::Button(u8"Process", ImVec2(-1, 0)))
    {

    }
    ImGui::End();

    List->render("myWidget");
    //myWidget("myWidget");
    // ImGui rendering
    glClearColor(0, 0, 0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    // Rendering
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

void resize_window_callback(GLFWwindow* glfw_window, int x, int y)
{
    if (x == 0 || y == 0)
    {
        return;
    }
    if (initialized)
    {
        render();
    }
    resized = true;
}
// -----------------------------
// Initializing openGL things
// -----------------------------
void InitGraphics()
{
    // Setup window
    //glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        return;
    }

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // Create window with graphics context
    window = glfwCreateWindow(win_width, win_height, "CGAL Framework", NULL, NULL);
    if (window == NULL)
    {
        return;
    }


    //windows[1] = glfwCreateWindow(400, 400, "Second", NULL, windows[0]);
    // https://www.khronos.org/opengl/wiki/OpenGL_and_multithreading

   // HGLRC glrc1 = wglCreateContext(window);
   // wglShareLists((HGLRC)window,NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Load Fonts        
    io.Fonts->AddFontFromFileTTF("fonts/FiraCode/ttf/FiraCode-Regular.ttf", 30, NULL, io.Fonts->GetGlyphRangesCyrillic());
    // merge in icons from Font Awesome
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("fonts/fontawesome-webfont.ttf", 30.0f, &icons_config, icons_ranges);

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    // Initialize GLEW
    glewExperimental = true; // Needed for core profile

    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialize GLEW\n");
        return;
    }
    List = new ImGuiImageList();
    fs::path root_path("F:/ImagesForTest/");
    List->scanFolder(root_path);

    glfwSetWindowSizeCallback(window, resize_window_callback);
    initialized = true;
}


// -----------------------------
// Free graphic resources
// -----------------------------
void TerminateGraphics(void)
{
    initialized = false;
    // Cleanup   
    delete List;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

void main(void)
{
    //LocaleName = "ru_RU.utf8";
    //setlocale(LC_ALL, LocaleName.c_str());
    // Size of window
    win_width = 1024;
    win_height = 768;
    // Width of side panel
    sidePanelWidth = 300;
    static bool open = true;
    // initialize openGL stuff.
    InitGraphics();
    // -------------
    // Main loop
    // -------------    
    while (!glfwWindowShouldClose(window))
    {
        glfwMakeContextCurrent(window);
        if (initialized)
        {
            if (resized)
            {
                resized = false;
                // do something on main window resize finish
            }
            render();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    TerminateGraphics();
}