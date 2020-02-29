// KY-CAD.cpp : アプリケーションのエントリ ポイントを定義します。
//

#pragma comment(lib, "htmlhelp")
#pragma comment(lib, "shlwapi")
#pragma comment(lib, "version")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "comctl32")

#include "framework.h"
#include "KY-CAD.h"
#include <htmlhelp.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <vector>
#include <d2d1.h>
#include <commctrl.h>
#include "util.h"

#define MAX_LOADSTRING 100
#define COLOR_SELECT RGB(255,0,0)
#define GRID_WIDTH 0.5

// グローバル変数:
HINSTANCE hInst;                                // 現在のインターフェイス
WCHAR szTitle[MAX_LOADSTRING];                  // タイトル バーのテキスト
WCHAR szWindowClass[MAX_LOADSTRING];            // メイン ウィンドウ クラス名
BOOL m_bPreview;
BOOL m_bShowStatusBar = TRUE;
BOOL m_bShowGridLine = TRUE;
ID2D1Factory* pFactory;
ID2D1HwndRenderTarget* pRenderTarget;
ID2D1SolidColorBrush* pGirdBrush;

class KyObject {
private:
    BOOL m_bSelected;
    BOOL m_bInvalid;
protected:
    ID2D1SolidColorBrush* pBrush;
public:
    KyObject() : m_bSelected(FALSE), m_bInvalid(FALSE), pBrush(NULL)
    {
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &pBrush);
    }
    virtual void Draw() const {};
    virtual void Delete() { m_bInvalid = TRUE; }
    virtual BOOL IsSelected() const { return m_bSelected; }
    virtual VOID Select() { m_bSelected = TRUE; }
    virtual VOID UnSelect() { m_bSelected = FALSE; }
    virtual BOOL IsInvalid() const { return m_bInvalid; }
    virtual BOOL HitTest(KYPOINT pos) const { return FALSE; }
    ~KyObject() { SafeRelease(&pBrush); }
};

class KyPoint : public KyObject {
private:
    double m_dX;
    double m_dY;
    double m_dWidth;
    double m_dHeight;
    D2D1_ELLIPSE ellipse;
public:
    KyPoint(double x, double y) :m_dX(x), m_dY(y) {
        m_dWidth = 4.0; m_dHeight = 4.0;
        ellipse = D2D1::Ellipse(D2D1::Point2F((FLOAT)x, (FLOAT)y), (FLOAT)m_dWidth, (FLOAT)m_dHeight);
    }
    VOID Draw() const override {
        pRenderTarget->FillEllipse(ellipse, pBrush);
    }
    BOOL HitTest(KYPOINT pos) const override { return FALSE; }
};

class KyLine : public KyObject {
private:
    KYPOINT m_posStart;
    KYPOINT m_posEnd;
    double m_dWidth = 1.0F;
public:
    KyLine(KYPOINT start, KYPOINT end) :m_posStart(start), m_posEnd(end) {}
    VOID Draw() const override {
        pRenderTarget->DrawLine(D2D1::Point2F((FLOAT)m_posStart.x, (FLOAT)m_posStart.y), D2D1::Point2F((FLOAT)m_posEnd.x, (FLOAT)m_posEnd.y), pBrush, (FLOAT)m_dWidth);
    }
};

class KyObjectList {
    std::vector<KyObject*> m_ObjectList;
public:
    VOID Draw() const {
        for (auto obj : m_ObjectList) { obj->Draw(); }
    }
    VOID InsertObject(KyObject* obj) {
        m_ObjectList.push_back(obj);
    }
    ~KyObjectList() {
        for (auto obj : m_ObjectList) { delete obj; }
    }
};

class KyInputMode {
public:
    enum IMPUTMODE {
        NONE,
        SELECT,
        MOVEPOINT, // 頂点のドラッグ
        MOVEOBJECT, // オブジェクトのドラッグ
        INPUTPOINT, // 点
        INPUTLINE, // 線分
        INPUTCLOSEDPOLYGON, // 閉じた多角形
        INPUTOPENPOLYGON, // 開いた多角形（連続した線分）入力
    };
private:
    IMPUTMODE m_nInputMode = NONE;
public:
    KyInputMode(IMPUTMODE mode) : m_nInputMode(mode) {}
    virtual IMPUTMODE GetInputMode() const { return m_nInputMode; }
    virtual VOID OnLButtonDown(WPARAM, LPARAM){}
    virtual VOID OnLButtonUp(WPARAM, LPARAM) {}
    virtual VOID OnRButtonDown(WPARAM, LPARAM) {}
    virtual VOID OnRButtonUp(WPARAM, LPARAM) {}
    virtual VOID OnMouseMove(WPARAM, LPARAM) {}
    virtual VOID OnKeyDown(WPARAM, LPARAM) {}
    virtual VOID OnKeyUp(WPARAM, LPARAM) {}
};

class KySelectMode : public KyInputMode {
public:
    KySelectMode() : KyInputMode(SELECT) {}
    VOID OnLButtonDown(WPARAM wParam, LPARAM lParam) override {}
    VOID OnLButtonUp() {}
    VOID OnRButtonDown() {}
    VOID OnRButtonUp() {}
    VOID OnMouseMove() {}
    VOID OnKeyDown() {}
    VOID OnKeyUp() {}
};


