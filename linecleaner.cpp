// linecleaner.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "linecleaner.h"

#include <commctrl.h>  // 引入进度条的头文件

#pragma comment(lib, "comctl32.lib")  // 链接comctl32库

#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uthash/uthash.h"

#define MAX_LOADSTRING 100
#define IDC_MAIN_FILE_BTN 101
#define IDC_SUB_FILES_BTN 102
#define IDC_PROCESS_BTN 103
#define IDC_FILE_LIST 104
#define IDC_MAIN_FILE_LABEL 105
#define IDC_CLEAN_FILES_BTN 106

char mainFileName[MAX_PATH];
char subFileNames[10][MAX_PATH];
int subFileCount = 0;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void AddFileToList(HWND hwnd, const char* filename);
void ProcessFiles(HWND hWnd);
void trimNewline(char* line);
char* readLine(FILE* file);
DWORD WINAPI thread_process(void* params);


typedef struct {
    char* line;
    UT_hash_handle hh;
} HashLine;

HashLine* mainFileHash = NULL;

int is_running = 0;

void trimNewline(char* str) {
    // 去除字符串末尾的空白字符
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[len - 1] = '\0';
        len--;
    }

    // 去除字符串开头的空白字符
    char* start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    // 将修剪后的字符串向前移动
    if (start != str) {
        memmove(str, start, len - (start - str) + 1);
    }
}


void addLineToHash(char* line) 
{
    trimNewline(line); // 去除行尾的换行符
    HashLine* s;
    HASH_FIND_STR(mainFileHash, line, s);
    if (s == NULL) {
        s = (HashLine*)malloc(sizeof(HashLine));
        s->line = _strdup(line);
        HASH_ADD_KEYPTR(hh, mainFileHash, s->line, strlen(s->line), s);
    }
}

