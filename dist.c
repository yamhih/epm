/*
 * "$Id: dist.c,v 1.15 2001/01/19 16:16:50 mike Exp $"
 *
 *   Distribution functions for the ESP Package Manager (EPM).
 *
 *   Copyright 1999-2001 by Easy Software Products.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 * Contents:
 *
 *   free_dist()    - Free memory used by a distribution.
 *   read_dist()    - Read a software distribution.
 *   add_string()   - Add a command to an array of commands...
 *   free_strings() - Free memory used by the array of strings.
 *   get_line()     - Get a line from a file, filtering for uname lines...
 *   expand_name()  - Expand a filename with environment variables.
 *   patmatch()     - Pattern matching...
 */

/*
 * Include necessary headers...
 */

#include "epm.h"
#include <pwd.h>


/*
 * Local functions...
 */

static int	add_string(int num_strings, char ***strings, char *string);
static void	expand_name(char *buffer, char *name);
static void	free_strings(int num_strings, char **strings);
static char	*get_line(char *buffer, int size, FILE *fp,
		          struct utsname *platform, const char *format,
			  int *skip);
static int	patmatch(const char *, const char *);


/*
 * 'add_file()' - Add a file to the distribution.
 */

file_t *		/* O - New file */
add_file(dist_t *dist)	/* I - Distribution */
{
  file_t	*file;	/* New file */


  if (dist->num_files == 0)
    dist->files = (file_t *)malloc(sizeof(file_t));
  else
    dist->files = (file_t *)realloc(dist->files, sizeof(file_t) *
					         (dist->num_files + 1));

  file = dist->files + dist->num_files;
  dist->num_files ++;

  return (file);
}


/*
 * 'free_dist()' - Free memory used by a distribution.
 */

void
free_dist(dist_t *dist)		/* I - Distribution to free */
{
  if (dist->num_files > 0)
    free(dist->files);

  free_strings(dist->num_descriptions, dist->descriptions);
  free_strings(dist->num_installs, dist->installs);
  free_strings(dist->num_removes, dist->removes);
  free_strings(dist->num_patches, dist->patches);
  free_strings(dist->num_incompats, dist->incompats);
  free_strings(dist->num_requires, dist->requires);

  free(dist);
}


/*
 * 'read_dist()' - Read a software distribution.
 */

