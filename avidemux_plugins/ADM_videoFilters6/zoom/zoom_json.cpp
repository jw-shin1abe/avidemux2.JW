// automatically generated by admSerialization.py, do not edit!
#include "ADM_default.h"
#include "ADM_paramList.h"
#include "ADM_coreJson.h"
#include "zoom.h"
bool  zoom_jserialize(const char *file, const zoom *key){
admJson json;
json.addUint32("top",key->top);
json.addUint32("bottom",key->bottom);
json.addUint32("left",key->left);
json.addUint32("right",key->right);
json.addUint32("ar_select",key->ar_select);
json.addUint32("algo",key->algo);
json.addUint32("pad",key->pad);
json.addFloat("tolerance",key->tolerance);
return json.dumpToFile(file);
};
bool  zoom_jdeserialize(const char *file, const ADM_paramList *tmpl,zoom *key){
admJsonToCouple json;
CONFcouple *c=json.readFromFile(file);
if(!c) {ADM_error("Cannot read json file");return false;}
bool r= ADM_paramLoadPartial(c,tmpl,key);
delete c;
return r;
};
