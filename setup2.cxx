//
// "$Id: setup2.cxx,v 1.15.2.19 2009/10/16 12:24:14 bsavelev Exp $"
//
//   ESP Software Installation Wizard main entry for the ESP Package Manager (EPM).
//
//   Copyright 1999-2006 by Easy Software Products.
//   Copyright 2009 by Dr.Web.
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2, or (at your option)
//   any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Contents:
//
//   main()         - Main entry for software wizard...
//   get_dists()    - Get a list of available software products.
//   install_dist() - Install a distribution...
//   list_cb()      - Handle selections in the software list.
//   load_image()   - Load the setup image file (setup.gif/xpm)...
//   load_readme()  - Load the readme file...
//   load_types()   - Load the installation types from the setup.types file.
//   log_cb()       - Add one or more lines of text to the installation log.
//   next_cb()      - Show software selections or install software.
//   type_cb()      - Handle selections in the type list.
//   update_size()  - Update the total +/- sizes of the installations.
//   update_control() - updates control on form
//

//#include <iostream>
#define _DEFINE_GLOBALS_
#include "setup.h"
#include <FL/Fl_GIF_Image.H>
#include <FL/Fl_XPM_Image.H>
#include <FL/x.H>
#include <FL/filename.H>
#include <FL/fl_ask.H>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <string>
#include <list>
using namespace std;

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif // HAVE_SYS_PARAM_H

#ifdef HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
#endif // HAVE_SYS_MOUNT_H

#ifdef HAVE_SYS_STATFS_H
#  include <sys/statfs.h>
#endif // HAVE_SYS_STATFS_H

#ifdef HAVE_SYS_VFS_H
#  include <sys/vfs.h>
#endif // HAVE_SYS_VFS_H

#ifdef __osf__
// No prototype for statfs under Tru64...
extern "C" {
extern int statfs(const char *, struct statfs *);
}
#endif // __osf__

#ifdef __APPLE__
#  include <Security/Authorization.h>
#  include <Security/AuthorizationTags.h>

AuthorizationRef SetupAuthorizationRef;
#endif // __APPLE__


//LANG
#define LANG_EN	0
#define LANG_RU	1

//
// Panes...
//

#define PANE_WELCOME	0
#define PANE_TYPE	1
#define PANE_SELECT	2
#define PANE_CONFIRM	3
#define PANE_LICENSE	4
#define PANE_INSTALL	5
#define PANE_POSTIN	6

//
// Define a C API function type for comparisons...
//

extern "C" {
typedef int (*compare_func_t)(const void *, const void *);
}

//
// Local var
//

int		licaccept = 0;
FILE		*fdfile = NULL;
int		skip_pane_select = 1;
int		Verbosity = 0;
int		InstallFailed = 1;
int		CustomType = 0;

#define POSTIN_SCRIPT "./scripts/postinstall.sh"
#define POSTIN_I_SCRIPT "./scripts/postinstall-i.sh"

//
// Local functions...
//

void	get_dists(const char *d);
int	install_dist(const gui_dist_t *dist);
int	license_dist(const gui_dist_t *dist);
void	load_image(void);
void	load_readme(void);
void	load_types(void);
void	load_license(void);
void	load_more_licenses(void);
void	log_cb(int fd, int *fdptr);
void	update_sizes(void);

//
// 'main()' - Main entry for software wizard...
//

int				// O - Exit status
main(int  argc,			// I - Number of command-line arguments
     char *argv[])		// I - Command-line arguments
{
  Fl_Window	*w;		// Main window...
  const char	*distdir;	// Distribution directory


  // Use GTK+ scheme for all operating systems...
  Fl::background(230, 230, 230);
  Fl::scheme("gtk+");

  set_gettext(argv);

#ifdef __APPLE__
  // OSX passes an extra command-line option when run from the Finder.
  // If the first command-line argument is "-psn..." then skip it and use the full path
  // to the executable to figure out the distribution directory...
  if (argc > 1)
  {
    if (strncmp(argv[1], "-psn", 4) == 0)
    {
      char		*ptr;		// Pointer into basedir
      static char	basedir[1024];	// Base directory (static so it can be used below)


      strlcpy(basedir, argv[0], sizeof(basedir));
      if ((ptr = strrchr(basedir, '/')) != NULL)
        *ptr = '\0';
      if ((ptr = strrchr(basedir, '/')) != NULL && !strcasecmp(ptr, "/MacOS"))
      {
        // Got the base directory, now add "Resources" to it...
	*ptr = '\0';
	strlcat(basedir, "/Resources", sizeof(basedir));
	distdir = basedir;
	chdir(basedir);
      }
      else
        distdir = ".";

    }
    else
      distdir = argv[1];
  }
  else
    distdir = ".";
#else
  // Get the directory that has the software in it...
  if (argc > 1)
    distdir = argv[1];
  else
    distdir = ".";
#endif // __APPLE__

  w = make_window();

  Pane[PANE_WELCOME]->show();
  PrevButton->deactivate();
  NextButton->deactivate();

  gui_get_installed();
  get_dists(distdir);

  load_image();
  load_readme();
  load_types();
  load_license();
  load_more_licenses();

  w->show();

  while (!w->visible())
    Fl::wait();

#ifdef __APPLE__
  OSStatus status;

  status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
                               kAuthorizationFlagDefaults,
			       &SetupAuthorizationRef);
  if (status != errAuthorizationSuccess)
  {
    fl_alert("You must have administrative priviledges to install this software!");
    return (1);
  }

  AuthorizationItem	items = { kAuthorizationRightExecute, 0, NULL, 0 };
  AuthorizationRights	rights = { 1, &items };

  status = AuthorizationCopyRights(SetupAuthorizationRef, &rights, NULL,
				   kAuthorizationFlagDefaults |
				       kAuthorizationFlagInteractionAllowed |
				       kAuthorizationFlagPreAuthorize |
				       kAuthorizationFlagExtendRights, NULL);

  if (status != errAuthorizationSuccess)
  {
    fl_alert("You must have administrative priviledges to install this software!");
    return (1);
  }