dist_t *				/* O - New distribution */
read_dist(const char     *filename,	/* I - Main distribution list file */
          struct utsname *platform,	/* I - Platform information */
          const char     *format)	/* I - Format of distribution */
{
  FILE		*listfiles[10];	/* File lists */
  int		listlevel;	/* Level in file list */
  char		line[2048],	/* Expanded line from list file */
		buf[1024],	/* Original line from list file */
		type,		/* File type */
		dst[256],	/* Destination path */
		src[256],	/* Source path */
		pattern[256],	/* Pattern for source files */
		user[32],	/* User */
		group[32],	/* Group */
		*temp;		/* Temporary pointer */
  int		mode,		/* File permissions */
		skip;		/* 1 = skip files, 0 = archive files */
  dist_t	*dist;		/* Distribution data */
  file_t	*file;		/* Distribution file */
  struct stat	fileinfo;	/* File information */
  DIR		*dir;		/* Directory */
  DIRENT	*dent;		/* Directory entry */
  struct passwd	*pwd;		/* Password entry */


 /*
  * Create a new, blank distribution...
  */

  dist = (dist_t *)calloc(sizeof(dist_t), 1);

 /*
  * Open the main list file...
  */

  if ((listfiles[0] = fopen(filename, "r")) == NULL)
  {
    fprintf(stderr, "epm: Unable to open list file \"%s\" -\n     %s\n",
            filename, strerror(errno));
    return (NULL);
  }

 /*
  * Find any product descriptions, etc. in the list file...
  */

  if (Verbosity)
    puts("Searching for product information...");

  skip      = 0;
  listlevel = 0;

  do
  {
    while (get_line(buf, sizeof(buf), listfiles[listlevel], platform, format,
                    &skip) != NULL)
    {
     /*
      * Do variable substitution...
      */

      line[0] = buf[0]; /* Don't expand initial $ */
      expand_name(line + 1, buf + 1);

     /*
      * Check line for config stuff...
      */

      if (line[0] == '%')
      {
       /*
        * Find whitespace...
	*/

        for (temp = line; !isspace(*temp) && *temp; temp ++);
	for (; isspace(*temp); *temp++ = '\0');

       /*
        * Process directive...
        */

	if (strcmp(line, "%include") == 0)
	{
	  listlevel ++;

	  if ((listfiles[listlevel] = fopen(temp, "r")) == NULL)
	  {
	    fprintf(stderr, "epm: Unable to include \"%s\" -\n     %s\n", temp,
	            strerror(errno));
	    listlevel --;
	  }
	}
	else if (strcmp(line, "%description") == 0)
	  dist->num_descriptions =
	      add_string(dist->num_descriptions, &(dist->descriptions), temp);
	else if (strcmp(line, "%install") == 0)
	  dist->num_installs =
	      add_string(dist->num_installs, &(dist->installs), temp);
	else if (strcmp(line, "%remove") == 0)
	  dist->num_removes =
	      add_string(dist->num_removes, &(dist->removes), temp);
	else if (strcmp(line, "%patch") == 0)
	  dist->num_patches =
	      add_string(dist->num_patches, &(dist->patches), temp);
        else if (strcmp(line, "%product") == 0)
	{
          if (!dist->product[0])
            strcpy(dist->product, temp);
	  else
	    fputs("epm: Ignoring %product line in list file.\n", stderr);
	}
	else if (strcmp(line, "%copyright") == 0)
	{
          if (!dist->copyright[0])
            strcpy(dist->copyright, temp);
	  else
	    fputs("epm: Ignoring %copyright line in list file.\n", stderr);
	}
	else if (strcmp(line, "%vendor") == 0)
	{
          if (!dist->vendor[0])
            strcpy(dist->vendor, temp);
	  else
	    fputs("epm: Ignoring %vendor line in list file.\n", stderr);
	}
	else if (strcmp(line, "%packager") == 0)
	{
          if (!dist->packager[0])
            strcpy(dist->packager, temp);
	  else
	    fputs("epm: Ignoring %packager line in list file.\n", stderr);
	}
	else if (strcmp(line, "%license") == 0)
	{
          if (!dist->license[0])
            strcpy(dist->license, temp);
	  else
	    fputs("epm: Ignoring %license line in list file.\n", stderr);
	}
	else if (strcmp(line, "%readme") == 0)
	{
          if (!dist->readme[0])
            strcpy(dist->readme, temp);
	  else
	    fputs("epm: Ignoring %readme line in list file.\n", stderr);
	}
	else if (strcmp(line, "%version") == 0)
	{
          if (!dist->version[0])
	  {
            strcpy(dist->version, temp);
	    if ((temp = strchr(dist->version, ' ')) != NULL)
	    {
	      *temp++ = '\0';
	      dist->vernumber = atoi(temp);
	    }
	    else
	    {
	      dist->vernumber = 0;
	      for (temp = dist->version; *temp; temp ++)
		if (isdigit(*temp))
	          dist->vernumber = dist->vernumber * 10 + *temp - '0';
            }
	  }
	}
	else if (strcmp(line, "%incompat") == 0)
	  dist->num_incompats = add_string(dist->num_incompats,
	                                   &(dist->incompats), temp);
	else if (strcmp(line, "%requires") == 0)
	  dist->num_requires = add_string(dist->num_requires, &(dist->requires),
	                                  temp);
	else if (strcmp(line, "%replaces") == 0)
	  dist->num_replaces = add_string(dist->num_replaces, &(dist->replaces),
	                                  temp);
	else
	{
	  fprintf(stderr, "epm: Unknown directive \"%s\" ignored!\n", line);
	  fprintf(stderr, "     %s %s\n", line, temp);
	}
      }
      else if (line[0] == '$')
      {
       /*
        * Define a variable...
	*/

        if ((temp = strdup(line + 1)) != NULL)
	  putenv(temp);
      }
      else if (sscanf(line, "%c%o%15s%15s%254s%254s", &type, &mode, user, group,
        	      dst, src) < 5)
	fprintf(stderr, "epm: Bad line - %s\n", line);
      else
      {
	if (tolower(type) == 'd' || type == 'R')
	  src[0] = '\0';

#ifdef __osf__ /* Remap group "sys" to "system" */
        if (strcmp(group, "sys") == 0)
	  strcpy(group, "system");
#elif defined(__linux) /* Remap group "sys" to "root" */
        if (strcmp(group, "sys") == 0)
	  strcpy(group, "root");
#endif /* __osf__ */

        if ((temp = strrchr(src, '/')) == NULL)
	  temp = src;
	else
	  temp ++;

        for (; *temp; temp ++)
	  if (strchr("*?[", *temp))
	    break;

        if (*temp)
	{
	 /*
	  * Add using wildcards...
	  */

          if ((temp = strrchr(src, '/')) == NULL)
	    temp = src;
	  else
	    *temp++ = '\0';

	  strncpy(pattern, temp, sizeof(pattern) - 1);
	  pattern[sizeof(pattern) - 1] = '\0';

          if (dst[strlen(dst) - 1] != '/')
	    strncat(dst, "/", sizeof(dst) - 1);

          if (temp == src)
	    dir = opendir(".");
	  else
	    dir = opendir(src);

          if (dir == NULL)
	    fprintf(stderr, "epm: Unable to open directory \"%s\" - %s\n",
	            src, strerror(errno));
          else
	  {
	   /*
	    * Make sure we have a directory separator...
	    */

	    if (temp > src)
	      temp[-1] = '/';

	    while ((dent = readdir(dir)) != NULL)
	    {
	      strcpy(temp, dent->d_name);
	      if (stat(src, &fileinfo))
	        continue; /* Skip files we can't read */

              if (S_ISDIR(fileinfo.st_mode))
	        continue; /* Skip directories */

              if (!patmatch(dent->d_name, pattern))
	        continue;

              file = add_file(dist);

              file->type = type;
	      file->mode = mode;
              strcpy(file->src, src);
	      strcpy(file->dst, dst);
	      strncat(file->dst, dent->d_name, sizeof(file->dst) - 1);
	      strcpy(file->user, user);
	      strcpy(file->group, group);
	    }

            closedir(dir);
	  }
	}
	else
	{
         /*
	  * Add single file...
	  */

          file = add_file(dist);

          file->type = type;
	  file->mode = mode;
          strcpy(file->src, src);
	  strcpy(file->dst, dst);
	  strcpy(file->user, user);
	  strcpy(file->group, group);
	}
      }
    }

    fclose(listfiles[listlevel]);
    listlevel --;
  }
  while (listlevel >= 0);

  if (!dist->packager[0])
  {
   /*
    * Assign a default packager name...
    */

    gethostname(buf, sizeof(buf));

    setpwent();
    if ((pwd = getpwuid(getuid())) != NULL)
      sprintf(dist->packager, "%s@%s", pwd->pw_name, buf);
    else
      sprintf(dist->packager, "unknown@%s", buf);
  }

  return (dist);
}


