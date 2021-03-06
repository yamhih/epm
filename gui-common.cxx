//
// "$Id: gui-common.cxx,v 1.2.2.2 2009/09/30 12:59:35 bsavelev Exp $"
//
//   ESP Software Wizard common functions for the ESP Package Manager (EPM).
//
//   Copyright 1999-2006 by Easy Software Products.
//   Copyright 2009-2010 by Dr.Web.
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
//   gui_add_depend()    - Add a dependency to a distribution.
//   gui_add_dist()      - Add a distribution.
//   gui_find_dist()     - Find a distribution.
//   gui_get_installed() - Get a list of installed software products.
//   gui_load_file()     - Load a file into a help widget.
//   gui_sort_dists()    - Compare two distribution names...
//

#include "gui-common.h"
#include <FL/filename.H>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <string>
using namespace std;


//
// 'gui_add_depend()' - Add a dependency to a distribution.
//

void
gui_add_depend(gui_dist_t *d,		// I - Distribution
               int        type,		// I - Dependency type
	       const char *name,	// I - Name of product
	       int        lowver,	// I - Lower version number
	       int        hiver)	// I - Highre version number
{
  gui_depend_t	*temp;			// Pointer to dependency


  if (d->num_depends == 0)
    temp = (gui_depend_t *)malloc(sizeof(gui_depend_t));
  else
    temp = (gui_depend_t *)realloc(d->depends,
                                   sizeof(gui_depend_t) * (d->num_depends + 1));

  if (temp == NULL)
  {
    perror("setup: Unable to allocate memory for dependency");
    exit(1);
  }

  d->depends     = temp;
  temp           += d->num_depends;
  d->num_depends ++;

  memset(temp, 0, sizeof(gui_depend_t));
  strncpy(temp->product, name, sizeof(temp->product) - 1);
  temp->type         = type;
  temp->vernumber[0] = lowver;
  temp->vernumber[1] = hiver;
}


//
// 'gui_add_dist()' - Add a distribution.
//

gui_dist_t *				// O - New distribution
gui_add_dist(int        *num_d,		// IO - Number of distributions
             gui_dist_t **d)		// IO - Distributions
{
  gui_dist_t	*temp;			// Pointer to current distribution


  // Add a new distribution entry...
  if (*num_d == 0)
    temp = (gui_dist_t *)malloc(sizeof(gui_dist_t));
  else
    temp = (gui_dist_t *)realloc(*d, sizeof(gui_dist_t) * (*num_d + 1));

  if (temp == NULL)
  {
    perror("setup: Unable to allocate memory for distribution");
    exit(1);
  }

  *d   = temp;
  temp += *num_d;
  (*num_d) ++;

  memset(temp, 0, sizeof(gui_dist_t));

  return (temp);
}


//
// 'gui_find_dist()' - Find a distribution.
//

gui_dist_t *				// O - Pointer to distribution or NULL
gui_find_dist(const char *name,		// I - Distribution name
              int        num_d,		// I - Number of distributions
	      gui_dist_t *d)		// I - Distributions
{
  while (num_d > 0)
  {
    if (!strcmp(name, d->product))
      return (d);

    d ++;
    num_d --;
  }

  return (NULL);
}


//
// 'gui_get_installed()' - Get a list of installed software products.
//

