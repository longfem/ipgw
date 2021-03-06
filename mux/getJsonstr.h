#include <string.h>
#include "list.h"
#include "datastructdef.h"
#include "clsMuxprgInfoGet.h"
#include "devinfo.h"
#include "freePrograms.h"
#include "cJSON.h"

#ifndef _IPGW_GETJSONSTR_GET_H_
#define _IPGW_GETJSONSTR_GET_H_

void getPrgsJson(char *ip, char *outprg, char *lan);
void getbaseJson(char *ip, char *outprg);
int getIpReadJson(char *ip, char *outprg);
int getInputStsJson(char *ip, char *outprg);
void getPrgoutListJson(char *outprg);
void getDb3Json(char *outprg);
void getSPTSCHJson(int prgnum, int chnid, char *outprg);
void getIPINJson(char *ip, int flag, char *outprg);
void getBackupJson(char *ip, char *outprg);

#endif