/*
 * 'add_string()' - Add a command to an array of commands...
 */

static int			/* O - Number of strings */
add_string(int  num_strings,	/* I - Number of strings */
           char ***strings,	/* O - Pointer to string pointer array */
           char *string)	/* I - String to add */
{
  if (num_strings == 0)
    *strings = (char **)malloc(sizeof(char *));
  else
    *strings = (char **)realloc(*strings, (num_strings + 1) * sizeof(char *));

  (*strings)[num_strings] = strdup(string);
  return (num_strings + 1);
}


/*
 * 'expand_name()' - Expand a filename with environment variables.
 */

static void
expand_name(char *buffer,	/* O - Output string */
            char *name)		/* I - Input string */
{
  char	var[255],		/* Environment variable name */
	*varptr;		/* Current position in name */


  while (*name != '\0')
  {
    if (*name == '$')
    {
      name ++;
      for (varptr = var; strchr("/ \t-", *name) == NULL && *name != '\0';)
        *varptr++ = *name++;

      *varptr = '\0';

      if ((varptr = getenv(var)) != NULL)
      {
        strcpy(buffer, varptr);
	buffer += strlen(buffer);
      }
    }
    else
      *buffer++ = *name++;
  }

  *buffer = '\0';
}


/*
 * 'free_strings()' - Free memory used by the array of strings.
 */

static void
free_strings(int  num_strings,	/* I - Number of strings */
             char **strings)	/* I - Pointer to string pointers */
{
  int	i;			/* Looping var */


  for (i = 0; i < num_strings; i ++)
    free(strings[i]);

  if (num_strings)
    free(strings);
}


/*
 * 'get_line()' - Get a line from a file, filtering for uname lines...
 */