void
gui_get_installed(void)
{
  int		i;			// Looping var
  int		num_files;		// Number of files
  dirent	**files;		// Files
  const char	*ext;			// Extension
  gui_dist_t	*temp;			// Pointer to current distribution
  FILE		*fp;			// File to read from
  char		line[1024],		// Line from file...
		product[64];		// Product name...
  int		lowver, hiver;		// Version numbers for dependencies

  // See if there are any installed files...
  NumInstalled = 0;
  Installed    = (gui_dist_t *)0;

  if ((num_files = fl_filename_list(EPM_SOFTWARE, &files)) > 0)
  {
    // Build a distribution list...
    for (i = 0; i < num_files; i ++)
    {
      ext = fl_filename_ext(files[i]->d_name);

      if (!strcmp(ext, ".remove"))
      {
	// Found a .remove script...
	snprintf(line, sizeof(line), EPM_SOFTWARE "/%s", files[i]->d_name);
	if ((fp = fopen(line, "r")) == NULL)
	{
          perror("setup: Unable to open removal script");
	  exit(1);
	}

	// Add a new distribution entry...
	temp       = gui_add_dist(&NumInstalled, &Installed);
        temp->type = PACKAGE_PORTABLE;

	strncpy(temp->product, files[i]->d_name, sizeof(temp->product) - 1);
	*strrchr(temp->product, '.') = '\0';	// Drop .remove

	// Read info from the removal script...
	while (fgets(line, sizeof(line), fp))
	{
          // Only read distribution info lines...
          if (strncmp(line, "#%", 2))
	    continue;

          // Drop the trailing newline...
          line[strlen(line) - 1] = '\0';

          // Copy data as needed...
	  if (!strncmp(line, "#%product ", 10))
	    strncpy(temp->name, line + 10, sizeof(temp->name) - 1);
	  else if (!strncmp(line, "#%version ", 10))
	    sscanf(line + 10, "%31s%s", temp->version, temp->fulver);
	  else if (!strncmp(line, "#%rootsize ", 11))
	    temp->rootsize = atoi(line + 11);
	  else if (!strncmp(line, "#%usrsize ", 10))
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

      free(files[i]);
    }

    free(files);
  }

// Do not handle any .rpm
//   // Get a list of RPM packages that are installed...
//   if (!access("/bin/rpm", 0) &&
//       (fp = popen("/bin/rpm -qa --qf "
//                   "'%{NAME}|%{VERSION}|%{SIZE}|%{SUMMARY}\\n'", "r")) != NULL)
//   {
//     char	*version,		// Version number
// 		*size,			// Size of package
// 		*description;		// Summary string
// 
// 
//     while (fgets(line, sizeof(line), fp))
//     {
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
//       temp       = gui_add_dist(&NumInstalled, &Installed);
//       temp->type = PACKAGE_RPM;
// 
//       strlcpy(temp->product, line, sizeof(temp->product));
//       strlcpy(temp->name, description, sizeof(temp->name));
//       strlcpy(temp->version, version, sizeof(temp->version));
//       temp->vernumber = get_vernumber(version);
//       temp->rootsize  = (int)(atof(size) / 1024.0 + 0.5);
//     }
// 
//     pclose(fp);
//   }

  if (NumInstalled > 1)
    qsort(Installed, NumInstalled, sizeof(gui_dist_t),
          (compare_func_t)gui_sort_dists);
}


//
// 'gui_load_file()' - Load a file into a help widget.
//

void
gui_load_file(Fl_Help_View *hv,		 // I - Help widget
              const char   *filename,	 // I - File to load
              bool         append)	// I - Append file to the widget str
{
  FILE		*fp;			// File pointer
  struct stat	info;			// Info about file
  int		ch;			// Character from file
  char		*buffer,		// File buffer
		*ptr;			// Pointer into buffer


  // Try opening the file and getting the file size...
  if ((fp = fopen(filename, "r")) == NULL)
  {
    hv->value(strerror(errno));
    return;
  }

  if (stat(filename, &info))
  {
    hv->value(strerror(errno));
    return;
  }

  // Allocate a buffer that is more than big enough to the hold the file...
  buffer = new char[info.st_size * 2];

  // See if we have a HTML file...
  if ((ch = getc(fp)) == '<')
  {
    // Yes, just read it in...
    buffer[0] = ch;
    size_t r=fread(buffer + 1, 1, info.st_size - 1, fp);
    buffer[info.st_size] = '\0';
  }
  else
  {
    // No, treat it as plain text...
    ungetc(ch, fp);

    strcpy(buffer, "<pre>");

    ptr = buffer + 5;

    while ((ch = getc(fp)) != EOF)
    {
      if (ch == '&')
      {
        strcpy(ptr, "&amp;");
	ptr += 5;
      }
      else if (ch == '<')
      {
        strcpy(ptr, "&lt;");
	ptr += 4;
      }
      else
        *ptr++ = ch;
    }

    strcpy(ptr, "</pre>\n");

    // Preformatted text will be too large without a reduction in the
    // base size...
    hv->textsize(10);
  }

  string text=buffer;
  if (append && hv->value())
      text=hv->value()+text;

  // Save the loaded buffer to the help widget...
  hv->value(text.c_str());

  // Free memory and close the file...
  delete[] buffer;
  fclose(fp);
}


//
// 'gui_sort_dists()' - Compare two distribution names...
//

int					// O - Result of comparison
gui_sort_dists(const gui_dist_t *d0,	// I - First distribution
               const gui_dist_t *d1)	// I - Second distribution
{
  return (strcmp(d0->product, d1->product));
}


char* findMypath(const char* argv)
{	
	static char path[300];
	int i;
	
	strncpy(path, argv, sizeof(path));
	
	for (i = strlen(path) - 1; i >= 0; --i)
	{
		if (path[i] == '\\' || path[i] == '/')
		{
			path[i + 1] = 0;
			break;
		}
	}	
	
	if (i == -1) // user just typed the binary name
		strcpy(path, ".");
				
	return path;
}

void set_gettext(char *argv[]) {
  struct stat stat_info;
  char localedir[1024];
  char *cwd;

  if ((cwd = getcwd ((char*) NULL, 512)) == NULL)
    if (!stat(findMypath(argv[0]),&stat_info))
      int r=chdir(findMypath(argv[0]));

  setlocale(LC_ALL, "");
  sprintf(localedir,"%slocale", findMypath(argv[0]));

  if ((!access(localedir,0))&&(!stat(localedir,&stat_info)))
    bindtextdomain ("epm", localedir);
  else
    bindtextdomain ("epm", LOCALEDIR);
  textdomain("epm");
  bind_textdomain_codeset("epm","UTF-8");
}

//
// End of "$Id: gui-common.cxx,v 1.2.2.2 2009/09/30 12:59:35 bsavelev Exp $".
//
