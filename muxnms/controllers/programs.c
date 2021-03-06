/*
    programs Controller for esp-html-mvc (esp-html-mvc)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "esp.h"
#include "cJSON.h"
#include "ipgw.h"
#include "getJsonstr.h"
#include "clsUcBase.h"
#include "ucIpDest.h"
#include "devinfo.h"
#include "clsMuxprgInfoGet.h"

//char *tmpip = "192.168.1.49";
//conn->rx->parsedUri->host

extern ClsProgram_st clsProgram;
extern ClsGlobal_st  clsGlobal;


static void rendersts(char *str,int status)
{
	cJSON *result = cJSON_CreateObject();
	char* jsonstring;
	cJSON_AddNumberToObject(result,"sts", status);
	jsonstring = cJSON_PrintUnformatted(result);
	memcpy(str, jsonstring, strlen(jsonstring));
	//释放内存
	cJSON_Delete(result);
	free(jsonstring);
}

static int isAuthed(){
    char str[32] = {0};
    cchar *isAuthed = getSessionVar("isAuthed");
    if(isAuthed == NULL){
        rendersts(str, 8);
        render(str);
        return 1;
    }
    if(strcmp(isAuthed, "false") == 0){
        rendersts(str, 8);
        render(str);
        return 1;
    }
    return 0;
}

static void getdevNetFun(HttpConn *conn){
    char str[32] = {0};
    if(isAuthed()){
        return;
    }
	printf("remote ip ====%s\n", conn->ip);
	sprintf(str, "ntpdate %s", conn->ip);
	system(str);
	memset(str, 0, sizeof(str));
    RefreshIpInOutMode(conn->rx->parsedUri->host);
    cJSON *result = cJSON_CreateObject();
    char* jsonstring;
    cJSON_AddNumberToObject(result,"mode", clsGlobal.ipGwDb->devNetFun);
    jsonstring = cJSON_PrintUnformatted(result);
    memcpy(str, jsonstring, strlen(jsonstring));
    //释放内存
    cJSON_Delete(result);
    free(jsonstring);
    render(str);
}

static void getDevinfo(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char pProg[256] = {0};
    int r = 0;
    EdiField *src;
    getbaseJson(conn->rx->parsedUri->host, pProg);
    render(pProg);
    //delete optlog 7days ago
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    time_t curTime;
    time(&curTime);
    EdiGrid *opts = ediReadTable(db, "optlog");
    if(opts != NULL){
       for (r = 0; r < opts->nrecords; r++) {
           src = opts->records[r]->fields;
           src += 4;
           if(atoi(src->value) < (curTime - 7*24*3600)){
                ediRemoveRec(db, "optlog", opts->records[r]->id);
           }
       }
    }
}

static void getPrgs(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
	char str[32] = {0};
    char outprg[102400] = {0};
    MprJson *jsonparam = httpGetParams(conn);
    int flag = atoi(mprGetJson(jsonparam, "flag"));
    if(flag==1){
        //search
        if(Search(conn->rx->parsedUri->host, 1)){
            rendersts(outprg, 6);
            render(outprg);
            return;
        }
    }
	cchar *lan = getSessionVar("language");
	if(lan == NULL){
		rendersts(str, 8);
        render(str);
        return 1;
	}
    getPrgsJson(conn->rx->parsedUri->host, outprg, lan);
    render(outprg);
}

static void ipRead(HttpConn *conn) {
    RefreshIpInOutMode(conn->rx->parsedUri->host);
    if(isAuthed()){
        return;
    }
    char outprg[128] = {0};
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(outprg, 8);
        render(outprg);
        return;
    }
    clsGlobal._moduleBaseCmd = 0xf1;
    if(!getIpReadJson(conn->rx->parsedUri->host, outprg)){
        rendersts(outprg, 8);
        render(outprg);
        return;
    }
    render(outprg);
}

static void readinputsts(HttpConn *conn) {
    char str[64] = {0};
    if(!getInputStsJson(conn->rx->parsedUri->host, str)){
        rendersts(str, 8);
        render(str);
        return;
    }
    render(str);
}

static void paramswriteAll(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char str[32] = {0};
    char optstr[256] = {0};
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(str, 8);
        render(str);
        return;
    }
    if((strcmp(role, "root") !=0) && (strcmp(role, "admin") !=0)){
        rendersts(str, 5);//无权限
        render(str);
        return;
    }

    MprJson *jsonparam = httpGetParams(conn);
    //printf("==========ParamsWriteAll===========%s\n", mprJsonToString (jsonparam, MPR_JSON_QUOTES));
    cchar *sip = mprGetJson(jsonparam, "ip");
    int port = atoi(mprGetJson(jsonparam, "port"));
    cchar *smac = mprGetJson(jsonparam, "mac");
    clsGlobal._ucDb->port = port;
    char *newip = strtok(sip, ".");
    int i=0, tmp = 0;
    while(newip)
    {
        clsGlobal._ucDb->ip[i] = (unsigned char)atoi(newip);
        newip = strtok(NULL, ".");
        i++;
    }
    //printf("===ip==%d.%d.%d.%d\n", clsGlobal._ucDb->ip[0], clsGlobal._ucDb->ip[1],clsGlobal._ucDb->ip[2],clsGlobal._ucDb->ip[3]);
    i=0;
    char *tmpmac = strtok(smac, ":");
    while(tmpmac)
    {
        sscanf(tmpmac,"%x", &tmp);
        clsGlobal._ucDb->mac[i] = (unsigned char)tmp;
        tmpmac = strtok(NULL, ":");
        i++;
    }
    //printf("===mac==%x:%x:%x:%x:%x:%x\n", clsGlobal._ucDb->mac[0], clsGlobal._ucDb->mac[1],clsGlobal._ucDb->mac[2],clsGlobal._ucDb->mac[3],clsGlobal._ucDb->mac[4],clsGlobal._ucDb->mac[5]);
    int isGood = 1;
    isGood &= ParamWriteByBytesCmd(conn->rx->parsedUri->host, (unsigned char)1, clsGlobal._ucDb->ip, 4);
    isGood &= ParamWriteByBytesCmd(conn->rx->parsedUri->host, (unsigned char)2, clsGlobal._ucDb->mac, 6);
    isGood &= ParamWriteByIntCmd(conn->rx->parsedUri->host, (unsigned char)3, clsGlobal._ucDb->port, 2);

    if (isGood){
        isGood &= ParamWriteByIntCmd(conn->rx->parsedUri->host, (unsigned char)0xf0, 0, 0);
    }
    //add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }

    time_t curTime;
    time(&curTime);
	cchar *lan = getSessionVar("language");
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '用户下发发送源配置.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'User send source config.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}
    
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
    if(!isGood){
        rendersts(str, 6);
    }else{
        rendersts(str, 1);
    }
    render(str);

}

static void iptvRead(HttpConn *conn) {
    char str[32] = {0};
    if(!IptvRead(conn->rx->parsedUri->host)){
        rendersts(str, 6);
        render(str);
        return;
    }
    DeleteInvalidOutputChn();
    rendersts(str, 1);
    render(str);
}

static void outprgList(HttpConn *conn) {
    char outprg[10240] = {0};
    getPrgoutListJson(outprg);
    render(outprg);
}

static void setIpTvmode(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char str[64] = {0};
    char optstr[128] = {0};
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(str, 8);
        render(str);
        return;
    }

    MprJson *jsonparam = httpGetParams(conn);
    int mode = atoi(mprGetJson(jsonparam, "mode"));
    clsGlobal.ipGwDb->dvbIptvMode = mode;
    //add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }
    if(mode){
        memcpy(str, "SPTS",  strlen("SPTS"));
    }else{
        memcpy(str, "MPTS",  strlen("MPTS"));
    }
    time_t curTime;
    time(&curTime);
	cchar *lan = getSessionVar("language");
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '用户更改输出模式[%s].', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), str, curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'User changed output mode[%s].', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), str, curTime);
	}
    
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
    memset(str, 0, sizeof(str));
    rendersts(str, 1);
    render(str);
}


static void tbl_selctdprg(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char outprg[256] = {0};
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(outprg, 8);
        render(outprg);
        return;
    }
    MprJson *jsonparam = httpGetParams(conn);
    int index = atoi(mprGetJson(jsonparam, "index"));
    SetDb3(index);
    getDb3Json(outprg);
    render(outprg);
}

static void tree_selctdprg(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char outprg[512] = {0};
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(outprg, 8);
        render(outprg);
        return;
    }

    MprJson *jsonparam = httpGetParams(conn);
    int prgnum = atoi(mprGetJson(jsonparam, "prgnum"));
    getSPTSCHJson(prgnum, 1, outprg);
    render(outprg);
}

static void outchnprg_output(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char str[32] = {0};
    char optstr[256] = {0};
    MprJson *jsonparam = httpGetParams(conn);
    int prgId = atoi(mprGetJson(jsonparam, "prgId"));
    int inCh = atoi(mprGetJson(jsonparam, "inCh"));
    int outChnId = atoi(mprGetJson(jsonparam, "outChnId"));
    OutChnPrg_output(inCh, prgId, outChnId);
    EnableValidOutChn();
    //add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }
    time_t curTime;
    time(&curTime);
	cchar *lan = getSessionVar("language");
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '用户更改节目[%d]SPTS通道.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), prgId, curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'User changed programe[%d] SPTS channel.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), prgId, curTime);
	}
    
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
    rendersts(str, 1);
    render(str);
}

static void outChnConfig(HttpConn *conn) {
    char outprg[256] = {0};
    char optstr[256] = {0};
    if(isAuthed()){
        return;
    }
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(optstr, 8);
        render(optstr);
        return;
    }
    if((strcmp(role, "root") !=0) && (strcmp(role, "admin") !=0)){
        rendersts(optstr, 5);//无权限
        render(optstr);
        return;
    }
    MprJson *jsonparam = httpGetParams(conn);
    int port = atoi(mprGetJson(jsonparam, "port"));
    int outMode = atoi(mprGetJson(jsonparam, "outMode"));
    int outChnId = atoi(mprGetJson(jsonparam, "outchn"));
    cchar *ipstr = mprGetJson(jsonparam, "ipstr");
    cchar *macstr = mprGetJson(jsonparam, "macstr");
    int prgId = 0;
    clsGlobal._ucDb4->outMode = outMode == 0 ? 0 : 3;
    clsGlobal._ucDb4->port = port;
    char *newip = strtok(ipstr, ".");
    int i=0, tmp = 0;
    while(newip)
    {
        clsGlobal._ucDb4->ip[i] = (unsigned char)atoi(newip);
        newip = strtok(NULL, ".");
        i++;
    }
    i=0;
    char *tmpmac = strtok(macstr, ":");
    while(tmpmac)
    {
        sscanf(tmpmac,"%x", &tmp);
        clsGlobal._ucDb4->mac[i] = (unsigned char)tmp;
        tmpmac = strtok(NULL, ":");
        i++;
    }
    //clsGlobal._ucDb4->outputEnable = 1;

    outChnId--;
    UcIpDestDbSt3_st *ucDb = NULL;
    list_get(clsGlobal.ucIpDestDb, outChnId, &ucDb);
    memcpy(ucDb->ip, clsGlobal._ucDb4->ip, 4);
    memcpy(ucDb->mac, clsGlobal._ucDb4->mac, 6);
    ucDb->outMode = clsGlobal._ucDb4->outMode;
    ucDb->port = clsGlobal._ucDb4->port;
    ucDb->outputEnable = clsGlobal._ucDb4->outputEnable;
    if(clsGlobal._ucDb4->prgList != NULL){
        if(list_len(clsGlobal._ucDb4->prgList)>0){
            if(ucDb->prgList != NULL){
                freeUcIpDestPrg(ucDb->prgList);
                free(ucDb->prgList);
                ucDb->prgList = NULL;
            }
            ucDb->prgList = malloc(sizeof(list_t));
            list_init(ucDb->prgList);
            UcIpDestPrgMuxInfoSt_st *eachPrg = NULL;
            UcIpDestPrgMuxInfoSt_st *curPrg = malloc(sizeof(UcIpDestPrgMuxInfoSt_st));
            for(i=0;i<list_len(clsGlobal._ucDb4->prgList);i++){
                list_get(clsGlobal._ucDb4->prgList, i, &eachPrg);
                if(eachPrg->avPidListLen>0){
                    curPrg->avPidListLen = eachPrg->avPidListLen;
                    curPrg->avPidList = malloc(eachPrg->avPidListLen);
                    memcpy(curPrg->avPidList, eachPrg->avPidList, eachPrg->avPidListLen);
                }else{
                    curPrg->avPidListLen = 0;
                }
                curPrg->inChn = eachPrg->inChn;
                curPrg->pmtPID = eachPrg->pmtPID;
                curPrg->prgId = eachPrg->prgId;
                prgId = curPrg->prgId;
                list_append(ucDb->prgList, curPrg);
            }
        }
    }
    getDb3Json(outprg);
    //add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }
    time_t curTime;
    time(&curTime);
	cchar *lan = getSessionVar("language");
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '用户更改输出节目[%d]信息.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), prgId, curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'User changed output programe[%d] info.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), prgId, curTime);
	}
    
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
    render(outprg);
}


static void prgMuxSptsMap(HttpConn *conn) {
    char str[32] = {0};
    char optstr[256] = {0};
    if(isAuthed()){
        return;
    }
    int i = 0;
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(str, 8);
        render(str);
        return;
    }
    if((strcmp(role, "root") !=0) && (strcmp(role, "admin") !=0)){
        rendersts(str, 5);//无权限
        render(str);
        return;
    }
    int prgIndex = 0;
    UcIpDestDbSt3_st *db3 = NULL;
    ChannelProgramSt *pst = NULL;
    Dev_prgInfo_st *inprg = NULL;

    for(i = 0; i < list_len(clsGlobal.ucIpDestDb); i++)
    {
        list_get(clsGlobal.ucIpDestDb, i, &db3);
        if(db3->prgList != NULL){
            freeUcIpDestPrg(db3->prgList);
            free(db3->prgList);
            db3->prgList = NULL;
        }
        list_get(&clsProgram.inPrgList, 0, &pst);
        if(&pst->prgNodes != NULL){
            if(prgIndex < list_len(&pst->prgNodes)){
                db3->prgList = malloc(sizeof(list_t));
                list_init(db3->prgList);
                list_get(&pst->prgNodes, prgIndex, &inprg);
                UcIpDestPrgMuxInfoSt_st *prgMuxInfo = malloc(sizeof(UcIpDestPrgMuxInfoSt_st));
                prgMuxInfo->inChn = inprg->chnId+1;
                prgMuxInfo->prgId = inprg->prgNum;
                prgMuxInfo->avPidListLen = 0;
                list_append(db3->prgList, prgMuxInfo);
                prgIndex++;
            }
        }
    }
    EnableValidOutChn();
    //add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }
    time_t curTime;
    time(&curTime);
	cchar *lan = getSessionVar("language");
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '用户自动映射输出节目.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'User auto mapping output programes.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}
    
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
    rendersts(str, 1);
    render(str);
}

static void clearprgMux(HttpConn *conn) {
    char str[32] = {0};
    char optstr[256] = {0};
    if(isAuthed()){
        return;
    }
    int i = 0;
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(str, 8);
        render(str);
        return;
    }
    if((strcmp(role, "root") !=0) && (strcmp(role, "admin") !=0)){
        rendersts(str, 5);//无权限
        render(str);
        return;
    }
    UcIpDestDbSt3_st *db3 = NULL;
    for(i = 0; i < list_len(clsGlobal.ucIpDestDb); i++)
    {
        list_get(clsGlobal.ucIpDestDb, i, &db3);
        if(db3->prgList != NULL){
            freeUcIpDestPrg(db3->prgList);
            free(db3->prgList);
            db3->prgList = NULL;
        }
    }
    EnableValidOutChn();
    //add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }
    time_t curTime;
    time(&curTime);
	cchar *lan = getSessionVar("language");
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '用户清空输出节目.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'User cleared output programes.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}
    
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
    rendersts(str, 1);
    render(str);
}

static void muxprgwrite(HttpConn *conn) {
    char str[32] = {0};
    char optstr[256] = {0};
    if(isAuthed()){
        return;
    }
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(str, 8);
        render(str);
        return;
    }
    if((strcmp(role, "root") !=0) && (strcmp(role, "admin") !=0)){
        rendersts(str, 5);//无权限
        render(str);
        return;
    }
    MprJson *jsonparam = httpGetParams(conn);
	//printf("==========jsonparam===========%s\n", mprJsonToString (jsonparam, MPR_JSON_QUOTES));
    int ttl = atoi(mprGetJson(jsonparam, "ttl"));
	int outmode = atoi(mprGetJson(jsonparam, "outmode"));
    clsGlobal.ipGwDb->ttl = ttl;
	clsGlobal.ipGwDb->dvbIptvMode = outmode;
    EnableValidOutChn();
    if(CheckSameDest() == 0){
        rendersts(str, 2);
        render(str);
        return;
    }
    if(clsGlobal.ipGwDb->devNetFun == 0){
        if (!IpWrite(conn->rx->parsedUri->host)){
            rendersts(str, 6);
            render(str);
            return;
        }
        if (!IptvWrite(conn->rx->parsedUri->host)){
            rendersts(str, 6);
            render(str);
            return;
        }
    }else{
        if (!ParamsWriteAll(conn->rx->parsedUri->host, 0xff)){
            rendersts(str, 6);
            render(str);
			return;
        }
    }
    //add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }
    time_t curTime;
    time(&curTime);
	cchar *lan = getSessionVar("language");
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '用户下发输出配置.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'User sending output config.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}
	rendersts(str, 1);
    render(str);
}

static void refreshIpMode(HttpConn *conn) {
    char str[32] = {0};
    if(isAuthed()){
        return;
    }
    RefreshIpInOutMode(conn->rx->parsedUri->host);
    cJSON *result = cJSON_CreateObject();
    char* jsonstring;
    cJSON_AddNumberToObject(result,"mode", clsGlobal.ipGwDb->devNetFun);
    jsonstring = cJSON_PrintUnformatted(result);
    memcpy(str, jsonstring, strlen(jsonstring));
    //释放内存
    cJSON_Delete(result);
    free(jsonstring);
    render(str);	
}

static void netapply(HttpConn *conn) {
    char str[32] = {0};
    char optstr[256] = {0};
    if(isAuthed()){
        return;
    }
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(str, 8);
        render(str);
        return;
    }
    if((strcmp(role, "root") !=0) && (strcmp(role, "admin") !=0)){
        rendersts(str, 5);//无权限
        render(str);
        return;
    }
    MprJson *jsonparam = httpGetParams(conn);
    int mode = atoi(mprGetJson(jsonparam, "mode"));
    clsGlobal.ipGwDb->devNetFun = mode;
    NetApply(conn->rx->parsedUri->host);

    //add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }
    time_t curTime;
    time(&curTime);
	cchar *lan = getSessionVar("language");
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '用户修改网络接口设置.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'User modified Net interface mode.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}
    
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
    rendersts(str, 1);
    render(str);
	rebootDevice(conn->rx->parsedUri->host);
}

static void readipIN(HttpConn *conn) {
    char outprg[256] = {0};
    if(isAuthed()){
        return;
    }
    clsGlobal._moduleBaseCmd = 0xf3;
    clsGlobal._moduleId = 1;
    MprJson *jsonparam = httpGetParams(conn);
    int flag = atoi(mprGetJson(jsonparam, "flag"));
    getIPINJson(conn->rx->parsedUri->host, flag, outprg);

    render(outprg);
}

static void readipinsts(HttpConn *conn) {
    char outprg[256] = {0};
    if(isAuthed()){
        return;
    }
    MprJson *jsonparam = httpGetParams(conn);
    int flag = atoi(mprGetJson(jsonparam, "flag"));
    getInputStatusJson(conn->rx->parsedUri->host, outprg);

    render(outprg);
}

static void ipinApply(HttpConn *conn) {
    char str[32] = {0};
    char optstr[256] = {0};
    if(isAuthed()){
        return;
    }
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(str, 8);
        render(str);
        return;
    }
    if((strcmp(role, "root") !=0) && (strcmp(role, "admin") !=0)){
        rendersts(str, 5);//无权限
        render(str);
        return;
    }
    MprJson *jsonparam = httpGetParams(conn);
    int ch = atoi(mprGetJson(jsonparam, "chk"));
    int port = atoi(mprGetJson(jsonparam, "port"));
    int netMode = atoi(mprGetJson(jsonparam, "netMode"));
    int streamType = atoi(mprGetJson(jsonparam, "streamType"));
    cchar *strip = mprGetJson(jsonparam, "ipstr");
    cchar *desip = mprGetJson(jsonparam, "desip");
    cchar *macstr = mprGetJson(jsonparam, "macstr");
    clsGlobal._inDb->valid = ch;
    clsGlobal._inDb->port = port;
    clsGlobal._inDb->unicastMulticast = netMode;
    clsGlobal._inDb->inStreamType = streamType;
    char *newip = strtok(strip, ".");
    int i=0, tmp = 0;
    while(newip)
    {
        clsGlobal._inDb->srcIp[i] = (unsigned char)atoi(newip);
        newip = strtok(NULL, ".");
        i++;
    }
    i=0;
    char *dip = strtok(desip, ":");
    while(dip)
    {
        clsGlobal._inDb->ip[i] = (unsigned char)atoi(dip);
        dip = strtok(NULL, ".");
        i++;
    }
    i=0;
    char *tmpmac = strtok(macstr, ":");
    while(tmpmac)
    {
        sscanf(tmpmac,"%x", &tmp);
        clsGlobal._inDb->mac[i] = (unsigned char)tmp;
        tmpmac = strtok(NULL, ":");
        i++;
    }

    in_ParamsWriteAll(conn->rx->parsedUri->host);

    //add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }
    time_t curTime;
    time(&curTime);
	cchar *lan = getSessionVar("language");
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '用户修改IP输入信息.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'User changed IP input info.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}
    
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
    rendersts(str, 1);
    render(str);
}

static void uploads(HttpConn *conn) {
	printf("================>>>uploads start!!\n");
	int i = 0, j = 0, tmp = 0;
	char str[32] = {0};
	char optstr[256] = {0};
	if(isAuthed()){
        redirect("/login.esp");
    }
    cchar *role = getSessionVar("role");
	cchar *lan = getSessionVar("language");		
    if(role == NULL){
        redirect("/login.esp");
    }
    if((strcmp(role, "root") !=0) && (strcmp(role, "admin") !=0)){
        rendersts(str, 5);//无权限
        render(str);
        return;
    }
	MprJson *jsonparam = httpGetParams(conn);	
	cchar *upstring = mprGetJson(jsonparam, "updatas");	
	MprJson *updatas = mprParseJson(upstring);
	//赋值
	clsGlobal.ipGwDb->devNetFun = atoi(mprGetJson(updatas, "devNetFun"));
	if(clsGlobal.ipGwDb->devNetFun){
		//in-mode
		MprJson *_inDb = mprGetJsonObj(updatas, "_inDb");
		if(_inDb == NULL){
			feedback("error", "Communication Error!");			           
			redirect("/import.esp");
		}
		clsGlobal._inDb->valid = atoi(mprGetJson(_inDb, "valid"));
		clsGlobal._inDb->port = atoi(mprGetJson(_inDb, "port"));
		clsGlobal._inDb->unicastMulticast = atoi(mprGetJson(_inDb, "unicastMulticast"));
		clsGlobal._inDb->inStreamType = atoi(mprGetJson(_inDb, "inStreamType"));
		//clsGlobal._inDb->outStreamBitrate = atoi(mprGetJson(_inDb, "outStreamBitrate"));
		cchar *ipstr = mprGetJson(_inDb, "ip");
		cchar *macstr = mprGetJson(_inDb, "mac");
		cchar *srcipstr = mprGetJson(_inDb, "srcip");
		char *newip = strtok(ipstr, ".");
		while(newip)
		{
			clsGlobal._inDb->ip[i] = (unsigned char)atoi(newip);
			newip = strtok(NULL, ".");
			i++;
		}
		i=0;
		char *tmpmac = strtok(macstr, ":");
		while(tmpmac)
		{
			sscanf(tmpmac,"%x", &tmp);
			clsGlobal._inDb->mac[i] = (unsigned char)tmp;
			tmpmac = strtok(NULL, ":");
			i++;
		}
		i=0;
		newip = strtok(srcipstr, ".");
		while(newip)
		{
			clsGlobal._inDb->srcIp[i] = (unsigned char)atoi(newip);
			newip = strtok(NULL, ".");
			i++;
		}
		in_ParamsWriteAll(conn->rx->parsedUri->host);
	}else{
		MprJson *destdb = NULL;
		MprJson *prgjson = NULL;
		UcIpDestDbSt3_st *eachPrg = NULL;
		UcIpDestPrgMuxInfoSt_st *muxPrg = NULL;
		if(list_len(clsGlobal.ucIpDestDb) == 0){
			redirect("/import.esp");
			return;
		}
		cchar *ipstr = mprGetJson(updatas, "srcip");
		cchar *macstr = mprGetJson(updatas, "srcmac");
		char *newip = strtok(ipstr, ".");
		while(newip)
		{
			clsGlobal._ucDb->ip[i] = (unsigned char)atoi(newip);
			newip = strtok(NULL, ".");
			i++;
		}
		i=0;
		char *tmpmac = strtok(macstr, ":");
		while(tmpmac)
		{
			sscanf(tmpmac,"%x", &tmp);
			clsGlobal._ucDb->mac[i] = (unsigned char)tmp;
			tmpmac = strtok(NULL, ":");
			i++;
		}
		clsGlobal._ucDb->port = atoi(mprGetJson(updatas, "srcport"));
		clsGlobal.ipGwDb->ttl = atoi(mprGetJson(updatas, "ttl"));
		clsGlobal.ipGwDb->dvbIptvMode = atoi(mprGetJson(updatas, "dvbIptvMode"));
		clsGlobal._ucDb->netInterfaceMode = atoi(mprGetJson(updatas, "netInterfaceMode"));
		//write orgin info
		clsGlobal._moduleBaseCmd = 0xf1;
		clsGlobal._moduleId = 0x1;		
		int isGood = 1;
		isGood &= ParamWriteByBytesCmd(conn->rx->parsedUri->host, (unsigned char)1, clsGlobal._ucDb->ip, 4);
		isGood &= ParamWriteByBytesCmd(conn->rx->parsedUri->host, (unsigned char)2, clsGlobal._ucDb->mac, 6);
		isGood &= ParamWriteByIntCmd(conn->rx->parsedUri->host, (unsigned char)3, clsGlobal._ucDb->port, 2);
		if (isGood){
			isGood &= ParamWriteByIntCmd(conn->rx->parsedUri->host, (unsigned char)0xf0, 0, 0);
		}else{
			if(!strcmp(lan, "zh-CN")){
				feedback("error", "通信错误!");
			}else{
				feedback("error", "Communication Error!");
			}            
			redirect("/import.esp");
		}
		int destdbcnt = atoi(mprGetJson(updatas, "destdbcnt"));	
		MprJson *ucIpDestDb = mprGetJsonObj(updatas, "ucIpDestDb");
		//printf("==============updatas=======>>%s\n", mprJsonToString (updatas, MPR_JSON_QUOTES));
		if(ucIpDestDb == NULL){
			feedback("error", "Communication Error!");			           
			redirect("/import.esp");
		}
		char tmpstr[32] = {0};
		for(i=0;i<destdbcnt;i++){
			memset(str, 0, sizeof(str));
			sprintf(str, "destdb%d", i);
			destdb = mprGetJsonObj(ucIpDestDb, str);			
            list_get(clsGlobal.ucIpDestDb, i, &eachPrg);
			eachPrg->outMode = atoi(mprGetJson(destdb, "outMode"));
			eachPrg->port = atoi(mprGetJson(destdb, "port"));
			cchar *prgipstr = mprGetJson(destdb, "ipStr");			
			cchar *prgmacstr = mprGetJson(destdb, "macStr");
			memset(tmpstr, 0, sizeof(tmpstr));
			memcpy(tmpstr, prgipstr, strlen(prgipstr));
			newip = strtok(tmpstr, ".");
			j = 0;
			while(newip)
			{
				eachPrg->ip[j] = (unsigned char)atoi(newip);
				newip = strtok(NULL, ".");
				j++;
			}
			j = 0;
			memset(tmpstr, 0, sizeof(tmpstr));
			memcpy(tmpstr, prgmacstr, strlen(prgmacstr));
			tmpmac = strtok(tmpstr, ":");
			while(tmpmac)
			{
				sscanf(tmpmac,"%x", &tmp);
				eachPrg->mac[j] = (unsigned char)tmp;
				tmpmac = strtok(NULL, ":");
				j++;
			}
			
			if(atoi(mprGetJson(destdb, "prgcnt")) == 1){
				prgjson = mprGetJsonObj(destdb, "prgjson");
				if(eachPrg->prgList == NULL){
					eachPrg->prgList = malloc(sizeof(list_t));
					list_init(eachPrg->prgList);
					muxPrg = malloc(sizeof(UcIpDestPrgMuxInfoSt_st));
					muxPrg->inChn = atoi(mprGetJson(prgjson, "inChn"));
					muxPrg->prgId = atoi(mprGetJson(prgjson, "prgId"));
					muxPrg->pmtPID = atoi(mprGetJson(prgjson, "pmtPID"));
					muxPrg->avPidListLen = atoi(mprGetJson(prgjson, "avPidListLen"));
					if(muxPrg->avPidListLen > 0){
						muxPrg->avPidList = malloc(sizeof(muxPrg->avPidListLen));
						memcpy(muxPrg->avPidList, mprGetJson(prgjson, "avPidList"), muxPrg->avPidListLen);
					}
					list_append(eachPrg->prgList, muxPrg);
				}else{
					list_get(eachPrg->prgList, 0, &muxPrg);
					muxPrg->inChn = atoi(mprGetJson(prgjson, "inChn"));
					muxPrg->prgId = atoi(mprGetJson(prgjson, "prgId"));
					muxPrg->pmtPID = atoi(mprGetJson(prgjson, "pmtPID"));
					if(muxPrg->avPidListLen > 0){
						memcpy(muxPrg->avPidList, mprGetJson(prgjson, "avPidList"), muxPrg->avPidListLen);
					}
				}			
			}else{
				if(eachPrg->prgList != NULL){
					freeUcIpDestPrg(eachPrg->prgList);
					free(eachPrg->prgList);
					eachPrg->prgList = NULL;
				}
			}
		}
		EnableValidOutChn();
		clsGlobal._moduleBaseCmd = 0xf2;
		//write to device
		if(clsGlobal.ipGwDb->devNetFun == 0){
			if (!IpWrite(conn->rx->parsedUri->host)){
				if(!strcmp(lan, "zh-CN")){
					feedback("error", "通信错误!");
				}else{
					feedback("error", "Communication Error!");
				}            
				redirect("/import.esp");
			}
			if (!IptvWrite(conn->rx->parsedUri->host)){
				if(!strcmp(lan, "zh-CN")){
					feedback("error", "通信错误!");
				}else{
					feedback("error", "Communication Error!");
				}            
				redirect("/import.esp");
			}
		}else{
			if (!ParamsWriteAll(conn->rx->parsedUri->host, 0xff)){
				if(!strcmp(lan, "zh-CN")){
					feedback("error", "通信错误!");
				}else{
					feedback("error", "Communication Error!");
				}            
				redirect("/import.esp");
			}
		}
		
	}	
	//add optlog
    Edi *db = ediOpen("db/ipgw.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }
    time_t curTime;
    time(&curTime);
	if(!strcmp(lan, "zh-CN")){
		sprintf(optstr, "{'user': '%s', 'desc': '导入备份数据.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}else{
		sprintf(optstr, "{'user': '%s', 'desc': 'Import backup datas.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
	}
    
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
	redirect("/index.esp");
}

static void downloads(HttpConn *conn) {
	char str[32] = {0};
	char outprg[20480] = {0};
	if(isAuthed()){
        return;
    }
    cchar *role = getSessionVar("role");
    if(role == NULL){
        rendersts(str, 8);
        render(str);
        return;
    }
    if((strcmp(role, "root") !=0) && (strcmp(role, "admin") !=0)){
        rendersts(str, 5);//无权限
        render(str);
        return;
    }
	getBackupJson(conn->rx->parsedUri->host, outprg);
	render(outprg);
}

static void gettoken(HttpConn *conn) {
	char str[132] = {0};
	printf("================>>>tokenkey--%s\n", httpGetSecurityToken(conn, 0));
	memcpy(str, httpGetSecurityToken(conn, 0), 132);
	render(str);
}


static void common(HttpConn *conn) {

}

static void espinit() {
	ChannelProgramSt *pst = NULL;
	//全局变量初始化
	clsProgram._outChannelCntMax = 1;
	clsProgram._intChannelCntMax = 8;
	clsProgram._pmtMaxCnt = 29;
	clsProgram.prgNum_min = 1;
	clsProgram.prgPid_min = 0x100;
	clsProgram.prgPid_max = 0xfff;
	//给全局变量申请内存
    pst = malloc(sizeof(ChannelProgramSt));
    memset(pst, 0, sizeof(ChannelProgramSt));
    pst->channelId = 1;
    list_append(&(clsProgram.inPrgList), pst);

    Init();
	printf("======>>>>esp init!!!!!!!\n");
}

/*
    Dynamic module initialization
 */
