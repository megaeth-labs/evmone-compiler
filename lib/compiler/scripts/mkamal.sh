#!/usr/bin/env bash
    
    
    
#   WHAT IS THIS?
#
#       This is a simplistic script that concatenates/processes some C-headers and -sourcefiles
#       to form a single "amalgamation" file, to be easily integrated into other projects.
#
#       Idea was taken from "SQLite" project (in-process database-lib / Hwaci); implementation
#       is a simplified version of the "createAmalgamation.sh" script from the "whefs" project 
#       (embeddable virtual filesystem / S. Beal).
#
#   HOW TO USE THIS SCRIPT?
#
#       To create an amalgamation, use:
#
#           myscript  <macro_prefix>  <list_of_headers>  <list_of_sources>
#
#       ...where 
#
#           <macro_prefix>      a prefix for macro-names and flags used in the generated file.
#                               (e.g. specifying "megalib" results in macros and flags being prefixed
#                               with "MEGALIB_AMALGAMATION_"). This would typically be the project-name.
#
#                               Must only contain alphanumeric chars and/or underscores.
#
#           <list_of_headers>   whitespace-delimited ordered list of header-files (all as one argument)
#
#           <list_of_sources>   whitespace-delimited list of source-files (all as one argument)
#
#
#       A single file contains the filtered contents of all headers and sources will be printed
#       on standard output. 
#
#   AND HOW TO USE THE RESULTING FILE..?
#
#       (see comment-header in generated file, or hardcoded help-text below.)
#   
    
            
function die
{
    echo  "FATAL: $1"  >&2
    exit 1
}
    
    
    
[ $# -eq 3 ]  ||  die "use: \"$( basename $0 ) <macro_prefix> <headers> <sources>\""
    
MACRO_PFX="$1"
    
HEADERS="$2"
    
SOURCES="$3"
    
    
    
VAR_PREFIX=$( echo $MACRO_PFX | tr a-z A-Z )_AMALGAMATION_
    
VERSION_VARNAME=${VAR_PREFIX}VERSION
    
HDRREQ_VARNAME=${VAR_PREFIX}GIVE_ME_HEADERS
    
INCGUARD_VARNAME=${VAR_PREFIX}INCLUDED
    
VERSION=$(date -Iseconds)
[ -n "$VERSION" ]  ||  die "cannot get timestamp"
    
    
    
function print_converted_contents_of
{
    local fname=$1
    
    [ -f "$fname" ]  ||  die "file '$fname' doesn't exist"
    
    echo
    echo
    echo
    echo  /////////////////////////////////////////////////////
    echo  //
    echo "//   $fname:"
    echo  //
    echo  /////////////////////////////////////////////////////
    echo
    
    sed  -e  '/^ *# *include ".*/d'  $fname
}
    
    
    
cat << ---EOF---
//  $VERSION
//
//    ^^^   The above string is the amalgamation-version, available as compile-time macro 
//          "$VERSION_VARNAME", which is a quoted string.
//
//          You can use  "sed -n -e 's@/* *@@' -e 's/ *$//' -e 1p"  to extract it from this file, 
//          e.g. in a build-script.
// 
// 
//  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//  !!   THIS FILE WAS GENERATED - YOU PROBABLY DON'T WANT TO EDIT IT!   !!
//  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// 
// 
//  WHAT IS THIS?
//  -------------
// 
//      This file is a so-called "amalgamation" (combination/unification/concatenation) of 
//      a number of C headers/sources, and this single file can be integrated into your own 
//      project.
//
//      The advantages over using the original pile-of-files or distributed library, are:
//
//        - it's instantly obvious that an amalgamation is a snapshot, not the original
//        - unambiguous version-control (there is only 1 resulting and diff'able file)
//        - less margin for errors/omission/mixup (there is only 1 resulting file)
//     
//      This single file can be dropped into your project and compiled as any other file.
//
//
//  HOW TO USE IT?
//  --------------
//
//      To extract/use only headers (assuming the file you're reading now is called "my_amalgamation.c"):
//
//          in "your_file.c":
//
//                ...
//                #define $HDRREQ_VARNAME
//                #include "my_amalgamation.c"
//                ...
//                // use functions from file "my_amalgamation.c" here
//                ...
//
//          (the flag "$HDRREQ_VARNAME" will be cleared automatically within "my_amalgamation.c")
//
//      To compile:
//
//          (nothing special - just compile "my_amalgamation.c" as any other)
//
//      Note that the suggested extension for this file is ".c". This is done to make the compiler
//      (or at least GCC) happy. However, it can also be included as if it were a header - see above.
//
//  WHICH FILES WERE INCLUDED?
//  --------------------------
// 
//      following C-headers and -sources:
// 
//          headers, in this order:
// 
$( for a in $HEADERS; do echo "//            $a"; done )
//
//          sources, in this order:
//
$( for a in $SOURCES; do echo "//            $a"; done )
//
    
#ifndef  $INCGUARD_VARNAME
#define  $INCGUARD_VARNAME
    
#define  $VERSION_VARNAME   "$VERSION"
    
/////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                        HEADERS FOLLOW:
//
/////////////////////////////////////////////////////////////////////////////////////////////////
    
---EOF---
    
    
    
for f in $HEADERS; do
    print_converted_contents_of  $f
done
    
    
    
cat << ---EOF---
    
/////////////////////////////////////////////////////////////////////////////////////////////////
//
//                                        SOURCES FOLLOW:
//
/////////////////////////////////////////////////////////////////////////////////////////////////
    
#ifndef  $HDRREQ_VARNAME
    
---EOF---
    
    
    
for f in $SOURCES; do
    print_converted_contents_of  $f
done
    
    
    
cat << ---EOF---
    
#endif  // ndef  $HDRREQ_VARNAME
    
#undef  $HDRREQ_VARNAME
    
#endif  // ndef  $INCGUARD_VARNAME
    
---EOF---