static char *				/* O - String read or NULL at EOF */
get_line(char           *buffer,	/* I - Buffer to read into */
         int            size,		/* I - Size of buffer */
	 FILE           *fp,		/* I - File to read from */
	 struct utsname *platform,	/* I - Platform information */
         const char     *format,	/* I - Distribution format */
	 int            *skip)		/* IO - Skip lines? */
{
  int	op,				/* Operation (0 = OR, 1 = AND) */
	namelen,			/* Length of system name + version */
	len,				/* Length of string */
	match;				/* 1 = match, 0 = not */
  char	*ptr,				/* Pointer into value */
	*bufptr,			/* Pointer into buffer */
	namever[255],			/* Name + version */
	value[255];			/* Value string */


  while (fgets(buffer, size, fp) != NULL)
  {
   /*
    * Skip comment and blank lines...
    */

    if (buffer[0] == '#' || buffer[0] == '\n')
      continue;

   /*
    * See if this is a %system or %format line...
    */

    if (strncmp(buffer, "%system ", 8) == 0)
    {
     /*
      * Yes, do filtering based on the OS (+version)...
      */

      *skip &= 2;

      if (strcmp(buffer + 8, "all\n") != 0)
      {
	namelen = strlen(platform->sysname);
        bufptr  = buffer + 8;
	sprintf(namever, "%s-%s", platform->sysname, platform->release);

        *skip |= *bufptr != '!';

        while (*bufptr)
	{
          if (*bufptr == '!')
	  {
	    op = 1;
	    bufptr ++;
	  }
	  else
	    op = 0;

	  for (ptr = value; *bufptr && !isspace(*bufptr) &&
	                        (ptr - value) < (sizeof(value) - 1);)
	    *ptr++ = *bufptr++;

	  *ptr = '\0';
	  while (isspace(*bufptr))
	    bufptr ++;

          if ((ptr = strchr(value, '-')) != NULL)
	    len = ptr - value;
	  else
	    len = strlen(value);

          if (len < namelen)
	    match = 0;
	  else
	    match = strncasecmp(value, namever, strlen(value)) == 0;

          if (op)
	    *skip |= match;
	  else
	    *skip &= ~match;
        }
      }
    }
    else if (strncmp(buffer, "%format ", 8) == 0)
    {
     /*
      * Yes, do filtering based on the distribution format...
      */

      *skip &= 1;

      if (strcmp(buffer + 8, "all\n") != 0)
      {
        bufptr = buffer + 8;
        *skip  |= (*bufptr != '!') * 2;

        while (*bufptr)
	{
          if (*bufptr == '!')
	  {
	    op = 1;
	    bufptr ++;
	  }
	  else
	    op = 0;

	  for (ptr = value; *bufptr && !isspace(*bufptr) &&
	                        (ptr - value) < (sizeof(value) - 1);)
	    *ptr++ = *bufptr++;

	  *ptr = '\0';
	  while (isspace(*bufptr))
	    bufptr ++;

	  match = (strcasecmp(value, format) == 0) * 2;

          if (op)
	    *skip |= match;
	  else
	    *skip &= ~match;
        }
      }
    }
    else if (!*skip)
    {
     /*
      * Otherwise strip any trailing newlines and return the string!
      */

      if (buffer[strlen(buffer) - 1] == '\n')
        buffer[strlen(buffer) - 1] = '\0';

      return (buffer);
    }
  }

  return (NULL);
}


/*
 * 'patmatch()' - Pattern matching...
 */

static int			/* O - 1 if match, 0 if no match */
patmatch(const char *s,		/* I - String to match against */
         const char *pat)	/* I - Pattern to match against */
{
 /*
  * Range check the input...
  */

  if (s == NULL || pat == NULL)
    return (0);

 /*
  * Loop through the pattern and match strings, and stop if we come to a
  * point where the strings don't match or we find a complete match.
  */

  while (*s != '\0' && *pat != '\0')
  {
    if (*pat == '*')
    {
     /*
      * Wildcard - 0 or more characters...
      */

      pat ++;
      if (*pat == '\0')
        return (1);	/* Last pattern char is *, so everything matches now... */

     /*
      * Test all remaining combinations until we get to the end of the string.
      */

      while (*s != '\0')
      {
        if (patmatch(s, pat))
	  return (1);

	s ++;
      }
    }
    else if (*pat == '?')
    {
     /*
      * Wildcard - 1 character...
      */

      pat ++;
      s ++;
      continue;
    }
    else if (*pat == '[')
    {
     /*
      * Match a character from the input set [chars]...
      */

      pat ++;
      while (*pat != ']' && *pat != '\0')
        if (*s == *pat)
	  break;
	else if (pat[1] == '-' && *s >= pat[0] && *s <= pat[2])
	  break;
	else
	  pat ++;

      if (*pat == ']' || *pat == '\0')
        return (0);

      while (*pat != ']' && *pat != '\0')
        pat ++;

      if (*pat == ']')
        pat ++;

      continue;
    }
    else if (*pat == '\\')
    {
     /*
      * Handle quoted characters...
      */

      pat ++;
    }

   /*
    * Stop if the pattern and string don't match...
    */

    if (*pat++ != *s++)
      return (0);
  }

 /*
  * Done parsing the pattern and string; return 1 if the last character matches
  * and 0 otherwise...
  */

  return (*s == *pat);
}


/*
 * End of "$Id: dist.c,v 1.15 2001/01/19 16:16:50 mike Exp $".
 */