#else
  if (getuid() != 0)
  {
    fl_alert(gettext("You must be logged in as root to run setup!"));
    return (1);
  }
#endif // __APPLE__

  NextButton->activate();

  Fl::run();

#ifdef __APPLE__
  AuthorizationFree(SetupAuthorizationRef, kAuthorizationFlagDefaults);
#endif // __APPLE__

  return (0);
}


//
// 'get_dists()' - Get a list of available software products.
//

void
get_dists(const char *d)	// I - Directory to look in
{
  int		i;		// Looping var
  int		num_files;	// Number of files
  dirent	**files;	// Files
  const char	*ext;		// Extension
  gui_dist_t	*temp,		// Pointer to current distribution
		*installed;	// Pointer to installed product
  FILE		*fp;		// File to read from
  char		line[1024],	// Line from file...
		product[64];	// Product name...
  int		lowver, hiver;	// Version numbers for dependencies


  // Get the files in the specified directory...
  if (chdir(d))
  {
    perror("setup: Unable to change to distribution directory");
    exit(1);
  }

  if ((num_files = fl_filename_list(".", &files)) == 0)
  {
    fputs("setup: Error - no software products found!\n", stderr);
    exit(1);
  }

  // Build a distribution list...
  NumDists = 0;
  Dists    = (gui_dist_t *)0;

  for (i = 0; i < num_files; i ++)
  {
    ext = fl_filename_ext(files[i]->d_name);
    if (!strcmp(ext, ".install") || !strcmp(ext, ".patch"))
    {
      // Found a .install script...
      if ((fp = fopen(files[i]->d_name, "r")) == NULL)
      {
        perror("setup: Unable to open installation script");
	exit(1);
      }

      // Add a new distribution entry...
      temp           = gui_add_dist(&NumDists, &Dists);
      temp->type     = PACKAGE_PORTABLE;
      temp->filename = strdup(files[i]->d_name);

      strncpy(temp->product, files[i]->d_name, sizeof(temp->product) - 1);
      *strrchr(temp->product, '.') = '\0';	// Drop .install

      // Read info from the installation script...
      while (fgets(line, sizeof(line), fp) != NULL)
      {
        // Only read distribution info lines...
        if (strncmp(line, "#%", 2))
	  continue;

        // Drop the trailing newline...
        line[strlen(line) - 1] = '\0';

        // Copy data as needed...
	if (strncmp(line, "#%product ", 10) == 0)
	  strncpy(temp->name, line + 10, sizeof(temp->name) - 1);
	else if (strncmp(line, "#%version ", 10) == 0)
	  sscanf(line + 10, "%31s%s", temp->version, temp->fulver);
	else if (strncmp(line, "#%rootsize ", 11) == 0)
	  temp->rootsize = atoi(line + 11);
	else if (strncmp(line, "#%usrsize ", 10) == 0)
	  temp->usrsize = atoi(line + 10);
	else if (strncmp(line, "#%incompat ", 11) == 0 ||
	         strncmp(line, "#%requires ", 11) == 0)
	{
	  lowver = 0;
	  hiver  = 0;

	  if (sscanf(line + 11, "%s%*s%d%*s%d", product, &lowver, &hiver) > 0)
	    gui_add_depend(temp, (line[2] == 'i') ? DEPEND_INCOMPAT :
	                                            DEPEND_REQUIRES,
	        	   product, lowver, hiver);
        }
      }

      fclose(fp);
    }
// Do not handle any .rpm
//     else if (!strcmp(ext, ".rpm"))
//     {
//       // Found an RPM package...
//       char	*version,		// Version number
// 		*size,			// Size of package
// 		*description;		// Summary string
// 
// 
//       // Get the package information...
//       snprintf(line, sizeof(line),
//                "rpm -qp --qf '%%{NAME}|%%{VERSION}|%%{SIZE}|%%{SUMMARY}\\n' %s",
// 	       files[i]->d_name);
// 
//       if ((fp = popen(line, "r")) == NULL)
//       {
//         free(files[i]);
// 	continue;
//       }
// 
//       if (!fgets(line, sizeof(line), fp))
//       {
//         free(files[i]);
// 	continue;
//       }
// 
//       // Drop the trailing newline...
//       line[strlen(line) - 1] = '\0';
// 
//       // Grab the different fields...
//       if ((version = strchr(line, '|')) == NULL)
//         continue;
//       *version++ = '\0';
// 
//       if ((size = strchr(version, '|')) == NULL)
//         continue;
//       *size++ = '\0';
// 
//       if ((description = strchr(size, '|')) == NULL)
//         continue;
//       *description++ = '\0';
// 
//       // Add a new distribution entry...
//       temp           = gui_add_dist(&NumDists, &Dists);
//       temp->type     = PACKAGE_RPM;
//       temp->filename = strdup(files[i]->d_name);
// 
//       strlcpy(temp->product, line, sizeof(temp->product));
//       strlcpy(temp->name, description, sizeof(temp->name));
//       strlcpy(temp->version, version, sizeof(temp->version));
//       temp->vernumber = get_vernumber(version);
//       temp->rootsize  = (int)(atof(size) / 1024.0 + 0.5);
// 
//       pclose(fp);
//     }

    free(files[i]);
  }

  free(files);

  if (NumDists == 0)
  {
    fl_alert(gettext("No software found to install!"));
    exit(1);
  }

  if (NumDists > 1)
    qsort(Dists, NumDists, sizeof(gui_dist_t), (compare_func_t)gui_sort_dists);

  for (i = 0, temp = Dists; i < NumDists; i ++, temp ++)
  {
    sprintf(line, "%s v%s", temp->name, temp->version);

    if ((installed = gui_find_dist(temp->product, NumInstalled,
                                   Installed)) == NULL)
    {
      strcat(line, gettext(" (new)"));
      SoftwareList->add(line, 0);
    }
    else if (strcmp(installed->fulver,temp->fulver) > 0)
    {
      strcat(line, gettext(" (downgrade)"));
      SoftwareList->add(line, 0);
    }
    else if (strcmp(installed->fulver,temp->fulver) == 0)
    {
      strcat(line, gettext(" (installed)"));
      SoftwareList->add(line, 0);
    }
    else
    {
      strcat(line, gettext(" (upgrade)"));
      SoftwareList->add(line, 1);
    }
  }

  update_sizes();
}