ESP_EXPORT int esp_controller_ipgw_programs(HttpRoute *route, MprModule *module) {
    espDefineBase(route, common);
    espinit();
    espDefineAction(route, "programs-cmd-getdevNetFun", getdevNetFun);
    espDefineAction(route, "programs-cmd-getDevinfo", getDevinfo);
    espDefineAction(route, "programs-cmd-ipRead", ipRead);
    espDefineAction(route, "programs-cmd-getPrgs", getPrgs);
    espDefineAction(route, "programs-cmd-readinputsts", readinputsts);
    espDefineAction(route, "programs-cmd-ParamsWriteAll", paramswriteAll);
    espDefineAction(route, "programs-cmd-iptvRead", iptvRead);
    espDefineAction(route, "programs-cmd-outprgList", outprgList);
    espDefineAction(route, "programs-cmd-setIpTvmode", setIpTvmode);
    espDefineAction(route, "programs-cmd-tbl_selctdprg", tbl_selctdprg);
    espDefineAction(route, "programs-cmd-tree_selctdprg", tree_selctdprg);
    espDefineAction(route, "programs-cmd-outchnprg_output", outchnprg_output);
    espDefineAction(route, "programs-cmd-outChnConfig", outChnConfig);
    espDefineAction(route, "programs-cmd-prgMuxSptsMap", prgMuxSptsMap);
    espDefineAction(route, "programs-cmd-clearprgMux", clearprgMux);
    espDefineAction(route, "programs-cmd-muxprgwrite", muxprgwrite);
    espDefineAction(route, "programs-cmd-refreshIpMode", refreshIpMode);
    espDefineAction(route, "programs-cmd-netapply", netapply);

    espDefineAction(route, "programs-cmd-readipIN", readipIN);
    espDefineAction(route, "programs-cmd-readipinsts", readipinsts);
    espDefineAction(route, "programs-cmd-ipinApply", ipinApply);
	espDefineAction(route, "programs-cmd-uploads", uploads);
	espDefineAction(route, "programs-cmd-downloads", downloads);
	espDefineAction(route, "programs-cmd-gettoken", gettoken);


#if SAMPLE_VALIDATIONS
    Edi *edi = espGetRouteDatabase(route);
    ediAddValidation(edi, "present", "programs", "title", 0);
    ediAddValidation(edi, "unique", "programs", "title", 0);
    ediAddValidation(edi, "banned", "programs", "body", "(swear|curse)");
    ediAddValidation(edi, "format", "programs", "phone", "/^\\(?([0-9]{3})\\)?[-. ]?([0-9]{3})[-. ]?([0-9]{4})$/");
#endif
    return 0;
}
