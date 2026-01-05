// GrabDemoDlg.cpp : implementation file
//

#include "stdafx.h"
#include "GrabDemo.h"
#include "GrabDemoDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
    CAboutDlg();

    // Dialog Data
    //{{AFX_DATA(CAboutDlg)
    enum { IDD = IDD_ABOUTBOX };
    //}}AFX_DATA

    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CAboutDlg)
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

    // Implementation
protected:
    //{{AFX_MSG(CAboutDlg)
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
    //{{AFX_DATA_INIT(CAboutDlg)
    //}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CAboutDlg)
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
    //{{AFX_MSG_MAP(CAboutDlg)
    // No message handlers
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGrabDemoDlg dialog

CGrabDemoDlg::CGrabDemoDlg(CWnd* pParent /*=NULL*/)
    : CDialog(CGrabDemoDlg::IDD, pParent)
{
    //{{AFX_DATA_INIT(CGrabDemoDlg)
    // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT
    // Note that LoadIcon does not require a subsequent DestroyIcon in Win32
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    m_Acq = NULL;
    m_Buffers = NULL;
    m_Xfer = NULL;
    m_View = NULL;

    m_IsSignalDetected = TRUE;

    // 【新增初始化】
    m_fpRaw = NULL;
    m_bIsRecording = FALSE;
    m_nFramesRecorded = 0;
}

void CGrabDemoDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CGrabDemoDlg)
    DDX_Control(pDX, IDC_STATUS, m_statusWnd);
    DDX_Control(pDX, IDC_VIEW_WND, m_ImageWnd);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CGrabDemoDlg, CDialog)
    //{{AFX_MSG_MAP(CGrabDemoDlg)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_SNAP, OnSnap)
    ON_BN_CLICKED(IDC_GRAB, OnGrab)
    ON_BN_CLICKED(IDC_FREEZE, OnFreeze)
    ON_BN_CLICKED(IDC_GENERAL_OPTIONS, OnGeneralOptions)
    ON_BN_CLICKED(IDC_AREA_SCAN_OPTIONS, OnAreaScanOptions)
    ON_BN_CLICKED(IDC_LINE_SCAN_OPTIONS, OnLineScanOptions)
    ON_BN_CLICKED(IDC_COMPOSITE_OPTIONS, OnCompositeOptions)
    ON_BN_CLICKED(IDC_LOAD_ACQ_CONFIG, OnLoadAcqConfig)
    ON_BN_CLICKED(IDC_IMAGE_FILTER_OPTIONS, OnImageFilterOptions)
    ON_BN_CLICKED(IDC_BUFFER_OPTIONS, OnBufferOptions)
    ON_BN_CLICKED(IDC_VIEW_OPTIONS, OnViewOptions)
    ON_BN_CLICKED(IDC_FILE_LOAD, OnFileLoad)
    ON_BN_CLICKED(IDC_FILE_NEW, OnFileNew)
    ON_BN_CLICKED(IDC_FILE_SAVE, OnFileSave)
    ON_BN_CLICKED(IDC_EXIT, OnExit)
    ON_WM_ENDSESSION()
    ON_WM_QUERYENDSESSION()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CGrabDemoDlg message handlers

