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
#include "clsMuxprgInfoGet.h"

char *tmpip = "192.168.1.49";

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

static void getDevinfo(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char pProg[256] = {0};
    int r = 0;
    EdiField *src;
    printf("---------------getdevinfo===\n");
    getbaseJson(tmpip, pProg);
    printf("---------------getdevinfo end===\n");
    render(pProg);
    //delete optlog 7days ago
    Edi *db = ediOpen("db/muxnms.mdb", "mdb", EDI_AUTO_SAVE);
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

static void search(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char str[32] = {0};
    char optstr[256] = {0};

    RefreshIpInOutMode(tmpip);
    if(clsGlobal.ipGwDb->devNetFun == 0){
        if(!Search(tmpip, 1)){
            //error
            rendersts(str, 6);
            render(str);
            return;
        }
    }
    //add optlog
    Edi *db = ediOpen("db/muxnms.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }

    time_t curTime;
    time(&curTime);
    sprintf(optstr, "{'user': '%s', 'desc': '用户搜索节目源.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
    MprJson  *row = mprParseJson(optstr);
    if(ediSetFields(optlog, row) == 0){
       printf("================>>>ediSetFields Failed!!\n");
    }
    ediUpdateRec(db, optlog);
    rendersts(str, 1);
    render(str);
}

static void getPrgs(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char outprg[102400] = {0};
    getPrgsJson(tmpip, outprg);
    render(outprg);
}

static void ipRead(HttpConn *conn) {
    if(isAuthed()){
        return;
    }
    char outprg[128] = {0};
    if(!getIpReadJson(tmpip, outprg)){
        rendersts(outprg, 8);
        render(outprg);
        return;
    }
    render(outprg);
}

static void readinputsts(HttpConn *conn) {
    char str[64] = {0};
    if(!getInputStsJson(tmpip, str)){
        rendersts(str, 8);
        render(str);
        return;
    }
    render(str);
}

static void ParamsWriteAll(HttpConn *conn) {
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
    isGood &= ParamWriteByBytesCmd(tmpip, (unsigned char)1, clsGlobal._ucDb->ip, 4);
    isGood &= ParamWriteByBytesCmd(tmpip, (unsigned char)2, clsGlobal._ucDb->mac, 6);
    isGood &= ParamWriteByIntCmd(tmpip, (unsigned char)3, clsGlobal._ucDb->port, 2);

    if (isGood){
        isGood &= ParamWriteByIntCmd(tmpip, (unsigned char)0xf0, 0, 0);
    }
    //add optlog
    Edi *db = ediOpen("db/muxnms.mdb", "mdb", EDI_AUTO_SAVE);
    EdiRec *optlog = ediCreateRec(db, "optlog");
    if(optlog == NULL){
       printf("================>>>optlog is NULL!!\n");
    }

    time_t curTime;
    time(&curTime);
    sprintf(optstr, "{'user': '%s', 'desc': '用户下发发送源配置.', 'level': '1', 'logtime':'%d'}", getSessionVar("userName"), curTime);
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
    if(!IptvRead(tmpip)){
        rendersts(str, 6);
        render(str);
        return;
    }
    rendersts(str, 1);
    render(str);
}

static void outprgList(HttpConn *conn) {
    char outprg[10240] = {0};
    DeleteInvalidOutputChn();
    getPrgoutListJson(outprg);
    render(outprg);
}
//
//static void redirectPost() {
//    redirect(sjoin(getConn()->rx->uri, "/", NULL));
//}



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
    espDefineAction(route, "programs-cmd-getDevinfo", getDevinfo);
    espDefineAction(route, "programs-cmd-ipRead", ipRead);
    espDefineAction(route, "programs-cmd-getPrgs", getPrgs);
    espDefineAction(route, "programs-cmd-search", search);
    espDefineAction(route, "programs-cmd-readinputsts", readinputsts);
    espDefineAction(route, "programs-cmd-ParamsWriteAll", ParamsWriteAll);
    espDefineAction(route, "programs-cmd-iptvRead", iptvRead);
    espDefineAction(route, "programs-cmd-outprgList", outprgList);

#if SAMPLE_VALIDATIONS
    Edi *edi = espGetRouteDatabase(route);
    ediAddValidation(edi, "present", "programs", "title", 0);
    ediAddValidation(edi, "unique", "programs", "title", 0);
    ediAddValidation(edi, "banned", "programs", "body", "(swear|curse)");
    ediAddValidation(edi, "format", "programs", "phone", "/^\\(?([0-9]{3})\\)?[-. ]?([0-9]{3})[-. ]?([0-9]{4})$/");
#endif
    return 0;
}
