#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include "resource.h"

// Variables globales
HWND hProgressBar;
HWND hStatusLabel;
HBITMAP hMargueriteBitmap;

// Prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI UpdateThread(LPVOID lpParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialiser les contrôles communs (pour la barre de progression)
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX) };
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    // Classe de la fenêtre
    const char CLASS_NAME[] = "LaMeuhWindowClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    // Création de la fenêtre
    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, _T("La Meuh - Mises à jour"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 250,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Boucle de messages
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            // Charger l'image de la marguerite
            hMargueriteBitmap = (HBITMAP)LoadImage(
                NULL, _T("marguerite.bmp"), IMAGE_BITMAP, 0, 0,
                LR_LOADFROMFILE | LR_CREATEDIBSECTION
            );

            // Créer un label pour l'image
            CreateWindow(
                "STATIC", NULL, WS_VISIBLE | WS_CHILD | SS_BITMAP,
                10, 10, 100, 100, hwnd, NULL, NULL, NULL
            );
            SendDlgItemMessage(hwnd, 0, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hMargueriteBitmap);

            // Créer un label de statut
            hStatusLabel = CreateWindow(
                "STATIC", _T("Prêt à mettre à jour !"),
                WS_VISIBLE | WS_CHILD | SS_CENTER,
                10, 120, 360, 20, hwnd, NULL, NULL, NULL
            );

            // Créer la barre de progression
            hProgressBar = CreateWindowEx(
                0, PROGRESS_CLASS, NULL,
                WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
                10, 150, 360, 20, hwnd, NULL, NULL, NULL
            );
            SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessage(hProgressBar, PBM_SETSTEP, 1, 0);

            // Bouton "Mettre à jour"
            CreateWindow(
                "BUTTON", _T("Mettre à jour"),
                WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                10, 180, 360, 30, hwnd, (HMENU)1, NULL, NULL
            );
            break;
        }
        case WM_COMMAND:
        {
            if (LOWORD(wParam) == 1)
            {
                SetWindowText(hStatusLabel, _T("Mise à jour en cours..."));
                CreateThread(NULL, 0, UpdateThread, hwnd, 0, NULL);
            }
            break;
        }
        case WM_DESTROY:
        {
            if (hMargueriteBitmap) DeleteObject(hMargueriteBitmap);
            PostQuitMessage(0);
            break;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

DWORD WINAPI UpdateThread(LPVOID lpParam)
{
    HWND hwnd = (HWND)lpParam;
    SendMessage(hProgressBar, PBM_SETPOS, 0, 0);

    // Lancer winget en caché (pas de fenêtre CMD)
    STARTUPINFO si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi;

    if (CreateProcess(
        NULL, _T("winget upgrade --all --accept-package-agreements --accept-source-agreements"),
        NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi
    ))
    {
        DWORD exitCode;
        while (1)
        {
            GetExitCodeProcess(pi.hProcess, &exitCode);
            if (exitCode != STILL_ACTIVE) break;
            SendMessage(hProgressBar, PBM_STEPIT, 0, 0);
            Sleep(500);
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        SendMessage(hProgressBar, PBM_SETPOS, 100, 0);
        SetWindowText(hStatusLabel, _T("Toutes les MAJ sont OK !"));
    }
    else
    {
        SetWindowText(hStatusLabel, _T("Erreur lors de la mise à jour."));
    }
    return 0;
}
