/*
	encFSGui - encfsgui.cpp
    main source

	written by Peter Van Eeckhoutte

*/

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include <wx/toolbar.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/config.h>
#include <wx/stdpaths.h>
#include <wx/log.h>
#include <wx/utils.h>
#include <vector>
#include <map>

#include "encfsgui.h"


// use bitmaps on windows, xpms on osx
#ifdef __WINDOWS__
    #define USE_XPM_BITMAPS 0
#else
    #define USE_XPM_BITMAPS 1
#endif

// keep config files local
#define wxCONFIG_USE_LOCAL_FILE 1
#define USE_LOCAL_FILE 1


// ----------------------------------------------------------------------------
// Resources 
// ----------------------------------------------------------------------------


#ifndef wxHAS_IMAGES_IN_RESOURCES
    #include "encfsgui.xpm" 
#endif

#if USE_XPM_BITMAPS
    #include "bitmaps/createfolder.xpm"
    #include "bitmaps/existingfolder.xpm"
    #include "bitmaps/browsefolder.xpm"
    #include "bitmaps/removefolder.xpm"
    #include "bitmaps/editfolder.xpm"
    #include "bitmaps/folderinfo.xpm"
    #include "bitmaps/mountfolder.xpm"
    #include "bitmaps/unmountfolder.xpm"
    #include "bitmaps/settings.xpm"
    #include "bitmaps/quit.xpm"
    #include "bitmaps/ico_ok.xpm"
    #include "bitmaps/ico_notok.xpm"
#endif // USE_XPM_BITMAPS


// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// only put toolbar at top
enum Positions
{
    TOOLBAR_TOP
};    


// globals to keep track of what we have selected in the ListCtrl
// messy, but convenient
int g_selectedIndex;
wxString g_selectedVolume;


// IDs for toolbar controls and the menu commands
enum
{
    // menu items
    ID_Menu_Quit = wxID_EXIT,

    // it is important for the id corresponding to the "About" command to have
    // this standard value as otherwise it won't be handled properly under Mac
    // (where it is special and put into the "Apple" menu)
    ID_Menu_About = wxID_ABOUT,
    // the others don't matter too much, just sequential numbers
    ID_Menu_New,
    ID_Menu_Existing,
    ID_Menu_Settings,
    //Toolbar stuff
    ID_Toolbar_Create,
    ID_Toolbar_Existing,
    ID_Toolbar_Browse,
    ID_Toolbar_Remove,
    ID_Toolbar_Edit,
    ID_Toolbar_Info,
    ID_Toolbar_Mount,
    ID_Toolbar_Unmount,
    ID_Toolbar_Settings,
    ID_Toolbar_Quit,
    ID_TOOLBAR,
    ID_List_Ctrl                   = 1000
};

// enum for return codes related with mount success
enum
{
    ID_MNT_OK,
    ID_MNT_PWDFAIL,
    ID_MNT_OTHER
};


// ----------------------------------------------------------------------------
// Classes
// ----------------------------------------------------------------------------

// DBEntry - Class for volume entry from DB

class DBEntry
{
public:
    // ctor
    DBEntry(wxString volname, 
            wxString enc_path, 
            wxString mount_path, 
            bool automount, 
            bool preventautounmount, 
            bool pwsaved);

    void setMountState(bool);
    bool getMountState();
    bool getPwSavedState();
    wxString getEncPath();
    bool getAutoMount();
    wxString getMountPath();
    wxString getVolName();
    bool getPreventAutoUnmount();

private:
    bool m_mountstate;
    bool m_automount;
    bool m_preventautounmount;
    bool m_pwsaved;
    wxString m_volname;
    wxString m_enc_path;
    wxString m_mount_path;
};


// DBENtry constructor
DBEntry::DBEntry(wxString volname, 
                 wxString enc_path, 
                 wxString mount_path, 
                 bool automount, 
                 bool preventautounmount, 
                 bool pwsaved)
{
    m_automount = automount;
    m_volname = volname;
    m_enc_path = enc_path;
    m_mount_path = mount_path;
    m_preventautounmount = preventautounmount;
    m_pwsaved = pwsaved;
}


// -----------------------------------------------
// global stuff to manage volumes
// vector of all volumes, fast lookup
std::vector<wxString> v_AllVolumes;
// map of all volumes, using volume name as key
std::map<wxString, DBEntry*> m_VolumeData;
//
// -----------------------------------------------



class mainListCtrl: public wxListCtrl
{
public:
    // ctor
    mainListCtrl(wxWindow *parent, 
                 const wxWindowID id, 
                 const wxPoint& pos, 
                 const wxSize& size, 
                 long style, 
                 wxStatusBar * statusbar);
    // event handlers
    //void OnMouseEvent(wxMouseEvent& event);
    void OnItemSelected(wxListEvent& event);
    void OnItemDeSelected(wxListEvent& event);
    void OnItemActivated(wxListEvent& event);
    void SetSelectedIndex(int);
    void LinkToolbar(wxToolBarBase*);
    void UpdateToolBarButtons();

private:
    wxDECLARE_EVENT_TABLE();
    wxToolBarBase *m_toolBar;
    wxStatusBar *m_statusBar;
};


// constructor
mainListCtrl::mainListCtrl(wxWindow *parent, 
                           const wxWindowID id, 
                           const wxPoint& pos, 
                           const wxSize& size, 
                           long style, 
                           wxStatusBar * statusbar) : wxListCtrl(parent, id, pos, size, style)
{
    m_statusBar = statusbar;   
}



//
// -----------------------------------------
//

class encFSGuiApp : public wxApp
{
public:
    bool OnInit();
};

// Define a new frame type: this is going to be our main frame
class frmMain : public wxFrame
{
public:
    // ctor(s)
    frmMain(const wxString& title, 
            const wxPoint& pos, 
            const wxSize& size, 
            long style);
	// dtor
    virtual ~frmMain();

