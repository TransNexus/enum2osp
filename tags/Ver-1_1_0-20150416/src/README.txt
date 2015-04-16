#
# The following two files should be put into BIND source tree.
#
# $BIND_SRC/bin/named/ospdb.c
# $BIND_SRC/bin/named/ospdb.h
#

#
# The following six files should replace the existing files in BIND source tree.
# Note, the files are for bind-9.10.1-P1. For other versions, please only insert ENUM2OSP related code.
#
# $BIND_SRC/bin/named/client.c
# $BIND_SRC/bin/named/include/named/client.h
# $BIND_SRC/bin/named/main.c
# $BIND_SRC/bin/named/Makefile.in
# $BIND_SRC/lib/dns/include/dns/message.h
# $BIND_SRC/lib/dns/sdb.c
#

#
# The patch file, bind-9.10.1-P1_enum2osp-1.1.0.patch, is provided to simplify the build procedure.
# Note, the patch file is only for bind-9.10.1-P1.
#
# $ patch -p1 -d $BIND_SRC < bind-9.10.1-P1_enum2osp-1.1.0.patch
#
