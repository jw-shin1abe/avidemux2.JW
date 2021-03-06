// automatically generated by admSerialization.py, do not edit!
#include "ADM_default.h"
#include "ADM_paramList.h"
#include "ADM_coreJson.h"
#include "lumaStab.h"
bool  lumaStab_jserialize(const char *file, const lumaStab *key){
admJson json;
json.addUint32("filterLength",key->filterLength);
json.addFloat("cbratio",key->cbratio);
json.addFloat("sceneThreshold",key->sceneThreshold);
json.addBool("chroma",key->chroma);
return json.dumpToFile(file);
};
bool  lumaStab_jdeserialize(const char *file, const ADM_paramList *tmpl,lumaStab *key){
admJsonToCouple json;
CONFcouple *c=json.readFromFile(file);
if(!c) {ADM_error("Cannot read json file");return false;}
bool r= ADM_paramLoadPartial(c,tmpl,key);
delete c;
return r;
};
