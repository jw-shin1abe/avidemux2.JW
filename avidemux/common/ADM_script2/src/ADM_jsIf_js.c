/*
--- DO NOT EDIT THIS FILE !!! ---

This file has been generated automatically with 'jsapigen'.

jsapigen is a glue-code generator for SpiderMonkey. It is distributed
under the conditions of version 3 of the GNU General Public License.
Please have a look at http://jsapigen.sourceforge.net.

This file is NOT part of jsapigen and is NOT necessarily covered by
jsapigen's license. For licensing information regarding this file,
please refer to the software package which it is part of.

*/
#include <string.h>
#include <wchar.h>
#include <jsapi.h>
#ifndef JS_THREADSAFE
#if JS_VERSION <= 170
#define jsrefcount int
#define JS_BeginRequest(cx)
#define JS_EndRequest(cx)
#define JS_SuspendRequest(cx)
#define JS_ResumeRequest(cx, saveDepth)
#endif
#endif
#ifndef JS_FS
#define JS_FS(name, call, nargs, flags, extra) {name, call, nargs, flags, extra}
#endif
#ifndef JS_FS_END
#define JS_FS_END {NULL, NULL, 0, 0, 0}
#endif
static JSBool
jjadmPopupError(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *var2;
    char *var7;
    int var6;
    int var8;
    jsval var9;
    JSString *var10;
    jsval var71;
    size_t var11;
    size_t var12;
    int var14;
    jschar *var13;
    jsval var72;
    JSBool var1;
    var2 = NULL;
    var7 = NULL;
    var6 = 0;
    var8 = 0;
    var9 = JSVAL_NULL;
    var10 = NULL;
    var71 = JSVAL_NULL;
    var11 = 0;
    var12 = 0;
    var14 = 0;
    var13 = NULL;
    var72 = JSVAL_NULL;
    var1 = JS_FALSE;
    var2 = obj;
    var6 = argc;
    var8 = 0;
    var8 = var8 < var6;
    if (var8) {
    var9 = argv[0];
    var10 = JS_ValueToString(cx, var9);
    if (!var10) {
        goto do_return;
    }
    var71 = STRING_TO_JSVAL(var10);
    argv[argc+0] = var71;
    var11 = JS_GetStringLength(var10);
    var12 = 1;
    var12 += var11;
    var7 = JS_malloc(cx, var12);
    if (!var7) {
        goto do_return;
    }
    var14 = 1;
    var13 = JS_GetStringChars(var10);
    var72 = STRING_TO_JSVAL(var10);
    argv[argc+1] = var72;
    {
        size_t i;
        for (i = 0; i < var11; ++i) {
            var7[i] = wctob(var13[i]);
        }
        var7[var11] = '\0';
    }
    }
    jsPopupError(var7);
    var1 = JS_TRUE;
    do_return:
    if (var14) {
        JS_free(cx, var7);
        var7 = NULL;
        var14 = 0;
    }
    return var1;
}
static JSBool
jjadmpPopupInfo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *var16;
    char *var21;
    int var20;
    int var22;
    jsval var23;
    JSString *var24;
    jsval var73;
    size_t var25;
    size_t var26;
    int var28;
    jschar *var27;
    jsval var74;
    JSBool var15;
    var16 = NULL;
    var21 = NULL;
    var20 = 0;
    var22 = 0;
    var23 = JSVAL_NULL;
    var24 = NULL;
    var73 = JSVAL_NULL;
    var25 = 0;
    var26 = 0;
    var28 = 0;
    var27 = NULL;
    var74 = JSVAL_NULL;
    var15 = JS_FALSE;
    var16 = obj;
    var20 = argc;
    var22 = 0;
    var22 = var22 < var20;
    if (var22) {
    var23 = argv[0];
    var24 = JS_ValueToString(cx, var23);
    if (!var24) {
        goto do_return;
    }
    var73 = STRING_TO_JSVAL(var24);
    argv[argc+0] = var73;
    var25 = JS_GetStringLength(var24);
    var26 = 1;
    var26 += var25;
    var21 = JS_malloc(cx, var26);
    if (!var21) {
        goto do_return;
    }
    var28 = 1;
    var27 = JS_GetStringChars(var24);
    var74 = STRING_TO_JSVAL(var24);
    argv[argc+1] = var74;
    {
        size_t i;
        for (i = 0; i < var25; ++i) {
            var21[i] = wctob(var27[i]);
        }
        var21[var25] = '\0';
    }
    }
    jsPopupInfo(var21);
    var15 = JS_TRUE;
    do_return:
    if (var28) {
        JS_free(cx, var21);
        var21 = NULL;
        var28 = 0;
    }
    return var15;
}
static JSBool
jjadmPrint(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *var30;
    char *var35;
    int var34;
    int var36;
    jsval var37;
    JSString *var38;
    jsval var75;
    size_t var39;
    size_t var40;
    int var42;
    jschar *var41;
    jsval var76;
    JSBool var29;
    var30 = NULL;
    var35 = NULL;
    var34 = 0;
    var36 = 0;
    var37 = JSVAL_NULL;
    var38 = NULL;
    var75 = JSVAL_NULL;
    var39 = 0;
    var40 = 0;
    var42 = 0;
    var41 = NULL;
    var76 = JSVAL_NULL;
    var29 = JS_FALSE;
    var30 = obj;
    var34 = argc;
    var36 = 0;
    var36 = var36 < var34;
    if (var36) {
    var37 = argv[0];
    var38 = JS_ValueToString(cx, var37);
    if (!var38) {
        goto do_return;
    }
    var75 = STRING_TO_JSVAL(var38);
    argv[argc+0] = var75;
    var39 = JS_GetStringLength(var38);
    var40 = 1;
    var40 += var39;
    var35 = JS_malloc(cx, var40);
    if (!var35) {
        goto do_return;
    }
    var42 = 1;
    var41 = JS_GetStringChars(var38);
    var76 = STRING_TO_JSVAL(var38);
    argv[argc+1] = var76;
    {
        size_t i;
        for (i = 0; i < var39; ++i) {
            var35[i] = wctob(var41[i]);
        }
        var35[var39] = '\0';
    }
    }
    jsPrint(var35);
    var29 = JS_TRUE;
    do_return:
    if (var42) {
        JS_free(cx, var35);
        var35 = NULL;
        var42 = 0;
    }
    return var29;
}
static JSBool
jjprint(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *var44;
    char *var49;
    int var48;
    int var50;
    jsval var51;
    JSString *var52;
    jsval var77;
    size_t var53;
    size_t var54;
    int var56;
    jschar *var55;
    jsval var78;
    JSBool var43;
    var44 = NULL;
    var49 = NULL;
    var48 = 0;
    var50 = 0;
    var51 = JSVAL_NULL;
    var52 = NULL;
    var77 = JSVAL_NULL;
    var53 = 0;
    var54 = 0;
    var56 = 0;
    var55 = NULL;
    var78 = JSVAL_NULL;
    var43 = JS_FALSE;
    var44 = obj;
    var48 = argc;
    var50 = 0;
    var50 = var50 < var48;
    if (var50) {
    var51 = argv[0];
    var52 = JS_ValueToString(cx, var51);
    if (!var52) {
        goto do_return;
    }
    var77 = STRING_TO_JSVAL(var52);
    argv[argc+0] = var77;
    var53 = JS_GetStringLength(var52);
    var54 = 1;
    var54 += var53;
    var49 = JS_malloc(cx, var54);
    if (!var49) {
        goto do_return;
    }
    var56 = 1;
    var55 = JS_GetStringChars(var52);
    var78 = STRING_TO_JSVAL(var52);
    argv[argc+1] = var78;
    {
        size_t i;
        for (i = 0; i < var53; ++i) {
            var49[i] = wctob(var55[i]);
        }
        var49[var53] = '\0';
    }
    }
    jsPrint2(var49);
    var43 = JS_TRUE;
    do_return:
    if (var56) {
        JS_free(cx, var49);
        var49 = NULL;
        var56 = 0;
    }
    return var43;
}
static JSBool
jjhelp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSObject *var58;
    char *var63;
    int var62;
    int var64;
    jsval var65;
    JSString *var66;
    jsval var79;
    size_t var67;
    size_t var68;
    int var70;
    jschar *var69;
    jsval var80;
    JSBool var57;
    var58 = NULL;
    var63 = NULL;
    var62 = 0;
    var64 = 0;
    var65 = JSVAL_NULL;
    var66 = NULL;
    var79 = JSVAL_NULL;
    var67 = 0;
    var68 = 0;
    var70 = 0;
    var69 = NULL;
    var80 = JSVAL_NULL;
    var57 = JS_FALSE;
    var58 = obj;
    var62 = argc;
    var64 = 0;
    var64 = var64 < var62;
    if (var64) {
    var65 = argv[0];
    var66 = JS_ValueToString(cx, var65);
    if (!var66) {
        goto do_return;
    }
    var79 = STRING_TO_JSVAL(var66);
    argv[argc+0] = var79;
    var67 = JS_GetStringLength(var66);
    var68 = 1;
    var68 += var67;
    var63 = JS_malloc(cx, var68);
    if (!var63) {
        goto do_return;
    }
    var70 = 1;
    var69 = JS_GetStringChars(var66);
    var80 = STRING_TO_JSVAL(var66);
    argv[argc+1] = var80;
    {
        size_t i;
        for (i = 0; i < var67; ++i) {
            var63[i] = wctob(var69[i]);
        }
        var63[var67] = '\0';
    }
    }
    jsHelp(var63);
    var57 = JS_TRUE;
    do_return:
    if (var70) {
        JS_free(cx, var63);
        var63 = NULL;
        var70 = 0;
    }
    return var57;
}
static JSPropertySpec jj_static_ps[] = {
    {NULL, 0, 0, NULL, NULL}
};
static JSPropertySpec jj_ps[] = {
    {NULL, 0, 0, NULL, NULL}
};
static JSFunctionSpec jj_static_fs[] = {
    JS_FS("admPopupError", jjadmPopupError, 1, 0, 2),
    JS_FS("admpPopupInfo", jjadmpPopupInfo, 1, 0, 2),
    JS_FS("admPrint", jjadmPrint, 1, 0, 2),
    JS_FS("print", jjprint, 1, 0, 2),
    JS_FS("help", jjhelp, 1, 0, 2),
    JS_FS_END
};
static JSFunctionSpec jj_fs[] = {
    JS_FS("admPopupError", jjadmPopupError, 1, 0, 2),
    JS_FS("admpPopupInfo", jjadmpPopupInfo, 1, 0, 2),
    JS_FS("admPrint", jjadmPrint, 1, 0, 2),
    JS_FS("print", jjprint, 1, 0, 2),
    JS_FS("help", jjhelp, 1, 0, 2),
    JS_FS_END
};

JSFunctionSpec  *jsGetIfFunctions(void)
{
        return jj_static_fs;
}


