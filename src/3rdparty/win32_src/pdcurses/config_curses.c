#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#if defined( _WIN32) || defined( __WATCOMC__)
   #include <direct.h>
#else
   #include <unistd.h>
#endif

/* Code to 'configure' curses.h.  Run as,  e.g.,

./config_curses -DPDC_WIDE -DXCURSES

it will check to see if the existing curses.h has those options set
(and not the others,  such as CHTYPE_32).  Essentially,  it just checks
to see if the existing configuration matches the one described on the
command line.

   If it matches,  we exit without doing anything.  No file is updated.

   If the configurations differ,  a new 'curses0.h' is created that
has the options given on the command line.  This is done by reading in
the existing 'curses.h',  snipping out the lines with config data,
and inserting new ones.  When we're done,  we remove 'curses.h' and
move our new 'curses0.h' into its place.  This will force a full
recompile on the next build,  which is what we want anyway if
the configuration has changed.  */

int main( const int argc, const char **argv)
{
   FILE *ifile, *ofile;
   int options_found = 0, i, j, options_desired = 0;
   int verbose = 0;
   const char *option_text[] = {
               "XCURSES", "PDC_RGB", "PDC_WIDE",
               "PDC_FORCE_UTF8", "PDC_DLL_BUILD", "PDC_NCMOUSE",
               "CHTYPE_32"  };
   const int n_options = (int)( sizeof( option_text) / sizeof( option_text[0]));
   char buff[300];
   const char *curses_h_filename = "curses.h";
   const char *curses0_h_filename = "curses0.h";

   for( i = 1; i < argc; i++)
      if( argv[i][0] == '-')
         switch( argv[i][1])
            {
            case 'd':
               {
               int err_code;
               const char *arg = (!argv[i][2] && i < argc - 1 ?
                           argv[i + 1] : argv[i] + 2);

#ifdef _WIN32
               err_code = _chdir( arg);
#else
               err_code = chdir( arg);
#endif
               if( err_code)
                  {
                  fprintf( stderr, "Couldn't change dir to '%s'\n", arg);
                  perror( NULL);
                  return( -1);
                  }
               }
               break;
            case 'v':
               verbose = 1;
               break;
            case 'D':
               for( j = 0; j < n_options; j++)
                  if( !strcmp( argv[i] + 2, option_text[j]))
                     options_desired |= (1 << j);
               break;
            default:
               printf( "Option '%s' not recognized\n", argv[i]);
            }

   ifile = fopen( curses_h_filename, "rb");
   assert( ifile);
   while( fgets( buff, sizeof( buff), ifile))
      if( !memcmp( buff, "   #define ", 11))
         for( j = 0; j < n_options; j++)
            if( !memcmp( buff + 11, option_text[j], strlen( option_text[j]))
                  && buff[11 + strlen( option_text[j])] <= ' ')
               options_found |= (1 << j);
   if( verbose)
      printf( "options_found = 0x%x   options_desired = 0x%x\n",
               options_found, options_desired);
   if( options_found == options_desired)
      {
      fclose( ifile);         /* nothing to do */
      return( 0);
      }
   fseek( ifile, 0L, SEEK_SET);
   ofile = fopen( curses0_h_filename, "wb");
   assert( ofile);
   while( fgets( buff, sizeof( buff), ifile))
      {
      fputs( buff, ofile);
      if( !memcmp( buff, "**man-end*******", 16))
         {
         fputs( "\n", ofile);
         for( i = 0; i < n_options; i++)
            if( options_desired & (1 << i))
               {
               if( !strcmp( option_text[i], "PDC_DLL_BUILD"))
                  fprintf( ofile, "#if defined( __WIN32) && !defined( PDC_DLL_BUILD)\n");
               else
                  fprintf( ofile, "#ifndef %s\n", option_text[i]);
               fprintf( ofile, "   #define %s\n", option_text[i]);
               fprintf( ofile, "#endif\n");
               }
         fputs( "\n", ofile);
         while( fgets( buff, sizeof( buff), ifile)
                                  && memcmp( buff, "#define PDCURSES", 16))
            ;
         fputs( buff, ofile);                      /* dump rest of file unaltered*/
         while( fgets( buff, sizeof( buff), ifile))
            fputs( buff, ofile);
         }
      }
   fclose( ofile);
   fclose( ifile);
#ifdef _WIN32
   _unlink( curses_h_filename);
#else
   unlink( curses_h_filename);
#endif
   rename( curses0_h_filename, curses_h_filename);
   if( verbose)
      printf( "curses.h updated\n");
   return( 0);
}
