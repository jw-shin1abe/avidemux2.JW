// automatically generated by admSerialization.py, do not edit!
#include "ADM_default.h"
#include "ADM_paramList.h"
#include "ADM_coreJson.h"
#include "artGrid.h"
bool  artGrid_jserialize(const char *file, const artGrid *key){
admJson json;
json.addUint32("size",key->size);
json.addBool("roll",key->roll);
return json.dumpToFile(file);
};
bool  artGrid_jdeserialize(const char *file, const ADM_paramList *tmpl,artGrid *key){
admJsonToCouple json;
CONFcouple *c=json.readFromFile(file);
if(!c) {ADM_error("Cannot read json file");return false;}
bool r= ADM_paramLoadPartial(c,tmpl,key);
delete c;
return r;
};