void deleteLineFromHash(char* line) 
{
    trimNewline(line); // 去除行尾的换行符
    HashLine* s;
    HASH_FIND_STR(mainFileHash, line, s);
    if (s != NULL) {
        HASH_DEL(mainFileHash, s);
        free(s->line);
        free(s);
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wcex;
    HWND hWnd;
    MSG msg;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = "MainWindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

    RegisterClassEx(&wcex);

    hWnd = CreateWindow("MainWindowClass", "File Line Cleaner", WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME ^ WS_MAXIMIZEBOX,
        CW_USEDEFAULT, 0, 500, 400, NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hMainFileBtn, hSubFilesBtn, hProcessBtn, hFileList, hMainFileLabel;

    switch (message) {
    case WM_CREATE: {
        hMainFileBtn = CreateWindow("BUTTON", "Select Main File", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 20, 150, 30, hWnd, (HMENU)IDC_MAIN_FILE_BTN, GetModuleHandle(NULL), NULL);

        hMainFileLabel = CreateWindow("STATIC", "", WS_VISIBLE | WS_CHILD,
            20, 70, 150, 30, hWnd, (HMENU)IDC_MAIN_FILE_LABEL, GetModuleHandle(NULL), NULL);

        hSubFilesBtn = CreateWindow("BUTTON", "Select Sub Files", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 120, 150, 30, hWnd, (HMENU)IDC_SUB_FILES_BTN, GetModuleHandle(NULL), NULL);

        hSubFilesBtn = CreateWindow("BUTTON", "Clean Sub Files", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 170, 150, 30, hWnd, (HMENU)IDC_CLEAN_FILES_BTN, GetModuleHandle(NULL), NULL);

        hProcessBtn = CreateWindow("BUTTON", "Process Files", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 220, 150, 30, hWnd, (HMENU)IDC_PROCESS_BTN, GetModuleHandle(NULL), NULL);

        hFileList = CreateWindow("LISTBOX", NULL, WS_VISIBLE | WS_CHILD | LBS_STANDARD,
            200, 20, 250, 300, hWnd, (HMENU)IDC_FILE_LIST, GetModuleHandle(NULL), NULL);
      
        // 初始化公共控件库
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_PROGRESS_CLASS;
        InitCommonControlsEx(&icex);

        // 创建进度条控件
        HWND hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE,
            20, 320, 430, 30, hWnd, NULL, GetModuleHandle(NULL), NULL);
        break;
    }
    case WM_COMMAND: {

        if (is_running == 1) {
            break;
        }

        if (LOWORD(wParam) == IDC_MAIN_FILE_BTN) {
            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = mainFileName;
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = sizeof(mainFileName);
            ofn.lpstrFilter = "All Files\0*.*\0Text Files\0*.TXT\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn) == TRUE) {
                SetWindowText(hMainFileLabel, mainFileName);
            }
        }
        else if (LOWORD(wParam) == IDC_CLEAN_FILES_BTN) {
            SendMessage(hFileList, LB_RESETCONTENT, 0, 0);
        
        }
        else if (LOWORD(wParam) == IDC_SUB_FILES_BTN) {
            OPENFILENAME ofn;
            char fileBuffer[1024];
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = fileBuffer;
            ofn.lpstrFile[0] = '\0';
            ofn.nMaxFile = sizeof(fileBuffer);
            ofn.lpstrFilter = "All Files\0*.*\0Text Files\0*.TXT\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

            if (GetOpenFileName(&ofn) == TRUE) {
                char* ptr = ofn.lpstrFile;
                while (*ptr) {
                    if (subFileCount < 10) {
                        AddFileToList(hFileList, ptr);
                        strcpy(subFileNames[subFileCount++], ptr);
                    }
                    ptr += strlen(ptr) + 1;
                }
            }
        }
        else if (LOWORD(wParam) == IDC_PROCESS_BTN) {

            is_running = 1;
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_process, hWnd, 1, 0);

         
        }
        break;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    default: {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    }
    return 0;
}

size_t countLinesInFile(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return 0;
    size_t count = 0;
    char* line;
    while ((line = readLine(file)) != NULL) {
        count++;
        free(line);
    }
    fclose(file);
    return count;
}

void AddFileToList(HWND hwnd, const char* filename) {
    SendMessage(hwnd, LB_ADDSTRING, 0, (LPARAM)filename);
}

char* readLine(FILE* file) {
    size_t capacity = 256;
    size_t length = 0;
    char* buffer = (char*)malloc(capacity);
    if (!buffer) return NULL;

    while (fgets(buffer + length, capacity - length, file)) {
        length += strlen(buffer + length);
        if (buffer[length - 1] == '\n') break;
        if (length + 1 >= capacity) {
            capacity *= 2;
            char* newBuffer = (char*)realloc(buffer, capacity);
            if (!newBuffer) {
                free(buffer);
                return NULL;
            }
            buffer = newBuffer;
        }
    }
    if (length == 0) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

DWORD WINAPI thread_process(void* params)
{

    HWND hWnd = (HWND)params;
    ProcessFiles(hWnd);

    is_running = 0;
    return 0;
}

void ProcessFiles(HWND hWnd) {
    HWND hProgressBar = FindWindowEx(hWnd, NULL, PROGRESS_CLASS, NULL);

    // 计算总行数
    size_t totalLines = 0;
    for (int i = 0; i < subFileCount; i++) {
        totalLines += countLinesInFile(subFileNames[i]);
    }

    // 设置进度条的范围
    SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(hProgressBar, PBM_SETSTEP, (WPARAM)1, 0);

    FILE* mainFile = fopen(mainFileName, "r");
    if (!mainFile) {
        MessageBox(NULL, "Main File Not Open", "ERROR", MB_OK);
        return;
    }

    char* line;
    size_t processedLines = 0;
    size_t percent = 0;

    // Load main file lines into hash table
    while ((line = readLine(mainFile)) != NULL) {
        trimNewline(line);
        addLineToHash(line);
        free(line);
        
    }
    fclose(mainFile);



    for (int i = 0; i < subFileCount; i++) {
        FILE* subFile = fopen(subFileNames[i], "r");
        if (!subFile) continue;

        while ((line = readLine(subFile)) != NULL) {
            trimNewline(line);
            deleteLineFromHash(line);
            free(line);
            processedLines++;
            percent = processedLines * 100 / totalLines;
            SendMessage(hProgressBar, PBM_SETPOS, (WPARAM)percent , 0);  // 更新进度条
            
        }
        fclose(subFile);
    }

    // Save the result to a new file
    char newFileName[MAX_PATH];
    strcpy(newFileName, mainFileName);
    strcat(newFileName, "_processed.txt");

    FILE* newFile = fopen(newFileName, "w");
    if (!newFile) return;

    HashLine* s, * tmp;
    HASH_ITER(hh, mainFileHash, s, tmp) {
        fputs(s->line, newFile);
        fputc('\n', newFile); // 在写入新文件时加上换行符
        free(s->line);
        HASH_DEL(mainFileHash, s);
        free(s);
    }
    fclose(newFile);

    MessageBox(NULL, newFileName, "Processed File Saved As", MB_OK);
}
