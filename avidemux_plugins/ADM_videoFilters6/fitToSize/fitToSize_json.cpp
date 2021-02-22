// automatically generated by admSerialization.py, do not edit!
#include "ADM_default.h"
#include "ADM_paramList.h"
#include "ADM_coreJson.h"
#include "fitToSize.h"
bool  fitToSize_jserialize(const char *file, const fitToSize *key){
admJson json;
json.addUint32("width",key->width);
json.addUint32("height",key->height);
json.addUint32("algo",key->algo);
json.addUint32("roundup",key->roundup);
json.addUint32("pad",key->pad);
json.addFloat("tolerance",key->tolerance);
return json.dumpToFile(file);
};
bool  fitToSize_jdeserialize(const char *file, const ADM_paramList *tmpl,fitToSize *key){
admJsonToCouple json;
CONFcouple *c=json.readFromFile(file);
if(!c) {ADM_error("Cannot read json file");return false;}
bool r= ADM_paramLoadPartial(c,tmpl,key);
delete c;
return r;
};
