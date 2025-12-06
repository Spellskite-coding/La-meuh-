#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include "resource.h"

// Variables globales
HWND hProgressBar;
HWND hStatusLabel;
HWND hLogEdit;
HBITMAP hMargueriteBitmap;

// Prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI UpdateThread(LPVOID lpParam);
void LogMessage(HWND hwnd, LPCTSTR message);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX) };
    icex.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    const char CLASS_NAME[] = "LaMeuhWindowClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, _T("La Meuh - Mises à jour"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
        NULL, NULL, hInstance, NULL
    );
    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

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
            // Charge l'image depuis les ressources
            hMargueriteBitmap = (HBITMAP)LoadImage(
                GetModuleHandle(NULL),
                MAKEINTRESOURCE(IDB_MARGUERITE),
                IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION
            );
            if (!hMargueriteBitmap)
                MessageBox(hwnd, _T("Erreur interne : impossible de charger l'image embarquée."), _T("Erreur"), MB_ICONERROR);

            CreateWindow(
                "STATIC", NULL, WS_VISIBLE | WS_CHILD | SS_BITMAP,
                10, 10, 100, 100, hwnd, NULL, NULL, NULL
            );
            SendDlgItemMessage(hwnd, 0, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hMargueriteBitmap);

            hStatusLabel = CreateWindow(
                "STATIC", _T("Prêt à mettre à jour !"),
                WS_VISIBLE | WS_CHILD | SS_CENTER,
                10, 120, 360, 20, hwnd, NULL, NULL, NULL
            );

            hLogEdit = CreateWindow(
                "EDIT", NULL,
                WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                10, 50, 360, 60, hwnd, (HMENU)2, NULL, NULL
            );

            hProgressBar = CreateWindowEx(
                0, PROGRESS_CLASS, NULL,
                WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
                10, 210, 360, 20, hwnd, NULL, NULL, NULL
            );
            SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessage(hProgressBar, PBM_SETSTEP, 1, 0);

            CreateWindow(
                "BUTTON", _T("Mettre à jour"),
                WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                10, 240, 360, 30, hwnd, (HMENU)1, NULL, NULL
            );
            break;
        }
        case WM_COMMAND:
        {
            if (LOWORD(wParam) == 1)
            {
                SetWindowText(hStatusLabel, _T("Recherche de mises à jour..."));
                SendMessage(hLogEdit, WM_SETTEXT, 0, (LPARAM)_T(""));
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

void LogMessage(HWND hwnd, LPCTSTR message)
{
    SendMessage(hLogEdit, EM_REPLACESEL, 0, (LPARAM)message);
    SendMessage(hLogEdit, EM_REPLACESEL, 0, (LPARAM)_T("\r\n"));
}

DWORD WINAPI UpdateThread(LPVOID lpParam)
{
    HWND hwnd = (HWND)lpParam;
    SendMessage(hProgressBar, PBM_SETPOS, 0, 0);
    LogMessage(hwnd, _T("Vérification de winget..."));

    // Vérifier si winget existe
    if (system("where winget >nul 2>&1") != 0)
    {
        SetWindowText(hStatusLabel, _T("La meuh n'a pas pu mettre à jour vos programmes. Votre PC n'est pas sous Windows 11 ou alors il rencontre un problème. Veuillez contacter votre informaticien local."));
        return 1;
    }

    // Lancer winget en caché et capturer la sortie
    SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
    {
        SetWindowText(hStatusLabel, _T("Erreur interne."));
        return 1;
    }

    STARTUPINFO si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;

    PROCESS_INFORMATION pi;
    TCHAR cmd[] = _T("winget upgrade --all --accept-package-agreements --accept-source-agreements");

    if (!CreateProcess(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        SetWindowText(hStatusLabel, _T("Erreur lors du lancement de winget."));
        return 1;
    }

    CloseHandle(hWrite);

    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead != 0)
    {
        buffer[bytesRead] = '\0';
        // Chercher des lignes intéressantes (ex: "Trouvé", "Installation")
        if (strstr(buffer, "Trouvé") || strstr(buffer, "Installation") || strstr(buffer, "Mise à jour"))
        {
            TCHAR wbuffer[4096];
            MultiByteToWideChar(CP_ACP, 0, buffer, -1, wbuffer, sizeof(wbuffer)/sizeof(TCHAR));
            LogMessage(hwnd, wbuffer);
            SendMessage(hProgressBar, PBM_STEPIT, 0, 0);
        }
    }

    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode == 0)
    {
        SetWindowText(hStatusLabel, _T("Tous les programmes sont à jour !"));
        SendMessage(hProgressBar, PBM_SETPOS, 100, 0);
    }
    else
    {
        SetWindowText(hStatusLabel, _T("Aucune mise à jour disponible !"));
    }

    return 0;
}
