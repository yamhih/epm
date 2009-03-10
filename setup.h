// generated by Fast Light User Interface Designer (fluid) version 1.0109

#ifndef setup_h
#define setup_h
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include "gui-common.h"
#include "setup2.h"
extern Fl_Double_Window *SetupWindow;
#include <FL/Fl_Box.H>
extern Fl_Box *Title[6];
#include <FL/Fl_Wizard.H>
extern Fl_Wizard *Wizard;
#include <FL/Fl_Group.H>
#include <FL/Fl_Help_View.H>
extern Fl_Help_View *ReadmeFile;
#include <FL/Fl_Round_Button.H>
extern void type_cb(Fl_Round_Button*, void*);
extern Fl_Round_Button *TypeButton[8];
#include <FL/Fl_Check_Browser.H>
extern void list_cb(Fl_Check_Browser*, void*);
extern Fl_Check_Browser *SoftwareList;
extern Fl_Box *SoftwareSize;
#include <FL/Fl_Button.H>
extern Fl_Button *InstallAllButton;
extern Fl_Button *InstallNoneButton;
#include <FL/Fl_Browser.H>
extern Fl_Browser *ConfirmList;
extern Fl_Help_View *LicenseFile;
extern Fl_Round_Button *LicenseAccept;
extern Fl_Round_Button *LicenseDecline;
extern Fl_Group *Pane[6];
#include <FL/Fl_Progress.H>
extern Fl_Progress *InstallPercent;
extern Fl_Browser *InstallLog;
extern Fl_Box *WelcomeImage;
extern Fl_Button *PrevButton;
extern void next_cb(Fl_Button*, void*);
extern Fl_Button *NextButton;
extern Fl_Button *CancelButton;
Fl_Double_Window* make_window();
#endif
