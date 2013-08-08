#
# The following two source files should be put into BIND source tree.
#
# $BIND_SRC/bin/named/ospdb.c
# $BIND_SRC/bin/named/ospdb.h
#

#
# The following three files should replace the files in BIND source tree.
# Note, the files are for bind-9.9.2-P2. For other versions, please only insert ENUM2OSP related code.
#
# $BIND_SRC/bin/named/main.c
# $BIND_SRC/bin/named/Makefile.in
# $BIND_SRC/lib/dns/sdb.c
#

diff -Nur bind-9.9.2-P2/bin/named/main.c bind-9.9.2-P2.osp/bin/named/main.c
--- bind-9.9.2-P2/bin/named/main.c	2013-03-07 00:56:08.000000000 +0800
+++ bind-9.9.2-P2.osp/bin/named/main.c	2013-04-25 15:33:53.346373590 +0800
@@ -82,6 +82,7 @@
  * Include header files for database drivers here.
  */
 /* #include "xxdb.h" */
+#include "ospdb.h"
 
 #ifdef CONTRIB_DLZ
 /*
@@ -892,6 +893,7 @@
 	 * Add calls to register sdb drivers here.
 	 */
 	/* xxdb_init(); */
+	ospdb_init();
 
 #ifdef ISC_DLZ_DLOPEN
 	/*
@@ -928,6 +930,7 @@
 	 * Add calls to unregister sdb drivers here.
 	 */
 	/* xxdb_clear(); */
+	ospdb_clear();
 
 #ifdef CONTRIB_DLZ
 	/*
diff -Nur bind-9.9.2-P2/bin/named/Makefile.in bind-9.9.2-P2.osp/bin/named/Makefile.in
--- bind-9.9.2-P2/bin/named/Makefile.in	2013-03-07 00:56:08.000000000 +0800
+++ bind-9.9.2-P2.osp/bin/named/Makefile.in	2013-05-10 14:45:42.091251195 +0800
@@ -28,10 +28,10 @@
 #
 # Add database drivers here.
 #
-DBDRIVER_OBJS =
-DBDRIVER_SRCS =
-DBDRIVER_INCLUDES =
-DBDRIVER_LIBS =
+DBDRIVER_OBJS = ospdb.o
+DBDRIVER_SRCS = ospdb.c
+DBDRIVER_INCLUDES = ospdb.h
+DBDRIVER_LIBS = -losptk -lssl -lpthread -lm
 
 DLZ_DRIVER_DIR =	${top_srcdir}/contrib/dlz/drivers
 
diff -Nur bind-9.9.2-P2/lib/dns/sdb.c bind-9.9.2-P2.osp/lib/dns/sdb.c
--- bind-9.9.2-P2/lib/dns/sdb.c	2013-03-07 00:56:08.000000000 +0800
+++ bind-9.9.2-P2.osp/lib/dns/sdb.c	2013-05-06 15:18:31.669794810 +0800
@@ -856,6 +856,13 @@
 	flags = sdb->implementation->flags;
 	i = (flags & DNS_SDBFLAG_DNS64) != 0 ? nlabels : olabels;
 	for (; i <= nlabels; i++) {
+
+		/* For ENUM2OSP start */
+		if ((i != olabels) && (i != nlabels)) {
+			continue;
+		}
+		/* For ENUM2OSP end */
+
 		/*
 		 * Look up the next label.
 		 */