//
// 'install_dist()' - Install a distribution...
//

int					// O - Install status
install_dist(const gui_dist_t *dist)	// I - Distribution to install
{
  char		command[1024];		// Command string
  int		fds[2];			// Pipe FDs
  int		status;			// Exit status
#ifndef __APPLE__
  int		pid;			// Process ID
#endif // !__APPLE__

  fdfile = fopen("install.log", "a+");
  sprintf(command, "**** %s ****", dist->name);
  InstallLog->add(command);
  if (fdfile)
  {
    fwrite( command, strlen(command), sizeof(char), fdfile );
    fwrite( "\n", strlen("\n"), 1, fdfile );
  }

#ifdef __APPLE__
  // Run the install script using Apple's authorization API...
  FILE		*fp = NULL;
  char		*args[2] = { "now", NULL };
  OSStatus	astatus;


  astatus = AuthorizationExecuteWithPrivileges(SetupAuthorizationRef,
                                               dist->filename,
                                               kAuthorizationFlagDefaults,
                                               args, &fp);

  if (astatus != errAuthorizationSuccess)
  {
    InstallLog->add(gettext("Failed to execute install script!"));
    return (1);
  }

  fds[0] = fileno(fp);
#else
  // Fork the command and redirect errors and info to stdout...
  if (pipe(fds)==-1)
  {
    fprintf(stderr, "epm: Unable to pipe - %s\n", strerror(errno));
    return (1);
  }

  if ((pid = fork()) == 0)
  {
    // Child comes here; start by redirecting stdout and stderr...
    close(1);
    close(2);
    if (dup(fds[1])==-1)
    {
      fprintf(stderr, "epm: Unable to dup - %s\n", strerror(errno));
      return (1);
    }
    if (dup(fds[1])==-1)
    {
      fprintf(stderr, "epm: Unable to dup - %s\n", strerror(errno));
      return (1);
    }

    // Close the original pipes...
    close(fds[0]);
    close(fds[1]);

    // Execute the command; if an error occurs, return it...
    if (dist->type == PACKAGE_PORTABLE) {
      setenv("LANG","C",1);
      execl(dist->filename, dist->filename, "now", (char *)0);
    } else {
      execlp("rpm", "rpm", "-U", "--nodeps", dist->filename, (char *)0);
    }
    exit(errno);
  }
  else if (pid < 0)
  {
    // Unable to fork!
    sprintf(command, "%s %s:", gettext("Unable to install"), dist->name);
    InstallLog->add(command);

    sprintf(command, "\t%s", strerror(errno));
    InstallLog->add(command);

    close(fds[0]);
    close(fds[1]);

    return (1);
  }

  // Close the output pipe (used by the child)...
  close(fds[1]);
#endif // __APPLE__

  // Listen for data on the input pipe...
  Fl::add_fd(fds[0], (void (*)(int, void *))log_cb, fds);

  // Show the user that we're busy...
  SetupWindow->cursor(FL_CURSOR_WAIT);

  // Loop until the child is done...
  while (fds[0])	// log_cb() will close and zero fds[0]...
  {
    // Wait for events...
    Fl::wait();

#ifndef __APPLE__
    // Check to see if the child went away...
    if (waitpid(0, &status, WNOHANG) == pid)
      break;
#endif // !__APPLE__
  }

#ifdef __APPLE__
  fclose(fp);
#endif // __APPLE__

  if (fds[0])
  {
    // Close the pipe - have all the data from the child...
    Fl::remove_fd(fds[0]);
    close(fds[0]);
  }
  else
  {
    // Get the child's exit status...
    wait(&status);
  }

  // Show the user that we're ready...
  SetupWindow->cursor(FL_CURSOR_DEFAULT);

  if (fdfile)
    fclose(fdfile);
  // Return...
  return (status);
}


//
// 'list_cb()' - Handle selections in the software list.
//

