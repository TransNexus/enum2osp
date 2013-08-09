#
# The following two source files should be put into BIND source tree.
#
# $BIND_SRC/bin/named/ospdb.c
# $BIND_SRC/bin/named/ospdb.h
#

#
# The following three files should replace the files in BIND source tree.
# Note, the files are for bind-9.9.3-P2. For other versions, please only insert ENUM2OSP related code.
#
# $BIND_SRC/bin/named/main.c
# $BIND_SRC/bin/named/Makefile.in
# $BIND_SRC/lib/dns/sdb.c
#

diff -Nur bind-9.9.3-P2/bin/named/main.c bind-9.9.3-P2.osp/bin/named/main.c
--- bind-9.9.3-P2/bin/named/main.c	2013-07-16 18:13:06.000000000 -0400
+++ bind-9.9.3-P2.osp/bin/named/main.c	2013-08-08 22:52:17.480221980 -0400
@@ -82,6 +82,7 @@
  * Include header files for database drivers here.
  */
 /* #include "xxdb.h" */
+#include "ospdb.h"
 
 #ifdef CONTRIB_DLZ
 /*
@@ -904,6 +905,7 @@
 	 * Add calls to register sdb drivers here.
 	 */
 	/* xxdb_init(); */
+	ospdb_init();
 
 #ifdef ISC_DLZ_DLOPEN
 	/*
@@ -940,6 +942,7 @@
 	 * Add calls to unregister sdb drivers here.
 	 */
 	/* xxdb_clear(); */
+	ospdb_clear();
 
 #ifdef CONTRIB_DLZ
 	/*
diff -Nur bind-9.9.3-P2/bin/named/Makefile.in bind-9.9.3-P2.osp/bin/named/Makefile.in
--- bind-9.9.3-P2/bin/named/Makefile.in	2013-07-16 18:13:06.000000000 -0400
+++ bind-9.9.3-P2.osp/bin/named/Makefile.in	2013-08-08 22:52:52.135762363 -0400
@@ -34,10 +34,10 @@
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
 
diff -Nur bind-9.9.3-P2/lib/dns/sdb.c bind-9.9.3-P2.osp/lib/dns/sdb.c
--- bind-9.9.3-P2/lib/dns/sdb.c	2013-07-16 18:13:06.000000000 -0400
+++ bind-9.9.3-P2.osp/lib/dns/sdb.c	2013-08-08 22:53:30.184388401 -0400
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