void CGrabDemoDlg::XferCallback(SapXferCallbackInfo* pInfo)
{
    CGrabDemoDlg* pDlg = (CGrabDemoDlg*)pInfo->GetContext();

    // If grabbing in trash buffer, do not display the image, update the
    // appropriate number of frames on the status bar instead
    if (pInfo->IsTrash())
    {
        CString str;
        str.Format(_T("Frames acquired in trash buffer: %d"), pInfo->GetEventCount());
        pDlg->m_statusWnd.SetWindowText(str);
    }

    // Refresh view
    else
    {
        pDlg->m_View->Show();
        // 【新增：高速写入硬盘逻辑】
        if (pDlg->m_bIsRecording && pDlg->m_fpRaw)
        {
            // 1. 获取图像尺寸 (宽 * 高 * 字节数)
            int width = pDlg->m_Buffers->GetWidth();
            int height = pDlg->m_Buffers->GetHeight();
            int bytesPerPixel = pDlg->m_Buffers->GetBytesPerPixel();
            int size = width * height * bytesPerPixel;

            // 2. 获取当前帧的数据指针
            void* pData = NULL;
            int bufIndex = pDlg->m_Buffers->GetIndex(); // 获取当前最新的缓冲区索引
            pDlg->m_Buffers->GetAddress(bufIndex, &pData);

            // 3. 写入硬盘
            if (pData != NULL)
            {
                size_t written = fwrite(pData, 1, size, pDlg->m_fpRaw);
                if (written == size)
                {
                    pDlg->m_nFramesRecorded++;

                    // (可选) 更新状态栏显示录制进度
                    // 注意：频繁更新界面会降低性能，这里每100帧更新一次
                    if (pDlg->m_nFramesRecorded % 100 == 0)
                    {
                        CString strStatus;
                        strStatus.Format(_T("Recording... %d frames"), pDlg->m_nFramesRecorded);
                        pDlg->m_statusWnd.SetWindowText(strStatus);
                    }
                }
            }
        }
    }
}

void CGrabDemoDlg::SignalCallback(SapAcqCallbackInfo* pInfo)
{
    CGrabDemoDlg* pDlg = (CGrabDemoDlg*)pInfo->GetContext();
    pDlg->GetSignalStatus(pInfo->GetSignalStatus());
}

void CGrabDemoDlg::PixelChanged(int x, int y)
{
    CString str = m_appTitle;
    str += "  " + m_ImageWnd.GetPixelString(CPoint(x, y));
    SetWindowText(str);
}

//***********************************************************************************
// Initialize Demo Dialog based application
//***********************************************************************************
BOOL CGrabDemoDlg::OnInitDialog()
{
    CRect rect;

    CDialog::OnInitDialog();

    // Add "About..." menu item to system menu.

    // IDM_ABOUTBOX must be in the system command range.
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL)
    {
        CString strAboutMenu;
        strAboutMenu.LoadString(IDS_ABOUTBOX);
        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }

        pSysMenu->EnableMenuItem(SC_MAXIMIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
        pSysMenu->EnableMenuItem(SC_SIZE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
    }

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, FALSE);	// Set small icon
    SetIcon(m_hIcon, TRUE);		// Set big icon

    // Initialize variables
    GetWindowText(m_appTitle);

    // Are we operating on-line?
    CAcqConfigDlg dlg(this, NULL);
    if (dlg.DoModal() == IDOK)
    {
        // Define on-line objects
        m_Acq = new SapAcquisition(dlg.GetAcquisition());
        m_Buffers = new SapBufferWithTrash(2000, m_Acq);
        m_Xfer = new SapAcqToBuf(m_Acq, m_Buffers, XferCallback, this);
    }
    else
    {
        // Define off-line objects
        m_Buffers = new SapBuffer();
    }

    // Define other objects
    m_View = new SapView(m_Buffers);

    // Attach sapview to image viewer
    m_ImageWnd.AttachSapView(m_View);

    // Create all objects
    if (!CreateObjects()) { EndDialog(TRUE); return FALSE; }

    m_ImageWnd.AttachEventHandler(this);
    m_ImageWnd.CenterImage(true);
    m_ImageWnd.Reset();

    UpdateMenu();

    // Get current input signal connection status
    GetSignalStatus();

    return TRUE;  // return TRUE  unless you set the focus to a control
}

BOOL CGrabDemoDlg::CreateObjects()
{
    CWaitCursor wait;

    // Create acquisition object
    if (m_Acq && !*m_Acq && !m_Acq->Create())
    {
        DestroyObjects();
        return FALSE;
    }

    // Create buffer object
    if (m_Buffers && !*m_Buffers)
    {
        if (!m_Buffers->Create())
        {
            DestroyObjects();
            return FALSE;
        }
        // Clear all buffers
        m_Buffers->Clear();
    }

    // Create view object
    if (m_View && !*m_View && !m_View->Create())
    {
        DestroyObjects();
        return FALSE;
    }

    // Create transfer object
    if (m_Xfer && !*m_Xfer && !m_Xfer->Create())
    {
        DestroyObjects();
        return FALSE;
    }

    return TRUE;
}