void
list_cb(Fl_Check_Browser *, void *)
{
  int		i, j, k;
  gui_dist_t	*dist,
		*dist2;
  gui_depend_t	*depend;

  CustomType = 1;

  if (SoftwareList->nchecked() == 0)
  {
    update_sizes();

    NextButton->deactivate();
    return;
  }

  int LoopExitFlag = 0;

  while (LoopExitFlag != SoftwareList->nchecked())
  {
    LoopExitFlag = SoftwareList->nchecked();
    for (i = 0, dist = Dists; i < NumDists; i ++, dist ++)
      if (SoftwareList->checked(i + 1))
        for (j = 0, depend = dist->depends; j < dist->num_depends; j ++, depend ++)
        {
          switch (depend->type)
          {
            case DEPEND_REQUIRES :
	      if ((dist2 = gui_find_dist(depend->product, NumDists,
	                                 Dists)) != NULL)
	      {
  		// Software is in the list, is it selected?
		k = dist2 - Dists;
		// if item checked
		if (SoftwareList->checked(SoftwareList->value())) {
// 		  std::cout << "checked!" << std::endl;
		  if (SoftwareList->checked(k + 1))
		    continue;
		  SoftwareList->checked(k + 1, 1);
		} else {
		  // uncheck item
// 		  std::cout << "unchecked!" << std::endl;
		  if ( ! SoftwareList->checked(k + 1))
		    SoftwareList->checked(i + 1, 0);
		}
	      }
	      else if ((dist2 = gui_find_dist(depend->product, NumInstalled,
	                                      Installed)) == NULL)
	      {
		// Required but not installed or available!
		fl_alert(gettext("%s requires %s to be installed, but it is not available "
	        	 "for installation."), dist->name, depend->product);
		SoftwareList->checked(i + 1, 0);
		skip_pane_select = 0;
		break;
	      }
	      break;

            case DEPEND_INCOMPAT :
	      if ((dist2 = gui_find_dist(depend->product, NumInstalled,
	                                 Installed)) != NULL)
	      {
		// Already installed!
		fl_alert(gettext("%s is incompatible with %s. Please remove it before "
	        	 "installing this software."), dist->name, dist2->name);
		SoftwareList->checked(i + 1, 0);
		skip_pane_select = 0;
		break;
	      }
	      else if ((dist2 = gui_find_dist(depend->product, NumDists,
	                                      Dists)) != NULL)
	      {
  		// Software is in the list, is it selected?
	        k = dist2 - Dists;

		// Software is in the list, is it selected?
		if (!SoftwareList->checked(k + 1))
		  continue;

        	// Yes, tell the user...
		fl_alert(gettext("%s is incompatible with %s. Please deselect it before "
	        	 "installing this software."), dist->name, dist2->name);
		SoftwareList->checked(i + 1, 0);
		skip_pane_select = 0;
		break;
	      }
            default :
	      break;
          }
        }
  }

  update_sizes();

  if (SoftwareList->nchecked())
    NextButton->activate();
  else
    NextButton->deactivate();
}


//
// 'load_image()' - Load the setup image file (setup.gif/xpm)...
//

void
load_image(void)
{
  Fl_Image	*img;			// New image


  if (!access("setup.xpm", 0))
    img = new Fl_XPM_Image("setup.xpm");
  else if (!access("setup.gif", 0))
    img = new Fl_GIF_Image("setup.gif");
  else
    img = NULL;

  if (img)
    WelcomeImage->image(img);
}


//
// 'load_readme()' - Load the readme file...
//

void
load_readme(void)
{
  ReadmeFile->textfont(FL_HELVETICA);
  ReadmeFile->textsize(14);

  if (access("setup.readme", 0))
  {
    int		i;			// Looping var
    gui_dist_t	*dist;			// Current distribution
    char	*buffer,		// Text buffer
		*ptr;			// Pointer into buffer


    buffer = new char[1024 + NumDists * 300];

    strcpy(buffer,
           gettext("This program allows you to install the following software:\n"
	   "<ul>\n"));
    ptr = buffer + strlen(buffer);

    for (i = NumDists, dist = Dists; i > 0; i --, dist ++)
    {
      sprintf(ptr, "<li>%s, %s\n", dist->name, dist->version);
      ptr += strlen(ptr);
    }

    strcpy(ptr, "</ul>");

    ReadmeFile->value(buffer);

    delete[] buffer;
  }
  else
    gui_load_file(ReadmeFile, "setup.readme");
 
}


//
// 'load_types()' - Load the installation types from the setup.types file.
//

