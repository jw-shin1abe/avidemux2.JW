// automatically generated by admSerialization.py, do not edit!
#include "ADM_default.h"
#include "ADM_paramList.h"
#include "ADM_coreJson.h"
#include "artVHS.h"
bool  artVHS_jserialize(const char *file, const artVHS *key){
admJson json;
json.addFloat("lumaBW",key->lumaBW);
json.addFloat("chromaBW",key->chromaBW);
json.addBool("lumaNoDelay",key->lumaNoDelay);
json.addBool("chromaNoDelay",key->chromaNoDelay);
json.addFloat("unSync",key->unSync);
json.addFloat("unSyncFilter",key->unSyncFilter);
json.addFloat("noise",key->noise);
return json.dumpToFile(file);
};
bool  artVHS_jdeserialize(const char *file, const ADM_paramList *tmpl,artVHS *key){
admJsonToCouple json;
CONFcouple *c=json.readFromFile(file);
if(!c) {ADM_error("Cannot read json file");return false;}
bool r= ADM_paramLoadPartial(c,tmpl,key);
delete c;
return r;
};