    // event handlers (these functions should _not_ be virtual)
    void OnQuit(wxCommandEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnNewFolder(wxCommandEvent& event);
    void OnAddExistingFolder(wxCommandEvent& event);
    void OnBrowseFolder(wxCommandEvent& event);
    void OnEditFolder(wxCommandEvent& event);
    void OnSettings(wxCommandEvent& event);
    void OnMount(wxCommandEvent& event);
    void OnUnMount(wxCommandEvent& event);
    void OnInfo(wxCommandEvent& event);
    void OnRemoveFolder(wxCommandEvent& event);

    // generic routine
    bool unmountVolumeAsk(wxString& volumename);   // ask for confirmation
    // function that does actual unmount is not a member function

    int mountFolder(wxString& volumename, wxString& pw);

    // override default OnExit handler (so we can run code when user clicks close button on frame)
    virtual int OnExit(wxCommandEvent& event);

    // handle clicks on toolbar
    void OnToolLeftClick(wxCommandEvent& event);

    // auto mount routine
    void AutoMountVolumes();
    // FYI -  auto unmount routine is not a member function


private:
    wxString m_datadir;
	// toolbar stuff
    size_t              m_rows;             // 1
    wxPanel            *m_panel;

    // statusbar
#if wxUSE_STATUSBAR
    wxStatusBar* m_statusBar;
#endif

    void PopulateVolumes();
    void PopulateToolbar(wxToolBarBase* toolBar);
    void CreateToolbar();  
    void RecreateStatusbar(); 
    void RefreshAll();
    //void UpdateToolBarButtons();  // 
    void SetToolBarButtonState(int, bool);
    void DoSize();
    int mountSelectedFolder(wxString& pw);

    wxString getPassWord(wxString&, wxString&);

    // list stuff
    void RecreateList();
    // fill the control with items
    void FillListWithVolumes();
    
    // ListView stuff
    mainListCtrl *m_listCtrl;

    // any class wishing to process wxWidgets events must use this macro
    wxDECLARE_EVENT_TABLE();
};


// ----------------------------------------------------------------------------
// event tables and other macros for wxWidgets
// ----------------------------------------------------------------------------

// the event tables connect the wxWidgets events with the functions (event
// handlers) which process them. It can be also done at run-time, but for the
// simple menu events like this the static method is much simpler.
wxBEGIN_EVENT_TABLE(frmMain, wxFrame)
    EVT_MENU(ID_Menu_Quit,  frmMain::OnQuit)
    EVT_MENU(ID_Menu_About, frmMain::OnAbout)
    EVT_MENU(ID_Menu_New, frmMain::OnNewFolder)
    EVT_MENU(ID_Menu_Existing, frmMain::OnAddExistingFolder)
    EVT_MENU(ID_Menu_Settings, frmMain::OnSettings)
    EVT_MENU(wxID_ANY, frmMain::OnToolLeftClick)
wxEND_EVENT_TABLE()


// ListCTRL specific events
wxBEGIN_EVENT_TABLE(mainListCtrl, wxListCtrl)
    EVT_LIST_ITEM_SELECTED(ID_List_Ctrl, mainListCtrl::OnItemSelected)
    EVT_LIST_ITEM_DESELECTED(ID_List_Ctrl, mainListCtrl::OnItemDeSelected)
    EVT_LIST_ITEM_ACTIVATED(ID_List_Ctrl, mainListCtrl::OnItemActivated)    // double-click/enter
wxEND_EVENT_TABLE()


// Create a new application object: this macro will allow wxWidgets to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also implements the accessor function
// wxGetApp() which will return the reference of the right type (i.e. encFSGuiApp and
// not wxApp)
wxIMPLEMENT_APP(encFSGuiApp);


// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

// 'Main program' equivalent: the program execution "starts" here
bool encFSGuiApp::OnInit()
{
    // call the base class initialization method, currently it only parses a
    // few common command-line options but it could be do more in the future
    if ( !wxApp::OnInit() )
        return false;

    // init - no selection made at this point
    g_selectedIndex = -1;
    g_selectedVolume = "";

    // this will be the default config file, that we can Get() when needed
    wxConfigBase *pConfig = wxConfigBase::Create();    
    wxConfigBase::Set(pConfig);
   
    // create the main application window
	wxSize frmMainSize;
	frmMainSize.Set(880,340);
	long framestyle;
	//framestyle = wxDEFAULT_FRAME_STYLE ^ wxRESIZE_BORDER | wxFRAME_EX_METAL | wxICONIZE | wxMINIMIZE; 

    framestyle = wxDEFAULT_FRAME_STYLE ^ wxRESIZE_BORDER | wxFRAME_EX_METAL;

    frmMain *frame = new frmMain("encFSGui", 
                                 wxDefaultPosition, 
                                 frmMainSize, 
                                 framestyle );

    frame->EnableCloseButton(false);
    // and show it (the frames, unlike simple controls, are not shown when
    // created initially)

    frame->Show(true);

    wxInitAllImageHandlers();
    
    // success: wxApp::OnRun() will be called which will enter the main message
    // loop and the application will run. If we returned false here, the
    // application would exit immediately.
    return true;
}

// ----------------------------------------------------------------------------
// main frame
// ----------------------------------------------------------------------------


// frame constructor, overload built-in wxFrame
frmMain::frmMain(const wxString& title, 
                 const wxPoint &pos, 
                 const wxSize &size, 
                 long style) : wxFrame(NULL, wxID_ANY, title, pos, size, style)
{
    
    wxStandardPathsBase& stdp = wxStandardPaths::Get();
    m_listCtrl = NULL;
    m_datadir = stdp.GetUserDataDir();

#if wxUSE_STATUSBAR
    m_statusBar = CreateStatusBar(2, wxSB_SUNKEN);
#endif

    // set the frame icon
    SetIcon(wxICON(encfsgui_ico));

    #if wxUSE_MENUS
        // create a menu bar
        wxMenu *fileMenu = new wxMenu;

        // the "About" item should be in the help menu
        wxMenu *helpMenu = new wxMenu;
        helpMenu->Append(ID_Menu_About, "&About\tF1", "Show about dialog");

        // create application-specific menu items
        fileMenu->Append(ID_Menu_New, "&Create a new EncFS folder\tF2","Create a new EncFS folder");
        fileMenu->Append(ID_Menu_Existing, "&Open existing EncFS folder\tF4","Open an existing encFS folder");
        fileMenu->AppendSeparator();
        fileMenu->Append(ID_Menu_Settings, "&Settings\tF6","Edit global settings");
        fileMenu->Append(ID_Menu_Quit, "E&xit\tAlt-X", "Quit this program");

        // now append the freshly created menu to the menu bar...
        wxMenuBar *menuBar = new wxMenuBar();
        menuBar->Append(fileMenu, "&File");
        menuBar->Append(helpMenu, "&Help");

        // ... and attach this menu bar to the frame
        SetMenuBar(menuBar);
    #endif // wxUSE_MENUS

    // update the StatusBar
    RecreateStatusbar();

    // Populate vector & map with volume information
    PopulateVolumes();

    m_rows = 1;
    // Create the toolbar
    CreateToolbar();
    
    // panel to be used as a container for the Toolbar
    m_panel = new wxPanel(this, wxID_ANY);

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    m_panel->SetSizer(sizer);

    // check if we need to mount volumes at startup
    AutoMountVolumes();

    // next, create the actual list control and populate it
    //long flags = wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_ALIGN_LEFT | wxLC_SMALL_ICON | wxLC_HRULES;
    long flags = wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_ALIGN_LEFT;
    m_listCtrl = new mainListCtrl(m_panel, 
                                  ID_List_Ctrl, 
                                  wxDefaultPosition, 
                                  wxDefaultSize, 
                                  flags, 
                                  m_statusBar );
    
    RecreateList();

    m_listCtrl->LinkToolbar(GetToolBar());
    m_listCtrl->UpdateToolBarButtons();

}


void frmMain::PopulateVolumes()
{
    wxConfigBase *pConfig = wxConfigBase::Get();
    pConfig->SetPath(wxT("/Config"));
    wxString mountbin = pConfig->Read(wxT("mountbin_path"), "/sbin/mount");

    // get info about already mounted volumes
    wxArrayString mount_output;
    mount_output = ArrRunCMDSync(mountbin);

    v_AllVolumes.clear();
    pConfig->SetPath(wxT("/Volumes"));
    wxString volumename;
    wxString allNames;
    long dummy;
    bool bCont = pConfig->GetFirstGroup(volumename, dummy);
    while ( bCont ) { 
        v_AllVolumes.push_back(volumename); 
        bCont = pConfig->GetNextGroup(volumename, dummy);
    }

    for (unsigned int i = 0; i < v_AllVolumes.size(); i++)
    {
        wxString currentPath;
        wxString enc_path;
        wxString mount_path;
        bool automount;
        bool alreadymounted;
        bool preventautounmount;
        bool pwsaved;
        wxString volumename = v_AllVolumes.at(i);
        currentPath.Printf(wxT("/Volumes/%s"), volumename);
        pConfig->SetPath(currentPath);
        enc_path = pConfig->Read(wxT("enc_path"), "");
        mount_path = pConfig->Read(wxT("mount_path"), "");
        automount = pConfig->Read(wxT("automount"), 0l);
        preventautounmount = pConfig->Read(wxT("preventautounmount"), 0l);
        alreadymounted = IsVolumeSystemMounted(mount_path, mount_output);
        pwsaved = pConfig->Read(wxT("passwordsaved"), 0l);
        if (not enc_path.IsEmpty() && not mount_path.IsEmpty())
        {
            DBEntry* thisvolume = new DBEntry(volumename, 
                                              enc_path, 
                                              mount_path, 
                                              automount, 
                                              preventautounmount, 
                                              pwsaved);
            thisvolume->setMountState(alreadymounted);
            // add to map
            m_VolumeData[volumename] = thisvolume;       
        }
    }

    // %u = unsigned int
    int nr_vols;
    nr_vols = v_AllVolumes.size();
    wxString statustxt = wxString::Format(wxT("Nr of volumes : %d"), nr_vols);
    SetStatusText(statustxt,0);
}


bool unmountVolume(wxString& volumename)
{
    DBEntry *thisvol = m_VolumeData[volumename];
    wxString mountvol = thisvol->getMountPath();
    wxString umountbin;
    wxString mountbin;
    bool beenmounted;
    mountbin = getMountBinPath();
    umountbin = getUMountBinPath();
    wxString cmd;
    cmd.Printf(wxT("'%s' '%s'"), umountbin, mountvol);
    wxString cmdoutput;
    cmdoutput = StrRunCMDSync(cmd);
    // get info about already mounted volumes
    wxArrayString mount_output;
    mount_output = ArrRunCMDSync(mountbin);
    wxString mountstr;
    mountstr = arrStrTowxStr(mount_output);
    
    beenmounted = IsVolumeSystemMounted(mountvol, mount_output);
    if (not beenmounted)
    {
        // it's gone - reset stuff
        thisvol->setMountState(false);
        return true;    // unmount success
    }
    return false;
}



void AutoUnmountVolumes()
{
    // check all volumes
    for (std::map<wxString, DBEntry*>::iterator it= m_VolumeData.begin(); it != m_VolumeData.end(); it++)
    {
        wxString volumename = it->first;
        DBEntry * thisvol = it->second;
        if (thisvol->getMountState() && !thisvol->getPreventAutoUnmount())
        {
            unmountVolume(volumename);
        }

    }
}


//
// event handlers
//

bool QuitApp(wxWindow * parent)
{
    // do we need to dismount all ?
    wxConfigBase *pConfig = wxConfigBase::Get();
    pConfig->SetPath(wxT("/Config"));
    bool autounmount;
    autounmount = pConfig->Read(wxT("autounmount"), 0l);

    wxString hdr;
    hdr.Printf(wxT("Are you sure you want to exit this program?\n"));

    wxString msg;

    if (autounmount)
    {
        msg.Printf(wxT("\n*******************\nAll mounted volumes will be automatically unmounted, except for the ones that were configured to never auto-unmount.\nPlease close all open files first! \n******************* \n"));
    }
    else
    {
        msg.Printf(wxT("\nMounted volumes will not be unmounted!\n"));
    }
    hdr << msg;

    // ask if user is sure to exit
    int res = wxMessageBox(hdr, wxT("Quit ?"), wxYES_NO, parent);
    
    if (res == wxYES)
    {
        delete wxConfigBase::Set((wxConfigBase *) NULL);
        // true is to force the frame to close

        // if autounmount, dismount volumes first
        if (autounmount)
        {
            AutoUnmountVolumes();
        }
        return true;
    }
    return false;
}

void frmMain::OnQuit(wxCommandEvent& WXUNUSED(event))
{
    if (QuitApp(this))
    {
        Close(true);
    }
}



int frmMain::OnExit(wxCommandEvent& WXUNUSED(event))
{
    if (QuitApp(this))
    {
        Close(true);
        return 1;
    }
    return 0;
}

// destructor
frmMain::~frmMain()
{
    this->Destroy();
    Close(true);
}


void frmMain::OnAbout(wxCommandEvent& WXUNUSED(event))
{

    wxString encfsbinpath = getEncFSBinPath();
    wxString msg = "";
    if (encfsbinpath.IsEmpty())
    {
        msg = "** EncFS not found. Go to Settings & set encfs path **";
    }
    else
    {
        msg = encfsbinpath;
    }

    wxStandardPathsBase& stdp = wxStandardPaths::Get();

    wxMessageBox(wxString::Format
                 (
                    "Welcome to encFSGui\n"
                    "written by Corelan GCV\n\n"
                    "Project repository:\nhttps://github.com/corelan/EncFSGui\n\n"
                    "You are running %s\n\n"
                    "EncFS used: %s\n"
                    "EncFS version: %s\n"
                    "Config Folder: %s\n",
                    wxGetOsDescription(),
                    msg,
                    getEncFSBinVersion(),
                    stdp.GetConfigDir()
                 ),
                 "About encFSGui",
                 wxOK | wxICON_INFORMATION,
                 this);
}


void frmMain::OnNewFolder(wxCommandEvent& WXUNUSED(event))
{
    createNewEncFSFolder(this);
    RefreshAll();
}


void frmMain::OnAddExistingFolder(wxCommandEvent& WXUNUSED(event))
{
    openExistingEncFSFolder(this);
    RefreshAll();
}



void frmMain::OnBrowseFolder(wxCommandEvent& WXUNUSED(event))
{
    if (not g_selectedVolume.IsEmpty())
    {
        DBEntry * thisvol = m_VolumeData[g_selectedVolume];
        wxString mountpath = thisvol->getMountPath();
        BrowseFolder(mountpath);
    }
}



// mount folder - generic routine
int frmMain::mountFolder(wxString& volumename, wxString& pw)
{
    wxString mountvol;
    wxString encvol;
    wxString buf;
    
    bool beenmounted;
    DBEntry *thisvol = m_VolumeData[volumename];
    mountvol = thisvol->getMountPath();
    encvol = thisvol->getEncPath();

    // run encfs command
    wxString cmd;
    wxString cmdoutput;
    wxString encfsbin = getEncFSBinPath();
    
    // first, create mount point if necessary
    cmd.Printf(wxT("mkdir -p '%s'"), mountvol);
    cmdoutput = StrRunCMDSync(cmd);

    // mount
    // TO DO : figure out a way to allow the use of single quotes in password (and other characters that would break the command below if not properly handled)
    cmd.Printf(wxT("sh -c \"echo '%s' | %s -v -S -o volname='%s' '%s' '%s'\""), pw, encfsbin, volumename, encvol, mountvol);

    cmdoutput = StrRunCMDSync(cmd);

    // check if mount was successful
    wxString errmsg;
    errmsg = "Error decoding volume key, password incorrect";

    //wxLogDebug(wxT("----------------------------"));
    //wxLogDebug(cmdoutput);
    //wxLogDebug(wxT("----------------------------"));
    if (cmdoutput.Find(errmsg) > -1)
    {
        return ID_MNT_PWDFAIL;
    }

    // check mount list, to be sure
    wxString mountbin = getMountBinPath();

    // get info about already mounted volumes
    wxArrayString mount_output;
    mount_output = ArrRunCMDSync(mountbin);
    wxString mountstr;
    mountstr = arrStrTowxStr(mount_output);
    
    beenmounted = IsVolumeSystemMounted(mountvol, mount_output);
    if (beenmounted)
    {
        thisvol->setMountState(true);
        return ID_MNT_OK;
    }
    return ID_MNT_OTHER;
}




// unmount folder, generic routine
bool frmMain::unmountVolumeAsk(wxString& volumename)
{
    wxString msg;
    wxString title;
    wxString mountvol;
    bool unmountok;
    unmountok = false;

    DBEntry *thisvol = m_VolumeData[volumename];
    mountvol = thisvol->getMountPath();

    msg.Printf(wxT("Are you sure you want to unmount\n'%s' ?\n\nNote: make sure to close all open files\nbefore clicking 'Yes'."),mountvol);
    title.Printf(wxT("Unmount '%s' ?"), volumename);

    wxMessageDialog * dlg = new wxMessageDialog(this, 
                                                msg, 
                                                title, 
                                                wxYES_NO|wxCENTRE|wxNO_DEFAULT|wxICON_QUESTION);
    if (dlg->ShowModal() == wxID_YES)
    {
        unmountok = unmountVolume(volumename);
    }
    dlg->Destroy();
    return unmountok; // unmount did not work, or not selected 
}




int frmMain::mountSelectedFolder(wxString& pw)
{
    wxString buf;   
    // update statustext
    wxString msg;
    msg.Printf(wxT("Mounting '%s'"), g_selectedVolume);
    PushStatusText(msg,0);

    buf.Printf(wxT("%s"), ".....");
    m_listCtrl->SetItem(g_selectedIndex, 0, buf);

    int mountstatus = mountFolder(g_selectedVolume, pw);

    if (mountstatus == ID_MNT_OK)
    {
        buf.Printf(wxT("%s"), "YES");
        wxColour itemColour = wxColour(*wxRED);
        m_listCtrl->SetItemTextColour(g_selectedIndex, itemColour);
        m_listCtrl->SetItem(g_selectedIndex, 0, buf);
        SetToolBarButtonState(ID_Toolbar_Mount, false);
        SetToolBarButtonState(ID_Toolbar_Unmount, true);
        SetToolBarButtonState(ID_Toolbar_Browse, true);
        SetToolBarButtonState(ID_Toolbar_Edit, true);   // limited edits allowed
    }
    else
    {
        buf.Printf(wxT("%s"), "NO");
        wxColour itemColour = wxColour(*wxBLUE);
        m_listCtrl->SetItemTextColour(g_selectedIndex, itemColour);
        m_listCtrl->SetItem(g_selectedIndex, 0, buf);
        SetToolBarButtonState(ID_Toolbar_Unmount, false);
        SetToolBarButtonState(ID_Toolbar_Mount, true);
        SetToolBarButtonState(ID_Toolbar_Browse, false);
        SetToolBarButtonState(ID_Toolbar_Edit, true);
    }

    PopStatusText(0);
    return mountstatus;
}


void frmMain::AutoMountVolumes()
{
    // iterate over volumes
    for (std::map<wxString, DBEntry*>::iterator it= m_VolumeData.begin(); it != m_VolumeData.end(); it++)
    {
        wxString volumename = it->first;
        DBEntry * thisvol = it->second;
        wxString mountvol = thisvol->getMountPath();
        wxString encvol = thisvol->getEncPath();
        wxString title;
        title.Printf(wxT("Automount '%s'"), volumename);
        if ((not thisvol->getMountState()) && (thisvol->getAutoMount()) )
        {
            bool trymount = true;
            wxString extratxt = "";
            while (trymount)
            {
                wxString msg;
                msg.Printf(wxT("%sPlease enter password to auto-mount\n'%s'\nas\n'%s'"), extratxt, encvol, mountvol);
                wxString pw;
                if (thisvol->getPwSavedState())
                {
                    pw = getKeychainPassword(volumename);
                }
                else
                {
                    pw = getPassWord(title, msg);
                }
                 
                
                if (!pw.IsEmpty())
                {
                     // try mount
                    int mountstatus = mountFolder(volumename, pw);
                    // to do : instead of setting pw to a new value, clear out memory location directly 
                    pw = "GoodLuckWithThat";

                    if (mountstatus == ID_MNT_PWDFAIL)
                    {
                        extratxt.Printf(wxT("** You have entered an invalid password **\n\n"));
                    }
                    else if (mountstatus == ID_MNT_OK)
                    {
                        trymount = false;
                    }
                    else if (mountstatus == ID_MNT_OTHER)
                    {
                        // show message and then break
                        wxString errormsg;
                        wxString errortitle;
                        errormsg.Printf(wxT("Unable to mount volume '%s'\nEncfs folder: %s\nMount path: %s"), volumename, encvol, mountvol);
                        errortitle.Printf(wxT("Error found while mounting '%s'"), volumename);
                        wxMessageDialog * dlg = new wxMessageDialog(this, 
                                                                    errormsg, 
                                                                    errortitle, 
                                                                    wxOK|wxCENTRE|wxICON_ERROR);
                        dlg->ShowModal();
                        dlg->Destroy();
                        trymount = false;
                    }   
                }
                else
                {
                    // bail out
                    trymount = false;
                }
                
            }            
        }
    }
    // mount when needed
}


void frmMain::OnUnMount(wxCommandEvent& WXUNUSED(event))
{
    bool beenunmounted;
    beenunmounted = unmountVolumeAsk(g_selectedVolume);
    if (beenunmounted)
    {
        // it's gone - reset stuff
        wxString buf;
        buf.Printf(wxT("%s"), "NO");
        wxColour itemColour = wxColour(*wxBLUE);
        m_listCtrl->SetItemTextColour(g_selectedIndex, itemColour);
        m_listCtrl->SetItem(g_selectedIndex, 0, buf);
        SetToolBarButtonState(ID_Toolbar_Mount, true);
        SetToolBarButtonState(ID_Toolbar_Unmount, false);
        SetToolBarButtonState(ID_Toolbar_Browse, false);
        SetToolBarButtonState(ID_Toolbar_Edit, true);
    }
    else
    {
        // still mounted
        SetToolBarButtonState(ID_Toolbar_Mount, false);
        SetToolBarButtonState(ID_Toolbar_Unmount, true);
        SetToolBarButtonState(ID_Toolbar_Browse, true);
        SetToolBarButtonState(ID_Toolbar_Edit, true); // limited edits allowed
    }
}

void frmMain::OnInfo(wxCommandEvent& WXUNUSED(event))
{
    // get full encfpath for this volume
    DBEntry * thisvol = m_VolumeData[g_selectedVolume];
    wxString encvol = thisvol->getEncPath();
    wxArrayString volinfo = getEncFSVolumeInfo(encvol);
    wxString msg = arrStrTowxStr(volinfo);
    wxString title;
    title.Printf(wxT("EncFS information for '%s'"), g_selectedVolume);
    wxString msgbody;
    msgbody.Printf(wxT("Encrypted path: '%s'\n\n"), encvol);
    msgbody << msg;
    
    wxMessageDialog * dlg = new wxMessageDialog(this, msgbody, title, wxOK|wxCENTRE|wxICON_INFORMATION);
    dlg->ShowModal();
    dlg->Destroy();
}


void frmMain::OnMount(wxCommandEvent& WXUNUSED(event))
{
    wxString msg;
    wxString extratxt = "";
    wxString mountvol;
    wxString encvol;
    wxString title;
    title.Printf(wxT("Enter password for '%s'"), g_selectedVolume);
    DBEntry * thisvol = m_VolumeData[g_selectedVolume];
    mountvol = thisvol->getMountPath();
    encvol = thisvol->getEncPath();
    bool trymount = true;
    while (trymount)
    {
        msg.Printf(wxT("%sPlease enter password to mount\n'%s'\nas\n'%s'"), extratxt, encvol,mountvol);
        wxString pw;
        if (thisvol->getPwSavedState())
        {
            pw = getKeychainPassword(g_selectedVolume);
        }
        else
        {
            pw = getPassWord(title, msg);   
        }

        if (!pw.IsEmpty())
        {
            // try mount
            int successmount = mountSelectedFolder(pw);
            
            pw = "GoodLuckWithThat";
            if (successmount == ID_MNT_PWDFAIL)
            {
                extratxt.Printf(wxT("** You have entered an invalid password **\n\n"));
            }
            else if (successmount == ID_MNT_OK)
            {
                trymount = false;
            }
            else if (successmount == ID_MNT_OTHER)
            {
                wxString errormsg;
                wxString errortitle;
                errormsg.Printf(wxT("Unable to mount volume '%s'\nEncfs folder: %s\nMount path: %s"), g_selectedVolume, thisvol->getEncPath(), thisvol->getMountPath());
                errortitle.Printf(wxT("Error found while mounting '%s'"), g_selectedVolume);
                wxMessageDialog * dlg = new wxMessageDialog(this, errormsg, errortitle, wxOK|wxCENTRE|wxICON_ERROR);
                dlg->ShowModal();
                dlg->Destroy();
                trymount = false;
            }
        }
        else
        {
            // bail out
            trymount = false;
        }
    }
}


wxString frmMain::getPassWord(wxString& title, wxString& prompt)
{
    wxString pw = "";
    wxPasswordEntryDialog * dlg = new wxPasswordEntryDialog(this, 
                                                            prompt, 
                                                            title , 
                                                            wxEmptyString, 
                                                            wxTextEntryDialogStyle, 
                                                            wxDefaultPosition);
    if (dlg->ShowModal() == wxID_OK)
    {
        pw = dlg->GetValue();
        return pw;
    }
    dlg->Destroy();
    return "";
}


void frmMain::OnEditFolder(wxCommandEvent& WXUNUSED(event))
{
    DBEntry * thisvol = m_VolumeData[g_selectedVolume];
    bool ismounted = thisvol->getMountState();
    editExistingEncFSFolder(this, g_selectedVolume, ismounted);
    RefreshAll();
}

void frmMain::OnRemoveFolder(wxCommandEvent& WXUNUSED(event))
{
    wxString msg;
    wxString title;
    bool deleted = false;
    title.Printf(wxT("Remove '%s' ?"), g_selectedVolume);
    msg.Printf(wxT("Are you really sure you want to remove volume '%s' from this application?\n"), g_selectedVolume);
    msg << "Notes:\n";
    msg << "1. This action will NOT remove the actual folders and/or data.  It will only cause this application to forget about this volume.\n";
    msg << "2. If you have removed a volume by mistake, you can simply add it back via 'Open existing encfs folder'.\n";
    msg << "3. Removing a mounted volume will NOT unmount it.\n";
    wxMessageDialog * dlg = new wxMessageDialog(this, 
                                                msg, 
                                                title, 
                                                wxYES_NO|wxCENTRE|wxNO_DEFAULT|wxICON_QUESTION);
    if (dlg->ShowModal() == wxID_YES)
    {
        // simply remove from config file by removing the config group
        wxConfigBase *pConfig = wxConfigBase::Get();
        wxString configgroup;
        configgroup.Printf(wxT("/Volumes/%s"), g_selectedVolume);
        pConfig->DeleteGroup(configgroup);
        deleted = true;
    }
    dlg->Destroy();
    if (deleted)
    {
        RefreshAll();
    }
}


void frmMain::OnSettings(wxCommandEvent& WXUNUSED(event))
{
    openSettings(this);
    RefreshAll();
}

void frmMain::OnToolLeftClick(wxCommandEvent& event)
{
    if (event.GetId() == ID_Toolbar_Create)
    {
        createNewEncFSFolder(this);
        RefreshAll();
    }
    else if (event.GetId() == ID_Toolbar_Existing)
    {
        openExistingEncFSFolder(this);
        RefreshAll();
    }
    else if (event.GetId() == ID_Toolbar_Settings)
    {
        openSettings(this);
        RefreshAll();
    }
    else if (event.GetId() == ID_Toolbar_Edit)
    {
        DBEntry * thisvol = m_VolumeData[g_selectedVolume];
        bool ismounted = thisvol->getMountState();
        editExistingEncFSFolder(this, g_selectedVolume, ismounted);
        RefreshAll();
    }
    else if (event.GetId() == ID_Toolbar_Quit)
    {
        OnQuit(event);
    }
    else if (event.GetId() == ID_Toolbar_Browse)
    {
        OnBrowseFolder(event);
    }
    else if (event.GetId() == ID_Toolbar_Mount)
    {
        OnMount(event);
    }
    else if (event.GetId() == ID_Toolbar_Unmount)
    {
        OnUnMount(event);
    }
    else if (event.GetId() == ID_Toolbar_Info)
    {
        OnInfo(event);
    }
    else if (event.GetId() == ID_Toolbar_Remove)
    {
        OnRemoveFolder(event);
    }
}


//
// Statusbar
//

void frmMain::RecreateStatusbar()
{
    #if wxUSE_STATUSBAR
        
        wxFont font = m_statusBar->GetFont();
        font.SetWeight(wxFONTWEIGHT_BOLD);
        m_statusBar->SetFont(font);

        // check if encfs is installed
        if (isEncFSBinInstalled())
        {
            wxString statustxt;
            statustxt.Printf("encFS ready // %s", getEncFSBinVersion());
            SetStatusText(statustxt,1);  
        }
        else
        {
            SetStatusText("** encFS not found. Open 'Settings' and set path to encfs **",1);    
        }
        
    #endif // wxUSE_STATUSBAR
}


//
// Toolbar
//


void frmMain::CreateToolbar()
{
    // delete and recreate the toolbar
    wxToolBarBase *toolBar = GetToolBar();
    static const long TOOLBAR_STYLE = wxTB_FLAT | wxTB_DOCKABLE | wxTB_TEXT;
    long style = toolBar ? toolBar->GetWindowStyle() : TOOLBAR_STYLE;
    delete toolBar; // just in case
    SetToolBar(NULL);
    toolBar = CreateToolBar(style, ID_TOOLBAR);
    PopulateToolbar(toolBar);
}

void frmMain::PopulateToolbar(wxToolBarBase* toolBar)
{
     // Set up toolbar
    enum
    {
        Tool_createfolder,
        Tool_existingfolder,
        Tool_browsefolder,
        Tool_removefolder,
        Tool_editfolder,
        Tool_folderinfo,
        Tool_mountfolder,
        Tool_unmountfolder,
        Tool_settings,
        Tool_quit,
        Tool_Max
    };


    wxBitmap toolBarBitmaps[Tool_Max];

#if USE_XPM_BITMAPS
    #define INIT_TOOL_BMP(bmp) \
        toolBarBitmaps[Tool_##bmp] = wxBitmap(bmp)
#else // !USE_XPM_BITMAPS
    #define INIT_TOOL_BMP(bmp) \
        toolBarBitmaps[Tool_##bmp] = wxBITMAP(bmp)
#endif // USE_XPM_BITMAPS/!USE_XPM_BITMAPS

    INIT_TOOL_BMP(createfolder);
    INIT_TOOL_BMP(existingfolder);
    INIT_TOOL_BMP(browsefolder);
    INIT_TOOL_BMP(removefolder);
    INIT_TOOL_BMP(editfolder);
    INIT_TOOL_BMP(folderinfo);
    INIT_TOOL_BMP(mountfolder);
    INIT_TOOL_BMP(unmountfolder);
    INIT_TOOL_BMP(settings);
    INIT_TOOL_BMP(quit);

    // get size from first bitmap
    int w = toolBarBitmaps[Tool_createfolder].GetWidth(),
        h = toolBarBitmaps[Tool_createfolder].GetHeight();

    // this call is actually unnecessary as the toolbar will adjust its tools
    // size to fit the biggest icon used anyhow but it doesn't hurt neither
    toolBar->SetToolBitmapSize(wxSize(w, h));
    

    toolBar->AddTool(ID_Toolbar_Create, wxT("Create"),
                     toolBarBitmaps[Tool_createfolder], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Create new encfs folder"), wxT("Create new encfs folder"));

    toolBar->AddTool(ID_Toolbar_Existing, wxT("Open Existing"),
                     toolBarBitmaps[Tool_existingfolder], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Open existing encfs folder"), wxT("Open existing encfs folder"));

    toolBar->AddSeparator();

    toolBar->AddTool(ID_Toolbar_Browse, wxT("Browse"),
                     toolBarBitmaps[Tool_browsefolder], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Browse mounted encfs folder"), wxT("Browse mounted encfs folder"));

    toolBar->AddTool(ID_Toolbar_Remove, wxT("Remove"),
                     toolBarBitmaps[Tool_removefolder], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Remove encfs folder"), wxT("Remove encfs folder from database"));

    toolBar->AddTool(ID_Toolbar_Info, wxT("Info"),
                     toolBarBitmaps[Tool_folderinfo], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Show info"), wxT("Show encfs related information about this folder"));


    toolBar->AddTool(ID_Toolbar_Edit, wxT("Edit"),
                     toolBarBitmaps[Tool_editfolder], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Edit encfs folder"), wxT("Edit encfs folder configuration"));

    toolBar->AddSeparator();


    toolBar->AddTool(ID_Toolbar_Mount, wxT("Mount && expose"),
                     toolBarBitmaps[Tool_mountfolder], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Mount and expose a protected encfs folder"), wxT("Mount and expose a protected encfs folder"));


    toolBar->AddTool(ID_Toolbar_Unmount, wxT("Unmount && lock"),
                     toolBarBitmaps[Tool_unmountfolder], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Unmount and protect a mounted encfs folder"), wxT("Unmount and protect a mounted encfs folder"));

    toolBar->AddSeparator();

    toolBar->AddTool(ID_Toolbar_Settings, wxT("Settings"),
                     toolBarBitmaps[Tool_settings], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Global settings"), wxT("Global settings"));

    toolBar->AddSeparator();

    toolBar->AddTool(ID_Toolbar_Quit, wxT("Quit"),
                     toolBarBitmaps[Tool_quit], wxNullBitmap, wxITEM_NORMAL,
                     wxT("Quit"), wxT("Quit this program"));


    // after adding the buttons to the toolbar, must call Realize() to reflect
    // the changes
    toolBar->Realize();

    toolBar->SetRows(toolBar->IsVertical() ? toolBar->GetToolsCount() / m_rows
                                           : m_rows);
}


void mainListCtrl::UpdateToolBarButtons()
{
    // keep certain buttons disabled when encfs is not found/installed
    bool encfsbininstalled = isEncFSBinInstalled();
    m_toolBar->EnableTool(ID_Toolbar_Create, encfsbininstalled);
    m_toolBar->EnableTool(ID_Toolbar_Existing, encfsbininstalled);
    m_toolBar->EnableTool(ID_Toolbar_Browse, encfsbininstalled);
    m_toolBar->EnableTool(ID_Toolbar_Remove, encfsbininstalled);
    m_toolBar->EnableTool(ID_Toolbar_Edit, encfsbininstalled);
    m_toolBar->EnableTool(ID_Toolbar_Info, encfsbininstalled);
    m_toolBar->EnableTool(ID_Toolbar_Mount, encfsbininstalled);
    m_toolBar->EnableTool(ID_Toolbar_Unmount, encfsbininstalled);

    if (encfsbininstalled)
    {
        if (g_selectedIndex > -1)  // a line was selected
        {
            m_toolBar->EnableTool(ID_Toolbar_Remove, true);
            m_toolBar->EnableTool(ID_Toolbar_Edit, true);
            m_toolBar->EnableTool(ID_Toolbar_Browse, true);
            m_toolBar->EnableTool(ID_Toolbar_Info, true);

            // to do - add logic to check if selected volume is mounted
            DBEntry *thisvolume = m_VolumeData[g_selectedVolume];
            if (thisvolume->getMountState())
            {
                m_toolBar->EnableTool(ID_Toolbar_Mount, false);
                m_toolBar->EnableTool(ID_Toolbar_Unmount, true);
                m_toolBar->EnableTool(ID_Toolbar_Browse, true);   
                m_toolBar->EnableTool(ID_Toolbar_Edit, true);   
            }
            else
            {
                m_toolBar->EnableTool(ID_Toolbar_Mount, true);
                m_toolBar->EnableTool(ID_Toolbar_Unmount, false); 
                m_toolBar->EnableTool(ID_Toolbar_Browse, false);
                m_toolBar->EnableTool(ID_Toolbar_Edit, true); 
            }    
        }
        else   // no selection in ListCtrl
        {
            m_toolBar->EnableTool(ID_Toolbar_Remove, false);
            m_toolBar->EnableTool(ID_Toolbar_Edit, false);
            m_toolBar->EnableTool(ID_Toolbar_Browse, false);
            m_toolBar->EnableTool(ID_Toolbar_Mount, false);
            m_toolBar->EnableTool(ID_Toolbar_Unmount, false);
            m_toolBar->EnableTool(ID_Toolbar_Info, false);    
        }    
    }

}

// in case we want to flip state of a specific button
void frmMain::SetToolBarButtonState(int ButtonID, bool newstate)
{
    wxToolBarBase *toolBar = GetToolBar();
    toolBar->EnableTool(ButtonID, newstate);
}


// ------------------------------------------------
// list
// ------------------------------------------------

void frmMain::DoSize()
{
    wxSize size = GetClientSize();
    wxCoord y = (2*size.y);
    m_listCtrl->SetSize(0, 0, size.x, y);
}

void frmMain::RecreateList()
{
    {        
        m_listCtrl->ClearAll();

        FillListWithVolumes();

        DoSize();
    }   
}

void frmMain::FillListWithVolumes()
{
    // global font settings
    wxFont font = this->GetFont();
    font.MakeSmaller();

    wxString columnHeader;

    // create columns
    
    columnHeader = "Mounted";
    m_listCtrl->AppendColumn(columnHeader);

    columnHeader = "Volume name";
    m_listCtrl->AppendColumn(columnHeader);

    columnHeader = "Encrypted folder";
    m_listCtrl->AppendColumn(columnHeader);

    columnHeader = "Mounted at";
    m_listCtrl->AppendColumn(columnHeader);

    columnHeader = "Automount";
    m_listCtrl->AppendColumn(columnHeader);


    // change Column width
    // Mounted
    m_listCtrl->SetColumnWidth(0,65);
    // Volume Name
    m_listCtrl->SetColumnWidth(1,120);
    // EncryptedFolder
    m_listCtrl->SetColumnWidth(2,300);
    // Mounted At
    m_listCtrl->SetColumnWidth(3,300);
    // Automount
    m_listCtrl->SetColumnWidth(4,70);


    
    // to speed up inserting we hide the control temporarily
    m_listCtrl->Hide();

    for (unsigned int rowindex = 0; rowindex < v_AllVolumes.size(); rowindex++)
    {
        wxString buf;
        wxString volumename;

        volumename = v_AllVolumes.at(rowindex);
        DBEntry * thisvol;
        thisvol = m_VolumeData[volumename];

        bool isMounted;
        isMounted = thisvol->getMountState();

        wxColour itemColour;
        // column[0]

        if (isMounted)
        {
            buf.Printf(wxT("%s"), "YES");
            itemColour = wxColour(*wxRED);
        }
        else
        {  
            buf.Printf(wxT("%s"), "NO"); 
            itemColour = wxColour(*wxBLUE);
        }

        long rid = m_listCtrl->InsertItem(rowindex, buf, 0);
        m_listCtrl->SetItemData(rid, rowindex);
        m_listCtrl->SetItemFont(rid, font);
        m_listCtrl->SetItemTextColour(rid, itemColour);


        // column[1]
        buf.Printf(wxT("%s"), volumename);
        m_listCtrl->SetItem(rid, 1, buf);

        // column[2]
        buf.Printf(wxT("%s"), thisvol->getEncPath());
        m_listCtrl->SetItem(rid, 2, buf);

        // column[3]
        buf.Printf(wxT("%s"), thisvol->getMountPath());
        m_listCtrl->SetItem(rid, 3, buf);

        // column[4]
        if (thisvol->getAutoMount())
        {
            buf.Printf(wxT("YES"));
        }
        else
        {
            buf.Printf(wxT("NO"));
        }
        m_listCtrl->SetItem(rid, 4, buf);
    }

    m_listCtrl->Show();
}


void frmMain::RefreshAll()
{
    PopulateVolumes();
    m_listCtrl->UpdateToolBarButtons();
    RecreateList();
    RecreateStatusbar();
}






// ----------------------------------------------------------------------------
// CDBEntry member functions
// ----------------------------------------------------------------------------


void DBEntry::setMountState(bool newstate)
{
    m_mountstate = newstate;
}

bool DBEntry::getMountState()
{
    return m_mountstate;
}

bool DBEntry::getPreventAutoUnmount()
{
    return m_preventautounmount;
}

bool DBEntry::getPwSavedState()
{
    return m_pwsaved;
}

wxString DBEntry::getEncPath()
{
    return m_enc_path;
}

bool DBEntry::getAutoMount()
{
    return m_automount;
}

wxString DBEntry::getMountPath()
{
    return m_mount_path;
}

wxString DBEntry::getVolName()
{
    return m_volname;
}

//
//  ListCtrl member functions
//

void mainListCtrl::LinkToolbar(wxToolBarBase * toolbar)
{
    m_toolBar = toolbar;
}


void mainListCtrl::SetSelectedIndex(int index)
{
    g_selectedIndex = index;
    int nr_vols;
    nr_vols = v_AllVolumes.size();
    wxString selvol;

    if (index >= 0)
    {
        g_selectedVolume = GetItemText(index,1);
        selvol.Printf(wxT("// Selected volume: %s"),g_selectedVolume);
    }
    else
    {
        g_selectedVolume = "";
        selvol = "";
    }
    wxString statustxt = wxString::Format(wxT("Nr of volumes : %d %s"), nr_vols, selvol);
    m_statusBar->SetStatusText(statustxt,0);
    UpdateToolBarButtons();
}


void mainListCtrl::OnItemSelected(wxListEvent& WXUNUSED(event))
{
    long itemIndex = -1;

    while ((itemIndex = GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND) 
    {
        // Got the selected item index
        //wxLogDebug(GetItemText(itemIndex));
        SetSelectedIndex(itemIndex);
    }
}


void mainListCtrl::OnItemDeSelected(wxListEvent& WXUNUSED(event))
{
    // this allows us to disable toolbar buttons again when no selection is active in ListCtrl
    long itemIndex = -1;
    bool somethingselected = false;

    while ((itemIndex = GetNextItem(itemIndex, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) != wxNOT_FOUND) 
    {
        somethingselected = true;
    }
    if (not somethingselected)
    {
        SetSelectedIndex(-1);
    }
}

void mainListCtrl::OnItemActivated(wxListEvent& WXUNUSED(event))
{
    if (g_selectedIndex > -1)
    {
        // if volume was mounted, browse the folder
        DBEntry * thisvol = m_VolumeData[g_selectedVolume];
        if (thisvol->getMountState())
        {
            // open
            wxString mountpath = thisvol->getMountPath();
            BrowseFolder(mountpath);
        }
        else
        {
            // edit
        }
    }
}
