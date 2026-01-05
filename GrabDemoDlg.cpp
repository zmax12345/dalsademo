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

   m_Acq					= NULL;
   m_Buffers			= NULL;
   m_Xfer				= NULL;
   m_View            = NULL;

   m_IsSignalDetected = TRUE;

   // 【新增初始化】必须置空，否则会报错
   m_fpRaw = NULL;
   m_pBigIOBuffer = NULL;
   m_bIsRecording = FALSE;
   m_nFramesRecorded = 0;
   m_pMemPool = NULL;
   m_hWorkerThread = NULL;
   m_hFileRaw = INVALID_HANDLE_VALUE;
   InitializeCriticalSection(&m_csPool); // 初始化锁
}

CGrabDemoDlg::~CGrabDemoDlg()
{
    DeleteCriticalSection(&m_csPool); // 删除锁
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

    // 1. 检查 Trash (如果这行代码触发，说明内存拷贝都来不及了，通常不会)
    if (pInfo->IsTrash()) {
        CString str; str.Format(_T("TRASH ERROR: %d"), pInfo->GetEventCount());
        pDlg->m_statusWnd.SetWindowText(str);
        return;
    }

    if (!pDlg->m_bIsRecording) return;

    // 2. 极速拷贝 (Copy)
    EnterCriticalSection(&pDlg->m_csPool);

    // 如果池子没满，就拷贝
    if (pDlg->m_nPoolLoad < POOL_FRAME_COUNT)
    {
        void* pSrc = NULL;
        pDlg->m_Buffers->GetAddress(pDlg->m_Buffers->GetIndex(), &pSrc);
        int size = pDlg->m_Buffers->GetWidth() * pDlg->m_Buffers->GetHeight();

        // 【关键】memcpy 速度极快 (20GB/s+)，瞬间完成
        memcpy(pDlg->m_pMemPool[pDlg->m_iHead], pSrc, size);

        // 移动指针
        pDlg->m_iHead = (pDlg->m_iHead + 1) % POOL_FRAME_COUNT;
        pDlg->m_nPoolLoad++;

        // 3. 通知后台线程 "来活了"
        SetEvent(pDlg->m_hDataAvailableEvent);
    }
    else
    {
        // 池子满了！说明 SSD 写太慢，内存已经爆了
        // 这里我们选择丢弃这一帧，保命要紧
        // 实际上这里就是你看到的 "十几秒后开始丢帧" 的软件表现
    }

    LeaveCriticalSection(&pDlg->m_csPool);

    // 4. 函数结束，自动 Return
    // Sapera 采集卡此时立刻收到了 "释放" 信号，可以去采下一帧了
}

void CGrabDemoDlg::SignalCallback(SapAcqCallbackInfo *pInfo)
{
   CGrabDemoDlg *pDlg = (CGrabDemoDlg *) pInfo->GetContext();
   pDlg->GetSignalStatus(pInfo->GetSignalStatus());
}

void CGrabDemoDlg::PixelChanged(int x, int y)
{
   CString str = m_appTitle;
   str += "  " + m_ImageWnd.GetPixelString(CPoint(x, y));
   SetWindowText(str);
}

DWORD WINAPI CGrabDemoDlg::WriteThreadEntry(LPVOID pParam)
{
    ((CGrabDemoDlg*)pParam)->WriteThreadLoop();
    return 0;
}

