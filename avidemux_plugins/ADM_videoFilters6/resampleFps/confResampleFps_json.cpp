// automatically generated by admSerialization.py, do not edit!
#include "ADM_default.h"
#include "ADM_paramList.h"
#include "ADM_coreJson.h"
#include "confResampleFps.h"
bool  confResampleFps_jserialize(const char *file, const confResampleFps *key){
admJson json;
json.addUint32("mode",key->mode);
json.addUint32("newFpsDen",key->newFpsDen);
json.addUint32("newFpsNum",key->newFpsNum);
json.addUint32("interpolation",key->interpolation);
return json.dumpToFile(file);
};
bool  confResampleFps_jdeserialize(const char *file, const ADM_paramList *tmpl,confResampleFps *key){
admJsonToCouple json;
CONFcouple *c=json.readFromFile(file);
if(!c) {ADM_error("Cannot read json file");return false;}
bool r= ADM_paramLoadPartial(c,tmpl,key);
delete c;
return r;
};