BOOL CGrabDemoDlg::DestroyObjects()
{
    // Destroy transfer object
    if (m_Xfer && *m_Xfer) m_Xfer->Destroy();

    // Destroy view object
    if (m_View && *m_View) m_View->Destroy();

    // Destroy buffer object
    if (m_Buffers && *m_Buffers) m_Buffers->Destroy();

    // Destroy acquisition object
    if (m_Acq && *m_Acq) m_Acq->Destroy();

    return TRUE;
}

//**********************************************************************************
//
//				Window related functions
//
//**********************************************************************************
void CGrabDemoDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else
    {
        CDialog::OnSysCommand(nID, lParam);
    }
}


// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.
void CGrabDemoDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, (WPARAM)dc.GetSafeHdc(), 0);

        // Center icon in client rectangle
        INT32 cxIcon = GetSystemMetrics(SM_CXICON);
        INT32 cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        INT32 x = (rect.Width() - cxIcon + 1) / 2;
        INT32 y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialog::OnPaint();
    }
}

void CGrabDemoDlg::OnDestroy()
{
    CDialog::OnDestroy();

    // 【新增：安全关闭文件】
    if (m_fpRaw)
    {
        fclose(m_fpRaw);
        m_fpRaw = NULL;
    }

    // Destroy all objects
    DestroyObjects();

    // Delete all objects
    if (m_Xfer)			delete m_Xfer;
    if (m_View)			delete m_View;
    if (m_Buffers)		delete m_Buffers;
    if (m_Acq)			delete m_Acq;
}

void CGrabDemoDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);

    CRect rClient;
    GetClientRect(rClient);

    // resize image viewer
    if (m_ImageWnd.GetSafeHwnd())
    {
        CRect rWnd;
        m_ImageWnd.GetWindowRect(rWnd);
        ScreenToClient(rWnd);
        rWnd.right = rClient.right - 5;
        rWnd.bottom = rClient.bottom - 5;
        m_ImageWnd.MoveWindow(rWnd);
    }
}


// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CGrabDemoDlg::OnQueryDragIcon()
{
    return (HCURSOR)m_hIcon;
}


void CGrabDemoDlg::OnExit()
{
    EndDialog(TRUE);
}

void CGrabDemoDlg::OnEndSession(BOOL bEnding)
{
    CDialog::OnEndSession(bEnding);

    if (bEnding)
    {
        // If ending the session, free the resources.
        OnDestroy();
    }
}

BOOL CGrabDemoDlg::OnQueryEndSession()
{
    if (!CDialog::OnQueryEndSession())
        return FALSE;

    return TRUE;
}

//**************************************************************************************
// Updates the menu items enabling/disabling the proper items depending on the state
//  of the application
//**************************************************************************************
void CGrabDemoDlg::UpdateMenu(void)
{
    BOOL bAcqNoGrab = m_Xfer && *m_Xfer && !m_Xfer->IsGrabbing();
    BOOL bAcqGrab = m_Xfer && *m_Xfer && m_Xfer->IsGrabbing();
    BOOL bNoGrab = !m_Xfer || !m_Xfer->IsGrabbing();
    INT32	 scan = 0;
    BOOL bLineScan = m_Acq && m_Acq->GetParameter(CORACQ_PRM_SCAN, &scan) && (scan == CORACQ_VAL_SCAN_LINE);
    INT32 iInterface = CORACQ_VAL_INTERFACE_DIGITAL;
    if (m_Acq)
        m_Acq->GetCapability(CORACQ_CAP_INTERFACE, (void*)&iInterface);

    // Acquisition Control
    GetDlgItem(IDC_GRAB)->EnableWindow(bAcqNoGrab);
    GetDlgItem(IDC_SNAP)->EnableWindow(bAcqNoGrab);
    GetDlgItem(IDC_FREEZE)->EnableWindow(bAcqGrab);

    // Acquisition Options
    GetDlgItem(IDC_GENERAL_OPTIONS)->EnableWindow(bAcqNoGrab);
    GetDlgItem(IDC_AREA_SCAN_OPTIONS)->EnableWindow(bAcqNoGrab && !bLineScan);
    GetDlgItem(IDC_LINE_SCAN_OPTIONS)->EnableWindow(bAcqNoGrab && bLineScan);
    GetDlgItem(IDC_COMPOSITE_OPTIONS)->EnableWindow(bAcqNoGrab && (iInterface == CORACQ_VAL_INTERFACE_ANALOG));
    GetDlgItem(IDC_LOAD_ACQ_CONFIG)->EnableWindow(m_Xfer && !m_Xfer->IsGrabbing());

    // File Options
    GetDlgItem(IDC_FILE_NEW)->EnableWindow(bNoGrab);
    GetDlgItem(IDC_FILE_LOAD)->EnableWindow(bNoGrab);
    GetDlgItem(IDC_FILE_SAVE)->EnableWindow(bNoGrab);

    // Image filter Options
    GetDlgItem(IDC_IMAGE_FILTER_OPTIONS)->EnableWindow(bAcqNoGrab && m_Acq && *m_Acq && m_Acq->IsImageFilterAvailable());

    // General Options
    GetDlgItem(IDC_BUFFER_OPTIONS)->EnableWindow(bNoGrab);

    // If last control was disabled, set default focus
    if (!GetFocus())
        GetDlgItem(IDC_EXIT)->SetFocus();
}