void
load_types(void)
{
  int		i;		// Looping var
  FILE		*fp;		// File to read from
  char		line[1024],	// Line from file
		*lineptr,	// Pointer into line
		*sep;		// Separator
  gui_intype_t	*dt;		// Current install type
  gui_dist_t	*dist;		// Distribution


  NumInstTypes = 0;

  if ((fp = fopen("setup.types", "r")) != NULL)
  {
    dt = NULL;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
      if (line[0] == '#' || line[0] == '\n')
        continue;

      lineptr = line + strlen(line) - 1;
      if (*lineptr == '\n')
        *lineptr = '\0';

      if (!strncasecmp(line, "TYPE", 4) && isspace(line[4]))
      {
        // New type...
	if (NumInstTypes >= (int)(sizeof(InstTypes) / sizeof(InstTypes[0])))
	{
	  fprintf(stderr, "setup: Too many TYPEs (> %d) in setup.types!\n",
	          (int)(sizeof(InstTypes) / sizeof(InstTypes[0])));
	  fclose(fp);
	  exit(1);
	}

        // Skip whitespace...
        for (lineptr = line + 5; isspace(*lineptr); lineptr ++);

	// Copy name and label string...
        dt = InstTypes + NumInstTypes;
	NumInstTypes ++;

	if (!strcasecmp(lineptr,"Custom Configuration"))
	  strncpy(lineptr, gettext("Custom Configuration"), sizeof(dt->name));
	else if (!strcasecmp(lineptr,"Dr.Web for Linux Workstations"))
	  strncpy(lineptr, gettext("Dr.Web for Linux Workstations"), sizeof(dt->name));

	memset(dt, 0, sizeof(gui_intype_t));
	if ((sep = strchr(lineptr, '/')) != NULL)
	  *sep++ = '\0';
	else
	  sep = lineptr;

	strncpy(dt->name, lineptr, sizeof(dt->name));
	strncpy(dt->label, sep, sizeof(dt->label) - 10);
      }
      else if (!strncasecmp(line, "INSTALL", 7) && dt && isspace(line[7]))
      {
        // Install a product...
	if (dt->num_products >= (int)(sizeof(dt->products) / sizeof(dt->products[0])))
	{
	  fprintf(stderr, "setup: Too many INSTALLs (> %d) in setup.types!\n",
	          (int)(sizeof(dt->products) / sizeof(dt->products[0])));
	  fclose(fp);
	  exit(1);
	}

        // Skip whitespace...
        for (lineptr = line + 8; isspace(*lineptr); lineptr ++);

	// Add product to list...
        if ((dist = gui_find_dist(lineptr, NumDists, Dists)) != NULL)
	{
          dt->products[dt->num_products] = dist - Dists;
	  dt->num_products ++;
	  dt->size += dist->rootsize + dist->usrsize;

          if ((dist = gui_find_dist(lineptr, NumInstalled, Installed)) != NULL)
	    dt->size -= dist->rootsize + dist->usrsize;
	}
	else
	  fprintf(stderr, "setup: Unable to find product \"%s\" for \"%s\"!\n",
	          lineptr, dt->label);
      }
      else
      {
        fprintf(stderr, "setup: Bad line - %s\n", line);
	fclose(fp);
	exit(1);
      }
    }

    fclose(fp);
  }
  else
  {
    Title[PANE_TYPE]->hide();
    Title[PANE_SELECT]->position(5, 35);
    Title[PANE_CONFIRM]->position(5, 60);
    Title[PANE_LICENSE]->position(5, 85);
    Title[PANE_INSTALL]->position(5, 110);
    Title[PANE_POSTIN]->position(5, 135);
  }

  for (i = 0, dt = InstTypes; i < NumInstTypes; i ++, dt ++)
  {
    /*if (dt->size <= 1024)
      sprintf(dt->label + strlen(dt->label), gettext(" (%.1fMb disk space)"),
              dt->size / 1024.0);
    else if (dt->size < 0)
      sprintf(dt->label + strlen(dt->label), gettext(" (%dKb disk space)"), dt->size);
    else if (dt->size >= 1024)
      sprintf(dt->label + strlen(dt->label), gettext(" (+%.1fMb disk space)"),
              dt->size / 1024.0);
    else if (dt->size = 0) */
      sprintf(dt->label + strlen(dt->label), "");
    /*else if (dt->size)
      sprintf(dt->label + strlen(dt->label), gettext(" (+%dKb disk space)"), dt->size);
*/
    if ((lineptr = strchr(dt->label, '/')) != NULL)
      TypeButton[i]->label(lineptr + 1);
    else
      TypeButton[i]->label(dt->label);

    TypeButton[i]->show();
  }

  for (; i < (int)(sizeof(TypeButton) / sizeof(TypeButton[0])); i ++)
    TypeButton[i]->hide();

  // Replace round button by a plain label if only 1 option available.
  if (NumInstTypes==1) {
    inst_type_label->label(gettext("The installation type:"));
    TypeButton[0]->hide();
    TypeButton[0]->parent()->begin();
    Fl_Box *box=new Fl_Box(TypeButton[0]->x(), TypeButton[0]->y(),
                           TypeButton[0]->w(), TypeButton[0]->h(),
                           TypeButton[0]->label());
    box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    TypeButton[0]->parent()->end();
  }

  TypeButton[0]->setonly();
  TypeButton[0]->do_callback();
}


//
// 'log_cb()' - Add one or more lines of text to the installation log.
//

void
log_cb(int fd,			// I - Pipe to read from
       int *fdptr)		// O - Pipe to read from
{
  int		bytes;		// Bytes read/to read
  char		*bufptr;	// Pointer into buffer
  static int	bufused = 0;	// Number of bytes used
  static char	buffer[8193];	// Buffer


  bytes = 8192 - bufused;
  if ((bytes = read(fd, buffer + bufused, bytes)) <= 0)
  {
    // End of file; zero the FD to tell the install_dist() function to
    // stop...

    Fl::remove_fd(fd);
    close(fd);
    *fdptr = 0;

    if (bufused > 0)
    {
      // Add remaining text...
      buffer[bufused] = '\0';
      InstallLog->add(buffer);
      bufused = 0;
    }
  }
  else
  {
    // Add bytes to the buffer, then add lines as needed...
    bufused += bytes;
    buffer[bufused] = '\0';

    while ((bufptr = strchr(buffer, '\n')) != NULL)
    {
      *bufptr++ = '\0';
	if (fdfile)
	{
		fwrite( buffer, strlen(buffer), sizeof(char), fdfile );
 		fwrite( "\n", strlen("\n"), 1, fdfile );
	}

      InstallLog->add(buffer);
      strcpy(buffer, bufptr);
      bufused -= bufptr - buffer;
    }
  }

  InstallLog->bottomline(InstallLog->size());
}