void CGrabDemoDlg::WriteThreadLoop()
{
    // 获取单帧大小
    int frameSize = m_Buffers->GetWidth() * m_Buffers->GetHeight();
    DWORD dwWritten;

    while (WaitForSingleObject(m_hStopEvent, 0) != WAIT_OBJECT_0)
    {
        // 等待有数据，或者 1秒超时
        WaitForSingleObject(m_hDataAvailableEvent, 1000);

        while (true)
        {
            BYTE* pDataToWrite = NULL;

            // 取出一帧数据
            EnterCriticalSection(&m_csPool);
            if (m_nPoolLoad > 0)
            {
                pDataToWrite = m_pMemPool[m_iTail];
            }
            LeaveCriticalSection(&m_csPool);

            if (pDataToWrite == NULL) break; // 没数据了，休息去

            // 【核心】写入硬盘
            // 这里的卡顿完全不影响 XferCallback
            // 我们要求写入必须对齐 (Sector Aligned)，因为用了 NO_BUFFERING
            // 但 WriteFile 会自动处理大部分对齐，只要 Buffer 是 VirtualAlloc 的
            WriteFile(m_hFileRaw, pDataToWrite, frameSize, &dwWritten, NULL);

            // 更新状态
            EnterCriticalSection(&m_csPool);
            m_iTail = (m_iTail + 1) % POOL_FRAME_COUNT;
            m_nPoolLoad--;
            m_nFramesRecorded++;
            LeaveCriticalSection(&m_csPool);
        }
    }
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
   {
       m_Acq = new SapAcquisition(dlg.GetAcquisition());

       // 采集卡Buffer：只需要很小，因为我们马上就会拷贝走
       m_Buffers = new SapBufferWithTrash(50, m_Acq);
       m_Xfer = new SapAcqToBuf(m_Acq, m_Buffers, XferCallback, this);

       // =================================================
       // 【核心】申请 22GB 的用户态内存池
       // =================================================
       int width = m_Buffers->GetWidth();
       int height = m_Buffers->GetHeight();
       int frameSize = width * height; // 8-bit

       m_pMemPool = new BYTE * [POOL_FRAME_COUNT];
       for (int i = 0; i < POOL_FRAME_COUNT; i++)
       {
           // VirtualAlloc 比 new 更适合大内存申请，且内存对齐
           m_pMemPool[i] = (BYTE*)VirtualAlloc(NULL, frameSize, MEM_COMMIT, PAGE_READWRITE);
       }

       m_iHead = 0;
       m_iTail = 0;
       m_nPoolLoad = 0;
   }
   else
   {
       // Off-line objects
       m_Buffers = new SapBuffer();
   }

   // Define other objects
   m_View = new SapView( m_Buffers);

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
      if( !m_Buffers->Create())
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
   if(( nID & 0xFFF0) == IDM_ABOUTBOX)
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
   if( IsIconic())
   {
      CPaintDC dc(this); // device context for painting

      SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

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

    // 安全清理
    if (m_fpRaw)
    {
        fclose(m_fpRaw);
        m_fpRaw = NULL;
    }
    if (m_pBigIOBuffer)
    {
        delete[] m_pBigIOBuffer;
        m_pBigIOBuffer = NULL;
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
   return (HCURSOR) m_hIcon;
}


void CGrabDemoDlg::OnExit() 
{
   EndDialog(TRUE);
}

void CGrabDemoDlg::OnEndSession(BOOL bEnding)
{
   CDialog::OnEndSession(bEnding);

   if( bEnding)
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
void CGrabDemoDlg::UpdateMenu( void)
{
   BOOL bAcqNoGrab	= m_Xfer && *m_Xfer && !m_Xfer->IsGrabbing();
   BOOL bAcqGrab		= m_Xfer && *m_Xfer && m_Xfer->IsGrabbing();
   BOOL bNoGrab		= !m_Xfer || !m_Xfer->IsGrabbing();
   INT32	 scan = 0;
   BOOL bLineScan    = m_Acq && m_Acq->GetParameter(CORACQ_PRM_SCAN, &scan) && (scan == CORACQ_VAL_SCAN_LINE);
   INT32 iInterface = CORACQ_VAL_INTERFACE_DIGITAL;
   if (m_Acq)
      m_Acq->GetCapability(CORACQ_CAP_INTERFACE, (void *) &iInterface);

   // Acquisition Control
   GetDlgItem(IDC_GRAB)->EnableWindow(bAcqNoGrab);
   GetDlgItem(IDC_SNAP)->EnableWindow(bAcqNoGrab);
   GetDlgItem(IDC_FREEZE)->EnableWindow(bAcqGrab);

   // Acquisition Options
   GetDlgItem(IDC_GENERAL_OPTIONS)->EnableWindow(bAcqNoGrab);
   GetDlgItem(IDC_AREA_SCAN_OPTIONS)->EnableWindow(bAcqNoGrab && !bLineScan);
   GetDlgItem(IDC_LINE_SCAN_OPTIONS)->EnableWindow(bAcqNoGrab && bLineScan);
   GetDlgItem(IDC_COMPOSITE_OPTIONS)->EnableWindow(bAcqNoGrab && (iInterface == CORACQ_VAL_INTERFACE_ANALOG) );
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
    m_Xfer->Freeze(); // 停止采集

    // 停止线程
    m_bIsRecording = FALSE;
    SetEvent(m_hStopEvent);
    WaitForSingleObject(m_hWorkerThread, INFINITE); // 等待线程写完

    CloseHandle(m_hWorkerThread);
    CloseHandle(m_hStopEvent);
    CloseHandle(m_hDataAvailableEvent);

    // 关闭文件
    CloseHandle(m_hFileRaw);
    m_hFileRaw = INVALID_HANDLE_VALUE;

    CString str;
    str.Format(_T("结束。已写入: %lld 帧。剩余未写: %d"), m_nFramesRecorded, m_nPoolLoad);
    AfxMessageBox(str);
}

void CGrabDemoDlg::OnGrab()
{
    CFileDialog dlg(FALSE, _T(".raw"), _T("FastStream.raw"),
        OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T("Raw Files|*.raw||"), this);
    if (dlg.DoModal() != IDOK) return;

    // 1. 打开文件 (使用 Windows API 直接 IO，绕过系统缓存)
    // FILE_FLAG_NO_BUFFERING: 不使用OS缓存，直接灌入SSD
    // FILE_FLAG_WRITE_THROUGH: 确保写入到位
    m_hFileRaw = CreateFile(dlg.GetPathName(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);

    if (m_hFileRaw == INVALID_HANDLE_VALUE) {
        AfxMessageBox(_T("创建文件失败！")); return;
    }

    // 2. 启动后台写盘线程
    m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hDataAvailableEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    m_hWorkerThread = CreateThread(NULL, 0, WriteThreadEntry, this, 0, NULL);

    // 3. 重置池子状态
    m_iHead = 0; m_iTail = 0; m_nPoolLoad = 0;
    m_nFramesRecorded = 0;
    m_bIsRecording = TRUE;

    m_Xfer->Grab();
}

void CGrabDemoDlg::OnSnap() 
{
   m_statusWnd.SetWindowText(_T(""));

   if( m_Xfer->Snap())
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
   if( m_Xfer->Snap())
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
   if( dlg.DoModal() == IDOK)
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
   InvalidateRect( NULL, FALSE);
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