//*****************************************************************************************
//
//					Acquisition Control
//
//*****************************************************************************************

void CGrabDemoDlg::OnFreeze()
{
    if (m_Xfer->Freeze())
    {
        if (CAbortDlg(this, m_Xfer).DoModal() != IDOK)
            m_Xfer->Abort();

        UpdateMenu();
    }

    // 【新增：关闭文件】
    if (m_fpRaw)
    {
        fclose(m_fpRaw);
        m_fpRaw = NULL;

        CString strMsg;
        strMsg.Format(_T("录制完成！共写入 %d 帧到 D:\\StreamTest.raw"), m_nFramesRecorded);
        AfxMessageBox(strMsg);
    }
    m_bIsRecording = FALSE;
}

void CGrabDemoDlg::OnGrab()
{
    m_statusWnd.SetWindowText(_T(""));

    // =========================================================
    // 【新增功能：弹出文件选择对话框】
    // =========================================================

    // 1. 定义对话框
    // FALSE: 表示这是"保存(Save)"对话框，不是"打开(Open)"
    // _T(".raw"): 默认文件后缀
    // _T("StreamTest.raw"): 默认文件名
    CFileDialog dlg(FALSE, _T(".raw"), _T("StreamTest.raw"),
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
        _T("Raw Files (*.raw)|*.raw|All Files (*.*)|*.*||"), this);

    // 2. 显示对话框，等待用户选择
    if (dlg.DoModal() == IDOK)
    {
        // 获取用户选择的完整路径（例如 E:\MyData\Test01.raw）
        CString filePath = dlg.GetPathName();

        // 3. 打开文件
        // 使用 _tfopen 宏可以自动处理 Unicode 路径问题
        m_fpRaw = _tfopen(filePath, _T("wb"));

        if (m_fpRaw != NULL)
        {
            m_bIsRecording = TRUE;
            m_nFramesRecorded = 0;

            // 提示用户文件已创建
            CString strMsg;
            strMsg.Format(_T("即将写入: %s"), filePath);
            m_statusWnd.SetWindowText(strMsg);
        }
        else
        {
            // 如果文件创建失败（比如盘符不存在或没权限）
            AfxMessageBox(_T("无法创建文件！请检查路径或磁盘权限。"));
            m_bIsRecording = FALSE;
            return; // 直接返回，不开始采集
        }
    }
    else
    {
        // 如果用户点击了“取消”，则什么都不做，直接返回
        return;
    }

    // =========================================================
    // 4. 开始采集 (和之前一样)
    // =========================================================
    if (m_Xfer->Grab())
    {
        UpdateMenu();
    }
}

