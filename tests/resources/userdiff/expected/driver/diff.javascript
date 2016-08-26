diff --git a/files/file.javascript b/files/file.javascript
index 6d7415d..3ddf7da 100644
--- a/files/file.javascript
+++ b/files/file.javascript
@@ -37,3 +37,4 @@ function getViewportH ()
     var client = docElem['clientHeight'],
-      inner = window['innerHeight'];
+      inner = window['innerHeight'],
+      sample = window['otherProperty'];
 
@@ -48,3 +49,3 @@ function getOffset (el)
       if (!isNaN(el.offsetTop)) {
-        offsetTop += el.offsetTop;
+        offsetTop += el.offsetTop + 1;
       }
@@ -64,8 +65,7 @@ function isElementInViewport (el, h)
         viewed = scrolled + getViewportH(),
-        elH = el.offsetHeight,
         elTop = getOffset(el).top,
-        elBottom = elTop + elH,
+        elBottom = elTop + el.offsetHeight,
         h = h || 0;
 
-    return (elTop + elH * h) <= viewed && (elBottom) >= scrolled;
+    return (elTop + el.offsetHeight * h) <= viewed && (elBottom) >= scrolled;
   }
@@ -81,4 +81,2 @@ _init: function ()
 
-  //  Initialize all scrollreveals, triggering all
-  //  reveals on visible elements.
       this.elems.forEach(function (el, i) {
@@ -92,3 +90,3 @@ var scrollHandler = function ()
             self._scrollPage();
-          }, 60);
+          }, 61);
         }
@@ -122,2 +120,3 @@ _scrollPage: function ()
         this.scrolled = false;
+		this.tested = true;
     },
