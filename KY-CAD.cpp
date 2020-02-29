// KY-CAD.cpp : アプリケーションのエントリ ポイントを定義します。
//

#pragma comment(lib, "htmlhelp")
#pragma comment(lib, "shlwapi")
#pragma comment(lib, "version")

#include "framework.h"
#include "KY-CAD.h"
#include <htmlhelp.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <vector>

#define MAX_LOADSTRING 100
#define COLOR_SELECT RGB(255,0,0)

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名
BOOL m_bPreview;
HDC m_hDC;

struct KYPOINT {
    double x;
    double y;
};

class KyObject {
private:
    BOOL m_bSelected;
    BOOL m_bInvalid;
public:
    KyObject() : m_bSelected(FALSE), m_bInvalid(FALSE) {}
    virtual const void Draw() {};
    virtual const void Delete() { m_bInvalid = TRUE; }
    virtual const BOOL IsSelected() { return m_bSelected; }
    virtual const VOID Select() { m_bSelected = TRUE; }
    virtual const VOID UnSelect() { m_bSelected = FALSE; }
    virtual const BOOL IsInvalid() { return m_bInvalid; }
};

class KyPoint : public KyObject {
private:
    double m_dX;
    double m_dY;
    double m_dWidth;
    double m_dHeight;
public:
    KyPoint(double x, double y) :m_dX(x), m_dY(y) { m_dWidth = 4.0; m_dHeight = 4.0; }
    const VOID Draw() override {
        Ellipse(m_hDC, (int)(m_dX - m_dWidth), (int)(m_dY - m_dHeight), (int)(m_dX + m_dWidth), (int)(m_dY + m_dHeight));
    }
};

class KyLine : public KyObject {
private:
    KYPOINT m_posStart;
    KYPOINT m_posEnd;
public:
    KyLine(KYPOINT start, KYPOINT end) :m_posStart(start), m_posEnd(end) {}
    const VOID Draw() override {
        MoveToEx(m_hDC, (int)m_posStart.x, (int)m_posStart.y, 0);
        LineTo(m_hDC, (int)m_posEnd.x, (int)m_posEnd.y);
    }
};

class KyObjectList {
    std::vector<KyObject*> m_ObjectList;
public:
    const VOID Draw(HDC hdc) {
        m_hDC = hdc;
        for (auto obj : m_ObjectList) { obj->Draw(); }
    }
    VOID InsertObject(KyObject* obj) {
        m_ObjectList.push_back(obj);
    }
    ~KyObjectList() {
        for (auto obj : m_ObjectList) { delete obj; }
    }
};

// このコード モジュールに含まれる関数の宣言を転送します:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: ここにコードを挿入してください。

    // グローバル文字列を初期化する
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_KYCAD, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // アプリケーション初期化の実行:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_KYCAD));

    MSG msg;

    // メイン メッセージ ループ:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

//
//  関数: MyRegisterClass()
//
//  目的: ウィンドウ クラスを登録します。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_KYCAD));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_KYCAD);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   関数: InitInstance(HINSTANCE, int)
//
//   目的: インスタンス ハンドルを保存して、メイン ウィンドウを作成します
//
//   コメント:
//
//        この関数で、グローバル変数でインスタンス ハンドルを保存し、
//        メイン プログラム ウィンドウを作成および表示します。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // グローバル変数にインスタンス ハンドルを格納する

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

void TogglePreviewMode(HWND hWnd)
{
    static LONG lStyle;
    static RECT rect;
    static HMENU hMenu;
    if (m_bPreview) {
        SetWindowLong(hWnd, GWL_STYLE, lStyle);
        SetWindowPos(hWnd, HWND_NOTOPMOST, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_FRAMECHANGED);
        SetMenu(hWnd, hMenu);
        m_bPreview = FALSE;
    }
    else {
        GetWindowRect(hWnd, &rect);
        hMenu = GetMenu(hWnd);
        SetMenu(hWnd, NULL);
        lStyle = GetWindowLong(hWnd, GWL_STYLE);
        SetWindowLong(hWnd, GWL_STYLE, WS_VISIBLE);
        auto hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
        GetMonitorInfo(hMonitor, &monitorInfo);
        SetWindowPos(hWnd, HWND_TOPMOST, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, SWP_NOZORDER | SWP_FRAMECHANGED);
        m_bPreview = TRUE;
    }
}

//
//  関数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目的: メイン ウィンドウのメッセージを処理します。
//
//  WM_COMMAND  - アプリケーション メニューの処理
//  WM_PAINT    - メイン ウィンドウを描画する
//  WM_DESTROY  - 中止メッセージを表示して戻る
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static KyObjectList * m_pObjectList;
    switch (message)
    {
    case WM_CREATE:
        m_pObjectList = new KyObjectList;
        {
            KyPoint* obj1 = new KyPoint(100.0, 100.0);
            m_pObjectList->InsertObject(obj1);
            KyLine* obj2 = new KyLine(KYPOINT({ 0.0,0.0 }), KYPOINT({ 200.0, 200.0 }));
            m_pObjectList->InsertObject(obj2);
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
        if (m_bPreview)
        {
            TogglePreviewMode(hWnd);
        }
        return DefWindowProc(hWnd, message, wParam, lParam);
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // 選択されたメニューの解析:
            switch (wmId)
            {
            case IDM_FEEDBACK:
                ShellExecute(hWnd, TEXT("open"), TEXT("https://ky-cad.com/ask"), NULL, NULL, SW_SHOWNORMAL);
                break;
            case IDM_PREVIEW:
                TogglePreviewMode(hWnd);
                break;
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_HELP:
                {
                    TCHAR szHelpFilePath[MAX_PATH];
                    GetModuleFileName(GetModuleHandle(0), szHelpFilePath, _countof(szHelpFilePath));
                    PathRemoveFileSpec(szHelpFilePath);
                    PathAppend(szHelpFilePath, TEXT("help.chm"));
                    HtmlHelp(hWnd, szHelpFilePath, HH_DISPLAY_TOPIC, 0);
                }
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            m_pObjectList->Draw(hdc);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        delete m_pObjectList;
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// バージョン情報ボックスのメッセージ ハンドラーです。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        {
            TCHAR szExePath[MAX_PATH];
            GetModuleFileName(GetModuleHandle(0), szExePath, _countof(szExePath));
            DWORD dwDummy;
            DWORD dwSize = GetFileVersionInfoSize(szExePath, &dwDummy);
            LPVOID lpData = GlobalAlloc(0, dwSize);
            if (lpData) {
                GetFileVersionInfo(szExePath, 0, dwSize, lpData);
                LPVOID lpBuffer;
                UINT uSize;
                if (VerQueryValue(lpData, _T("\\"), &lpBuffer, &uSize)) {
                    VS_FIXEDFILEINFO* pFileInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                    TCHAR szStaticText[256];
                    GetDlgItemText(hDlg, IDC_VERSION, szStaticText, _countof(szStaticText));
                    TCHAR szWithVersion[256];
                    wsprintf(szWithVersion, TEXT("%s %d.%d.%d.%d"),
                        szStaticText,
                        HIWORD(pFileInfo->dwFileVersionMS),
                        LOWORD(pFileInfo->dwFileVersionMS),
                        HIWORD(pFileInfo->dwFileVersionLS),
                        LOWORD(pFileInfo->dwFileVersionLS));
                    SetDlgItemText(hDlg, IDC_VERSION, szWithVersion);
                }
                GlobalFree(lpData);
            }
        }
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