void CGrabDemoDlg::OnSnap()
{
    m_statusWnd.SetWindowText(_T(""));

    if (m_Xfer->Snap())
    {
        if (CAbortDlg(this, m_Xfer).DoModal() != IDOK)
            m_Xfer->Abort();

        UpdateMenu();
    }
}


//*****************************************************************************************
//
//					Acquisition Options
//
//*****************************************************************************************

void CGrabDemoDlg::OnGeneralOptions()
{
    CAcqDlg dlg(this, m_Acq);
    dlg.DoModal();
}

void CGrabDemoDlg::OnAreaScanOptions()
{
    CAScanDlg dlg(this, m_Acq);
    dlg.DoModal();
}

void CGrabDemoDlg::OnLineScanOptions()
{
    CLScanDlg dlg(this, m_Acq);
    dlg.DoModal();
}

void CGrabDemoDlg::OnCompositeOptions()
{
    if (m_Xfer->Snap())
    {
        CCompDlg dlg(this, m_Acq, m_Xfer);
        dlg.DoModal();

        UpdateMenu();
    }
}

void CGrabDemoDlg::OnLoadAcqConfig()
{
    // Set acquisition parameters
    CAcqConfigDlg dlg(this, m_Acq);
    if (dlg.DoModal() == IDOK)
    {
        // Destroy objects
        DestroyObjects();

        // Update acquisition object
        SapAcquisition acq = *m_Acq;
        *m_Acq = dlg.GetAcquisition();

        // Recreate objects
        if (!CreateObjects())
        {
            *m_Acq = acq;
            CreateObjects();
        }

        GetSignalStatus();

        m_ImageWnd.Reset();
        InvalidateRect(NULL);
        UpdateWindow();
        UpdateMenu();
    }
}

void CGrabDemoDlg::OnImageFilterOptions()
{
    CImageFilterEditorDlg dlg(m_Acq);
    dlg.DoModal();

}

//*****************************************************************************************
//
//					General Options
//
//*****************************************************************************************

void CGrabDemoDlg::OnBufferOptions()
{
    CBufDlg dlg(this, m_Buffers, m_View->GetDisplay());
    if (dlg.DoModal() == IDOK)
    {
        // Destroy objects
        DestroyObjects();

        // Update buffer object
        SapBuffer buf = *m_Buffers;
        *m_Buffers = dlg.GetBuffer();

        // Recreate objects
        if (!CreateObjects())
        {
            *m_Buffers = buf;
            CreateObjects();
        }

        m_ImageWnd.Reset();
        InvalidateRect(NULL);
        UpdateWindow();
        UpdateMenu();
    }
}

void CGrabDemoDlg::OnViewOptions()
{
    CViewDlg dlg(this, m_View);
    if (dlg.DoModal() == IDOK)
        m_ImageWnd.Refresh();
}

//*****************************************************************************************
//
//					File Options
//
//*****************************************************************************************

void CGrabDemoDlg::OnFileNew()
{
    m_Buffers->Clear();
    InvalidateRect(NULL, FALSE);
}

void CGrabDemoDlg::OnFileLoad()
{
    CLoadSaveDlg dlg(this, m_Buffers, TRUE);
    if (dlg.DoModal() == IDOK)
    {
        InvalidateRect(NULL);
        UpdateWindow();
    }
}

void CGrabDemoDlg::OnFileSave()
{
    CLoadSaveDlg dlg(this, m_Buffers, FALSE);
    dlg.DoModal();
}

void CGrabDemoDlg::GetSignalStatus()
{
    SapAcquisition::SignalStatus signalStatus;

    if (m_Acq && m_Acq->IsSignalStatusAvailable())
    {
        if (m_Acq->GetSignalStatus(&signalStatus, SignalCallback, this))
            GetSignalStatus(signalStatus);
    }
}

void CGrabDemoDlg::GetSignalStatus(SapAcquisition::SignalStatus signalStatus)
{
    m_IsSignalDetected = (signalStatus != SapAcquisition::SignalNone);

    if (m_IsSignalDetected)
        SetWindowText(m_appTitle);
    else
    {
        CString newTitle = m_appTitle;
        newTitle += " (No camera signal detected)";
        SetWindowText(newTitle);
    }
}



