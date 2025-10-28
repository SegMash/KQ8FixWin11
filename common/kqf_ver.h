#ifndef KQF_VER_H_
#define KQF_VER_H_

#define KQF_VERF_MAJOR 1
#define KQF_VERF_MINOR 99
#define KQF_VERF_PATCH 19
#define KQF_VERS_PATCH "s"

#define KQF_VERS_TAG "~unstable"
#ifdef KQF_DEBUG
# define KQF_VERF_FLAGS 9
# define KQF_VERS_BUILD " [debug]"
#else
# define KQF_VERF_FLAGS 8
# define KQF_VERS_BUILD
#endif
#define KQF_VERF_MKSTR(x) #x
#define KQF_VERF_TOSTR(x) KQF_VERF_MKSTR(x)
#define KQF_VERS_UIVER KQF_VERF_TOSTR(KQF_VERF_MAJOR) "." KQF_VERF_TOSTR(KQF_VERF_MINOR) KQF_VERS_PATCH KQF_VERS_TAG KQF_VERS_BUILD

#define KQF_VERS_CNAME "Nico Bendlin"
#define KQF_VERS_EMAIL KQF_VERS_CNAME " <nico@nicode.net>"
#define KQF_VERS_LCOPY "(c) 2014,2016,2019 " KQF_VERS_EMAIL
#define KQF_VERS_INAME "kqmoefix"
#define KQF_VERS_FDESC "King's Quest: Mask of Eternity - Shim"
#define KQF_VERS_PNAME "King's Quest: Mask of Eternity"
#define KQF_VERS_PVERS  "1.0.0.3"
#define KQF_VERP_MAJOR 1
#define KQF_VERP_MINOR 0
#define KQF_VERP_PATCH 0
#define KQF_VERP_BUILD 3

#endif