void lockInstallAll() {
int		j,k,m;
gui_dist_t	*dist,*dist1;
gui_depend_t	*depend;

  InstallAllButton->activate();
  for (j = 0, dist = Dists; j < NumDists; j ++, dist ++ )
      for (k = 0, depend = dist->depends; k < dist->num_depends; k ++, depend ++ )
        if ( depend != NULL)
	  if (depend->type == DEPEND_INCOMPAT)
	    for (m = 0, dist1 = Dists; m < NumDists; m ++, dist1 ++ )
	    {
	      if (strcmp(dist1->product,depend->product)) {
//not equal
		InstallAllButton->activate();
	      } else {
		InstallAllButton->deactivate();
		return;
	      }
            }
}

void update_control(int from) {
  int		i;		// Looping var
  int		progress;		// Progress so far...
  int		error;			// Errors?
  static char	message[1024];		// Progress message...
  static char	install_type[1024];	// EPM_INSTALL_TYPE env variable
  char		postin_script[1024];
  struct stat	postin_info;		// postin message file info	

  update_label();

//open log
  if (!fdfile) {
    fdfile = fopen("install.log", "w+");
    fclose(fdfile);
  }
  if (Wizard->value() == Pane[PANE_WELCOME]) {
    PrevButton->deactivate();
    NextButton->activate();
  }
  if (Wizard->value() == Pane[PANE_TYPE]) {
    // if no InstTypes skip
    if (NumInstTypes == 0 && from == 1)
      Wizard->next();

    PrevButton->activate();
    NextButton->activate();
    CancelButton->activate();
  }
  if (Wizard->value() == Pane[PANE_SELECT]) {
      lockInstallAll();
      NextButton->activate();
      CancelButton->activate();
      if (SoftwareList->nchecked() == 0)
      {
        NextButton->deactivate();
        update_label();
        return;
      }

      if (NumInstTypes)
        PrevButton->activate();
    // Last entry is Custom always
   if ( CustomType == 1 )
     for (i = (int)(sizeof(TypeButton) / sizeof(TypeButton[0])); i > 0 ; i --)
       if ( TypeButton[i]->visible() && !(strcasecmp(TypeButton[i]->label(),"Custom Configuration"))) {
         TypeButton[i]->setonly();
       }

    // Figure out which type is chosen...
      for (i = 0; i < (int)(sizeof(TypeButton) / sizeof(TypeButton[0])); i ++)
        if (TypeButton[i]->value())
          break;

      if (i < (int)(sizeof(TypeButton) / sizeof(TypeButton[0])))
      {
        // Set the EPM_INSTALL_TYPE environment variable...
        snprintf(install_type, sizeof(install_type), "EPM_INSTALL_TYPE=%s", InstTypes[i].name);
        putenv(install_type);

      // Skip product selection if this type has a list already...
        if (InstTypes[i].num_products > 0  && from == 1)
	{
	//dont skip! may be conflicts. check depends with list_cb
	  skip_pane_select = 1;
	  list_cb(0,0);
	  if ( skip_pane_select )
            Wizard->next();
	}
      }
  }
  if (Wizard->value() == Pane[PANE_LICENSE]) {
    //copy code from license_dist
    static char		liclabel[1024];		// Label for license pane

//    snprintf(liclabel, sizeof(liclabel), gettext("Software License"));
    CancelButton->label(gettext("Cancel"));
    CancelButton->deactivate();
    PrevButton->activate();
    NextButton->activate();
    InstallLog->clear();
//hack
    licaccept = 0;

    // Set the title string...
//    LicenseFile->label(liclabel);
    // Load the license into the viewer...
    LicenseFile->textfont(FL_HELVETICA);
    LicenseFile->textsize(10);

    if (Language->size() != 0)
    {
      // Show the license window and wait for the user...
      Pane[PANE_LICENSE]->show();
      Title[PANE_LICENSE]->activate();
      NextButton->activate();
      CancelButton->activate();
      if ( licaccept == 0 ) {
        LicenseDecline->set();
        LicenseAccept->clear();
      } else {
        LicenseDecline->clear();
        LicenseAccept->set();
      }

      while (Pane[PANE_LICENSE]->visible())
        Fl::wait();
      Title[PANE_INSTALL]->deactivate();
      CancelButton->deactivate();
      NextButton->deactivate();

      if (LicenseDecline->value())
      {
        // Can't install without acceptance...
        char	message[1024];		// Message for log
        InstallLog->clear();
        snprintf(message, sizeof(message), gettext("License is not accepted!"));
        InstallLog->add(message);
        licaccept = 0;
      } else if (LicenseAccept->value())
        licaccept = 1;
    }
    else
    {
      licaccept = 1;
      Wizard->next();
    }
  }
//install must be in own check
  if (Wizard->value() == Pane[PANE_INSTALL]) {
    // Show the licenses for each of the selected software packages...
    update_label();
    CheckPostin->hide();
    if ( licaccept == 0 )
    {
      InstallPercent->label(gettext("Installation canceled!"));
      Pane[PANE_INSTALL]->redraw();
      CancelButton->label(gettext("Close"));
      CancelButton->activate();
      NextButton->deactivate();
      fl_beep();
      return;
    }
//deactivate buttons due install progress
    NextButton->deactivate();
    PrevButton->deactivate();
    CancelButton->deactivate();
    CancelButton->label(gettext("Cancel"));
    fdfile = fopen("install.log", "w+");
    if (fdfile)
	fclose(fdfile);
    else
	perror("fopen");

    for (i = 0, progress = 0, error = 0; i < NumDists; i ++)
      if (SoftwareList->checked(i + 1))
      {
        sprintf(message, gettext("Installing %s v%s..."), Dists[i].name,
	        Dists[i].version);

        InstallPercent->value(100.0 * progress / SoftwareList->nchecked());
	InstallPercent->label(message);
	Pane[PANE_INSTALL]->redraw();
        if ((error = install_dist(Dists + i)) != 0)
	  break;

	progress ++;
      }

    InstallPercent->value(100.0);
    InstallFailed = error;
    if (error) {
      InstallPercent->label(gettext("Installation failed"));
      CancelButton->label(gettext("Close"));
      CancelButton->activate();
      NextButton->activate();
    } else {
      InstallPercent->label(gettext("Installation complete"));
      NextButton->activate();
      sprintf(postin_script, POSTIN_I_SCRIPT);
      if (!stat(postin_script, &postin_info))
        CheckPostin->show();
        CheckPostin->set();
    }
    Pane[PANE_INSTALL]->redraw();

    CancelButton->deactivate();

    fl_beep();

  }

  if (Wizard->value() == Pane[PANE_POSTIN]) {

    NextButton->deactivate();
    PrevButton->deactivate();
    CancelButton->activate();
    CancelButton->label(gettext("Close"));
    update_label();
   if (InstallFailed) {
    PostinFile->value(gettext("<span>Installation failed. See install.log for details.</span>"));
   } else {
    // Show the licenses for each of the selected software packages...
    char		postin[1024];		// postin message filename
    int res;

    sprintf(postin_script, POSTIN_SCRIPT);
    if (!stat(postin_script, &postin_info))
      res = run_command(NULL, "%s", POSTIN_SCRIPT);
    if (CheckPostin->value() != 0)
    {
      res = run_command(NULL, "%s", POSTIN_I_SCRIPT);
      if (res==100) {
        const int strlength=1024;
        char fname[strlength];
        fl_filename_absolute(fname, strlength-1, POSTIN_SCRIPT);
        fl_message(gettext("No installed terminal emulator found. Interactive\n"
                           "postinstall script could not be launched. Please run\n"
                           "\"%s\"\n"
                           "in the terminal of your choice to configure your\n"
                           "Dr.Web installation."), fname);
      }
    }
    sprintf(postin, "POSTIN-MSG");
    if (!stat(postin, &postin_info))
    {
      // Load the license into the viewer...
      gui_load_file(PostinFile, postin);
      PostinFile->textfont(FL_HELVETICA);
      PostinFile->textsize(14);
    }
    else
    {
      PostinFile->value(gettext("<span>Installation complete.</span>"));
    }
   }
  }

// else cannot be here. we maybe increase Wizard->value in prev check
  if (Wizard->value() == Pane[PANE_CONFIRM]) {
    ConfirmList->clear();
    for (i = 0; i < NumDists; i ++)
      if (SoftwareList->checked(i + 1))
        ConfirmList->add(SoftwareList->text(i + 1));
    CancelButton->label(gettext("Cancel"));
    CancelButton->activate();
    PrevButton->activate();
    NextButton->activate();
  }

  update_label();
}

