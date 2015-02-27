#
# The following two source files should be put into BIND source tree.
#
# $BIND_SRC/bin/named/ospdb.c
# $BIND_SRC/bin/named/ospdb.h
#

#
# The following three files should replace the files in BIND source tree.
# Note, the files are for bind-9.10.1-P1. For other versions, please only insert ENUM2OSP related code.
#
# $BIND_SRC/bin/named/main.c
# $BIND_SRC/bin/named/Makefile.in
# $BIND_SRC/lib/dns/sdb.c
#

diff -Nur bind-9.10.1-P1/bin/named/main.c bind-9.10.1-P1.osp/bin/named/main.c
--- bind-9.10.1-P1/bin/named/main.c	2014-11-20 18:56:37.000000000 -0500
+++ bind-9.10.1-P1.osp/bin/named/main.c	2015-02-04 08:32:53.489150137 -0500
@@ -85,6 +85,7 @@
  * Include header files for database drivers here.
  */
 /* #include "xxdb.h" */
+#include "ospdb.h"
 
 #ifdef CONTRIB_DLZ
 /*
@@ -1016,6 +1017,7 @@
 	 * Add calls to register sdb drivers here.
 	 */
 	/* xxdb_init(); */
+	ospdb_init();
 
 #ifdef ISC_DLZ_DLOPEN
 	/*
@@ -1056,6 +1058,7 @@
 	 * Add calls to unregister sdb drivers here.
 	 */
 	/* xxdb_clear(); */
+	ospdb_clear();
 
 #ifdef CONTRIB_DLZ
 	/*
diff -Nur bind-9.10.1-P1/bin/named/Makefile.in bind-9.10.1-P1.osp/bin/named/Makefile.in
--- bind-9.10.1-P1/bin/named/Makefile.in	2014-11-20 18:56:37.000000000 -0500
+++ bind-9.10.1-P1.osp/bin/named/Makefile.in	2015-02-04 08:32:16.673013703 -0500
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
 
diff -Nur bind-9.10.1-P1/lib/dns/sdb.c bind-9.10.1-P1.osp/lib/dns/sdb.c
--- bind-9.10.1-P1/lib/dns/sdb.c	2014-11-20 18:56:37.000000000 -0500
+++ bind-9.10.1-P1.osp/lib/dns/sdb.c	2015-02-04 08:34:00.883230308 -0500
@@ -855,6 +855,13 @@
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