VOID DrawGrid()
{
    D2D1_SIZE_F size = pRenderTarget->GetSize();
    for (double x = 0.0; x < size.width; x += 50.0)
    {
        pRenderTarget->DrawLine(D2D1::Point2F((FLOAT)x, 0.0), D2D1::Point2F((FLOAT)x, (FLOAT)size.height), pGirdBrush, (FLOAT)GRID_WIDTH);
    }
    for (double y = 0.0; y < size.height; y += 50.0)
    {
        pRenderTarget->DrawLine(D2D1::Point2F(0.0, (FLOAT)y), D2D1::Point2F((FLOAT)size.width, (FLOAT)y), pGirdBrush, (FLOAT)GRID_WIDTH);
    }
}

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

   HWND hWnd = CreateWindowExW(WS_EX_CLIENTEDGE, szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
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
        if (m_bShowStatusBar) {
            ShowWindow(GetDlgItem(hWnd, IDC_STATUSBAR), SW_SHOWNA);
        }
        m_bPreview = FALSE;
    }
    else {
        GetWindowRect(hWnd, &rect);
        hMenu = GetMenu(hWnd);
        SetMenu(hWnd, NULL);
        ShowWindow(GetDlgItem(hWnd, IDC_STATUSBAR), SW_HIDE);
        lStyle = GetWindowLong(hWnd, GWL_STYLE);
        SetWindowLong(hWnd, GWL_STYLE, WS_VISIBLE);
        auto hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
        GetMonitorInfo(hMonitor, &monitorInfo);
        SetWindowPos(hWnd, HWND_TOPMOST, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, SWP_NOZORDER | SWP_FRAMECHANGED);
        m_bPreview = TRUE;
    }
}

void ToggleShowStatusBar(HWND hWnd)
{
    if (m_bShowStatusBar) {
        ShowWindow(GetDlgItem(hWnd, IDC_STATUSBAR), SW_HIDE);
        CheckMenuItem(GetMenu(hWnd), IDM_SHOWSTATUSBAR, MF_BYCOMMAND | MFS_UNCHECKED);
        m_bShowStatusBar = FALSE;
    }
    else {
        ShowWindow(GetDlgItem(hWnd, IDC_STATUSBAR), SW_SHOWNA);
        CheckMenuItem(GetMenu(hWnd), IDM_SHOWSTATUSBAR, MF_BYCOMMAND | MFS_CHECKED);
        m_bShowStatusBar = TRUE;
    }
}

void ToggleShowGridLine(HWND hWnd)
{
    if (m_bShowGridLine) {
        InvalidateRect(hWnd, NULL, FALSE);
        CheckMenuItem(GetMenu(hWnd), IDM_SHOWGRIDLINE, MF_BYCOMMAND | MFS_UNCHECKED);
        m_bShowGridLine = FALSE;
    }
    else {
        InvalidateRect(hWnd, NULL, FALSE);
        CheckMenuItem(GetMenu(hWnd), IDM_SHOWGRIDLINE, MF_BYCOMMAND | MFS_CHECKED);
        m_bShowGridLine = TRUE;
    }
}

HRESULT CreateGraphicsResources(HWND hWnd)
{
    HRESULT hr = S_OK;
    if (pRenderTarget == NULL)
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
        hr = pFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hWnd, size),
            &pRenderTarget);
        if (SUCCEEDED(hr))
        {
            hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.8f, 0.8f, 0.8f), &pGirdBrush);
        }
    }
    return hr;
}

void DiscardGraphicsResources()
{
    SafeRelease(&pRenderTarget);
    SafeRelease(&pGirdBrush);
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
    static KyObjectList* m_pObjectList;
    static HWND hStatus;
    switch (message)
    {
    case WM_CREATE:
        {
            INITCOMMONCONTROLSEX iccex;
            iccex.dwICC = ICC_BAR_CLASSES;
            iccex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            InitCommonControlsEx(&iccex);
            hStatus = CreateWindow(
                STATUSCLASSNAME,
                0,
                WS_CHILD | SBARS_SIZEGRIP | CCS_BOTTOM | (m_bShowStatusBar ? WS_VISIBLE : 0),
                0,
                0,
                0,
                0,
                hWnd,
                (HMENU)IDC_STATUSBAR,
                ((LPCREATESTRUCT)lParam)->hInstance,
                0);
            SendMessage(hStatus, SB_SETTEXT, 0 | 0, (LPARAM)TEXT("表示する文字列"));
        }
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)) || FAILED(CreateGraphicsResources(hWnd)))
        {
            return -1;
        }
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
            case IDM_SHOWGRIDLINE:
                ToggleShowGridLine(hWnd);
                break;
            case IDM_SHOWSTATUSBAR:
                ToggleShowStatusBar(hWnd);
                break;
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
    case WM_SIZE:
        if (pRenderTarget != NULL)
        {
            RECT rc;
            GetClientRect(hWnd, &rc);
            D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
            pRenderTarget->Resize(size);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        SendMessage(hStatus, message, wParam, lParam);
        break;
    case WM_PAINT:
        {
            HRESULT hr = CreateGraphicsResources(hWnd);
            if (SUCCEEDED(hr))
            {
                PAINTSTRUCT ps;
                BeginPaint(hWnd, &ps);
                pRenderTarget->BeginDraw();
                pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
                pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
                if (m_bShowGridLine) {
                    DrawGrid();
                }
                m_pObjectList->Draw();
                hr = pRenderTarget->EndDraw();
                if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
                {
                    DiscardGraphicsResources();
                }
                EndPaint(hWnd, &ps);
            }
        }
        break;
    case WM_DESTROY:
        delete m_pObjectList;
        DiscardGraphicsResources();
        SafeRelease(&pFactory);
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
                    wsprintf(szWithVersion, TEXT("%s %d.%d.%d.%d"), szStaticText, HIWORD(pFileInfo->dwFileVersionMS), LOWORD(pFileInfo->dwFileVersionMS), HIWORD(pFileInfo->dwFileVersionLS), LOWORD(pFileInfo->dwFileVersionLS));
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