void update_label() {
int i;
// update titles
  for (i = 0; i < 7; i ++)
  {
    Title[i]->activate();
    if (Pane[i]->visible())
      break;
  }
  for (i ++; i < 7; i ++)
    Title[i]->deactivate();
}

//
// 'next_cb()' - Show software selections or install software.
//

void
next_cb(Fl_Button *, void *)
{
  Wizard->next();
  update_control(1);
}


//
// 'type_cb()' - Handle selections in the type list.
//

void
type_cb(Fl_Round_Button *w, void *)	// I - Radio button widget
{
  int		i;		// Looping var
  gui_intype_t	*dt;		// Current install type
  gui_dist_t	*temp,		// Current software
		*installed;	// Installed software


  for (i = 0; i < (int)(sizeof(TypeButton) / sizeof(TypeButton[0])); i ++)
    if (w == TypeButton[i])
      break;

  if (i >= (int)(sizeof(TypeButton) / sizeof(TypeButton[0])))
    return;

  dt = InstTypes + i;

  SoftwareList->check_none();

  // Select all products in the install type
  for (i = 0; i < dt->num_products; i ++)
    SoftwareList->checked(dt->products[i] + 1, 1);

  // And then any upgrade products...
  for (i = 0, temp = Dists; i < NumDists; i ++, temp ++)
  {
    if ((installed = gui_find_dist(temp->product, NumInstalled,
                                   Installed)) != NULL &&
        (strcmp(installed->fulver,temp->fulver) < 0))
      SoftwareList->checked(i + 1, 1);
  }

  update_sizes();

  NextButton->activate();
}


//
// 'update_size()' - Update the total +/- sizes of the installations.
//

void
update_sizes(void)
{
  int		i;		// Looping var
  gui_dist_t	*dist,		// Distribution
		*installed;	// Installed distribution
  int		rootsize,	// Total root size difference in kbytes
		usrsize;	// Total /usr size difference in kbytes
  struct statfs	rootpart,	// Available root partition
		usrpart;	// Available /usr partition
  int		rootfree,	// Free space on root partition
		usrfree;	// Free space on /usr partition
  static char	sizelabel[1024];// Label for selected sizes...


  // Get the sizes for the selected products...
  for (i = 0, dist = Dists, rootsize = 0, usrsize = 0;
       i < NumDists;
       i ++, dist ++)
    if (SoftwareList->checked(i + 1))
    {
      rootsize += dist->rootsize;
      usrsize  += dist->usrsize;

      if ((installed = gui_find_dist(dist->product, NumInstalled,
                                     Installed)) != NULL)
      {
        rootsize -= installed->rootsize;
	usrsize  -= installed->usrsize;
      }
    }

  // Get the sizes of the root and /usr partition...
/*#if defined(__sgi) || defined(__svr4__) || defined(__SVR4) || defined(M_XENIX)
  if (statfs("/", &rootpart, sizeof(rootpart), 0))
#else
  if (statfs("/", &rootpart))
#endif // __sgi || __svr4__ || __SVR4 || M_XENIX
    rootfree = 1024;
  else
    rootfree = (int)((double)rootpart.f_bfree * (double)rootpart.f_bsize /
                     1024.0 / 1024.0 + 0.5);

#if defined(__sgi) || defined(__svr4__) || defined(__SVR4) || defined(M_XENIX)
  if (statfs("/usr", &usrpart, sizeof(usrpart), 0))
#else
  if (statfs("/usr", &usrpart))
#endif // __sgi || __svr4__ || __SVR4 || M_XENIX
    usrfree = 1024;
  else
    usrfree = (int)((double)usrpart.f_bfree * (double)usrpart.f_bsize /
                    1024.0 / 1024.0 + 0.5);

  // Display the results to the user...
  if (rootfree == usrfree)
  {
    rootsize += usrsize;

    if (rootsize >= 1024)
      snprintf(sizelabel, sizeof(sizelabel),
               gettext("%+.1fMb required, %dMb available."), rootsize / 1024.0,
               rootfree);
    else
      snprintf(sizelabel, sizeof(sizelabel),
               gettext("%+dk required, %dMb available."), rootsize, rootfree);
  }
  else if (rootsize >= 1024 && usrsize >= 1024)
    snprintf(sizelabel, sizeof(sizelabel),
             gettext("%+.1fMb required on /, %dMb available,\n"
             "%+.1fMb required on /usr, %dMb available."),
             rootsize / 1024.0, rootfree, usrsize / 1024.0, usrfree);
  else if (rootsize >= 1024)
    snprintf(sizelabel, sizeof(sizelabel),
             gettext("%+.1fMb required on /, %dMb available,\n"
             "%+dk required on /usr, %dMb available."),
             rootsize / 1024.0, rootfree, usrsize, usrfree);
  else if (usrsize >= 1024)
    snprintf(sizelabel, sizeof(sizelabel),
             gettext("%+dk required on /, %dMb available,\n"
             "%+.1fMb required on /usr, %dMb available."),
             rootsize, rootfree, usrsize / 1024.0, usrfree);
  else
    snprintf(sizelabel, sizeof(sizelabel),
             gettext("%+dk required on /, %dMb available,\n"
             "%+dk required on /usr, %dMb available."),
             rootsize, rootfree, usrsize, usrfree);
*/
  snprintf(sizelabel, sizeof(sizelabel), "");
  SoftwareSize->label(sizelabel);
  SoftwareSize->redraw();
}

void
change_lang(Fl_Choice*, void*)
{
   //copy code from license_dist
    char		licfile_en[1024];		// License filename
    char		licfile_ru[1024];		// License filename
    // See if we need to show the license file...
    sprintf(licfile_en, LIC_EN);
    sprintf(licfile_ru, LIC_RU);
    switch (Language->value())
    {
	case LANG_EN:
		gui_load_file(LicenseFile, licfile_en);
		break;
	case LANG_RU:
		gui_load_file(LicenseFile, licfile_ru);
		break;
    }
}

void
load_license()
{
  struct stat		licinfo;		// License file info
  char		licfile_en[1024];		// License filename
  char		licfile_ru[1024];		// License filename

    // See if we need to show the license file...
    sprintf(licfile_en, LIC_EN);
    sprintf(licfile_ru, LIC_RU);

    LicenseFile->textfont(FL_HELVETICA);
    LicenseFile->textsize(10);

    if (!stat(licfile_en, &licinfo))
    {
      //Lang control
      Language->add(gettext("English"));
      Language->value(LANG_EN);
      gui_load_file(LicenseFile, licfile_en);
    }
    if (!stat(licfile_ru, &licinfo))
    {
      //Lang control
      Language->add(gettext("Russian"));
      if (Language->size() == 0) {
          Language->value(LANG_RU);
          gui_load_file(LicenseFile, licfile_ru);
      }
      if (getenv("LANG"))
        if (strncasecmp(getenv("LANG"),"ru",2) == 0) {
          Language->value(LANG_RU);
          gui_load_file(LicenseFile, licfile_ru);
        }
    }
}

void
load_more_licenses()
{
  dirent **files;
  int num_files;
  int i;
  string s, header;

  // Clear the widget.
  LicenseFileOther->value(0);

  // Merge subpackage copyrights and put them in a tab.
  // TODO: Take in account the list of components selected?
  if ((num_files=fl_filename_list(".", &files))==0) {
    fputs("setup: No additinal software licenses found.\n", stderr);
    return;
  }
  for (i=0; i<num_files; ++i) {
    s=files[i]->d_name;
    if (s.find(".COPYRIGHTS", s.size()-strlen(".COPYRIGHTS"))!=string::npos) {
      header="<pre>---------------------------------------------------\n";
      header+="Subpackage: ";
      header+=s.substr(0, s.find(".COPYRIGHTS"));
      header+="\n</pre>";
      if (LicenseFileOther->value())
        header=LicenseFileOther->value()+header;
      LicenseFileOther->value(header.c_str());
      gui_load_file(LicenseFileOther, s.c_str(), true);
    }
  }

  // Free file list.
  for (i=num_files; i>0;)
    free((void *)(files[--i]));
  free((void *)files);
}

//
// End of "$Id: setup2.cxx,v 1.15.2.19 2009/10/16 12:24:14 bsavelev Exp $".
//
