#include "listeners.h"
#include "configurationmanager.h"
#include "megacmdutils.h"

void MegaCmdGlobalListener::onChatsUpdate(mega::MegaApi*, mega::MegaTextChatList*){}

void MegaCmdGlobalListener::onUsersUpdate(MegaApi *api, MegaUserList *users)
{
    if (users)
    {
        if (users->size()==1)
        {
            LOG_info <<" 1 user received or updated" ;
        }
        else
        {
            LOG_info << users->size() << " users received or updated";
        }
    }
    else //initial update or too many changes
    {
        MegaUserList *users = api->getContacts();

        if (users)
        {
            if (users->size()==1)
            {
                LOG_info <<" 1 user received or updated" ;
            }
            else
            {
                LOG_info << users->size() << " users received or updated";
            }
            delete users;
        }
    }
}

MegaCmdGlobalListener::MegaCmdGlobalListener(MegaCMDLogger *logger)
{
    this->loggerCMD = logger;
}

void MegaCmdGlobalListener::onNodesUpdate(MegaApi *api, MegaNodeList *nodes)
{
    int nfolders = 0;
    int nfiles = 0;
    int rfolders = 0;
    int rfiles = 0;
    if (nodes)
    {
        for (int i=0;i<nodes->size();i++)
        {
            MegaNode *n = nodes->get(i);
            if (n->getType() == MegaNode::TYPE_FOLDER)
            {
                if (n->isRemoved()) rfolders++;
                else nfolders++;
            }
            else if (n->getType() == MegaNode::TYPE_FILE)
            {
                if (n->isRemoved()) rfiles++;
                else nfiles++;
            }
        }
    }
    else //initial update or too many changes
    {
        if (loggerCMD->getMaxLogLevel() >= logInfo)
        {
            MegaNode * nodeRoot= api->getRootNode();
            int * nFolderFiles = getNumFolderFiles(nodeRoot,api);
            nfolders+=nFolderFiles[0];
            nfiles+=nFolderFiles[1];
            delete []nFolderFiles;
            delete nodeRoot;

            MegaNode * inboxNode= api->getInboxNode();
            nFolderFiles = getNumFolderFiles(inboxNode,api);
            nfolders+=nFolderFiles[0];
            nfiles+=nFolderFiles[1];
            delete []nFolderFiles;
            delete inboxNode;

            MegaNode * rubbishNode= api->getRubbishNode();
            nFolderFiles = getNumFolderFiles(rubbishNode,api);
            nfolders+=nFolderFiles[0];
            nfiles+=nFolderFiles[1];
            delete []nFolderFiles;
            delete rubbishNode;

            MegaNodeList *inshares = api->getInShares();
            if (inshares)
            for (int i=0; i<inshares->size();i++)
            {
                nfolders++; //add the share itself
                nFolderFiles = getNumFolderFiles(inshares->get(i),api);
                nfolders+=nFolderFiles[0];
                nfiles+=nFolderFiles[1];
                delete []nFolderFiles;
            }
            delete inshares;
        }

        if (nfolders) { LOG_info << nfolders << " folders " << "added or updated "; }
        if (nfiles) { LOG_info << nfiles << " files " << "added or updated "; }
        if (rfolders) { LOG_info << rfolders << " folders " << "removed"; }
        if (rfiles) { LOG_info << rfiles << " files " << "removed"; }
    }
}



////////////////////////////////////////
///      MegaCmdListener methods     ///
////////////////////////////////////////

void MegaCmdListener::onRequestStart(MegaApi* api, MegaRequest *request)
{
    if (!request)
    {
        LOG_err << " onRequestStart for undefined request ";
        return;
    }

    LOG_verbose << "onRequestStart request->getType(): " << request->getType();
}

void MegaCmdListener::doOnRequestFinish(MegaApi* api, MegaRequest *request, MegaError* e)
{
    if (!request)
    {
        LOG_err << " onRequestFinish for undefined request ";
        return;
    }

    LOG_verbose << "onRequestFinish request->getType(): " << request->getType();

    switch(request->getType())
    {
        case MegaRequest::TYPE_FETCH_NODES:
        {
            map<string,sync_struct *>::iterator itr;
            int i =0;
            for(itr = ConfigurationManager::configuredSyncs.begin(); itr != ConfigurationManager::configuredSyncs.end(); ++itr,i++)
            {
                sync_struct *thesync = ((sync_struct *)(*itr).second);

                MegaCmdListener *megaCmdListener = new MegaCmdListener(api,NULL);
                MegaNode * node = api->getNodeByHandle(thesync->handle);

                api->resumeSync(thesync->localpath.c_str(), node, thesync->fingerprint,megaCmdListener);
                megaCmdListener->wait();
                if (megaCmdListener->getError() && megaCmdListener->getError()->getErrorCode() == MegaError::API_OK)
                {
                    thesync->fingerprint = megaCmdListener->getRequest()->getNumber();
                    thesync->active = true;

                    ConfigurationManager::loadedSyncs[thesync->localpath]=thesync;
                    char *nodepath = api->getNodePath(node);
                    LOG_info << "Loaded sync: " << thesync->localpath << " to " << nodepath;
                    delete []nodepath;
                }

                delete megaCmdListener;
                delete node;
            }
            break;
        }
        default:
//            LOG_debug << "onRequestFinish of unregistered type of request: " << request->getType();
// //            rl_message("");
// //            clear_display();
            break;
    }
}

void MegaCmdListener::onRequestUpdate(MegaApi* api, MegaRequest *request){
    if (!request)
    {
        LOG_err << " onRequestUpdate for undefined request ";
        return;
    }

    LOG_verbose << "onRequestUpdate request->getType(): " << request->getType();

    switch(request->getType())
    {
    case MegaRequest::TYPE_FETCH_NODES:
    {
#if defined(RL_ISSTATE) && defined(RL_STATE_INITIALIZED)
        int rows = 1,cols = 80;

        if (RL_ISSTATE(RL_STATE_INITIALIZED))
        {
            rl_resize_terminal();
            rl_get_screen_size (&rows,&cols);
        }
        char outputString[cols+1];
        for (int i=0;i<cols;i++) outputString[i]='.';
        outputString[cols]='\0';
        char * ptr = outputString;
        sprintf(ptr,"%s","Fetching nodes ||");
        ptr+=strlen("Fetching nodes ||");
        *ptr='.'; //replace \0 char


        float oldpercent = percentFetchnodes;
        percentFetchnodes =  request->getTransferredBytes()*1.0/request->getTotalBytes()*100.0;
        if (percentFetchnodes==oldpercent && oldpercent!=0) return;
        if (percentFetchnodes <0) percentFetchnodes = 0;

        char aux[40];
        if (request->getTotalBytes()<0) return; // after a 100% this happens
        if (request->getTransferredBytes()<0.001*request->getTotalBytes()) return; // after a 100% this happens
        sprintf(aux,"||(%lld/%lld MB: %.2f %%) ",request->getTransferredBytes()/1024/1024,request->getTotalBytes()/1024/1024,percentFetchnodes);
        sprintf(outputString+cols-strlen(aux),"%s",aux);
        for (int i=0; i<= (cols-strlen("Fetching nodes ||")-strlen(aux))*1.0*percentFetchnodes/100.0; i++) *ptr++='#';
        {
            if (RL_ISSTATE(RL_STATE_INITIALIZED))
            {
                rl_message("%s",outputString);
            }
            else
            {
                cout << outputString << endl; //too verbose
            }
        }
#endif
        break;

    }
    default:
        LOG_debug << "onRequestUpdate of unregistered type of request: " << request->getType();
        break;
    }
}

void MegaCmdListener::onRequestTemporaryError(MegaApi *api, MegaRequest *request, MegaError* e){

}


MegaCmdListener::~MegaCmdListener(){

}

MegaCmdListener::MegaCmdListener(MegaApi *megaApi, MegaRequestListener *listener)
{
    this->megaApi=megaApi;
    this->listener=listener;
}


////////////////////////////////////////
///      MegaCmdListener methods     ///
////////////////////////////////////////

void MegaCmdTransferListener::onTransferStart(MegaApi* api, MegaTransfer *Transfer)
{
    if (!Transfer)
    {
        LOG_err << " onTransferStart for undefined Transfer ";
        return;
    }

    LOG_verbose << "onTransferStart Transfer->getType(): " << Transfer->getType();
}

void MegaCmdTransferListener::doOnTransferFinish(MegaApi* api, MegaTransfer *transfer, MegaError* e)
{
    if (!transfer)
    {
        LOG_err << " onTransferFinish for undefined transfer ";
        return;
    }

    LOG_verbose << "onTransferFinish Transfer->getType(): " << transfer->getType();
}


void MegaCmdTransferListener::onTransferUpdate(MegaApi* api, MegaTransfer *Transfer)
{
    if (!Transfer)
    {
        LOG_err << " onTransferUpdate for undefined Transfer ";
        return;
    }

    LOG_verbose << "onTransferUpdate Transfer->getType(): " << Transfer->getType();


}

void MegaCmdTransferListener::onTransferTemporaryError(MegaApi *api, MegaTransfer *transfer, MegaError* e)
{

}


MegaCmdTransferListener::~MegaCmdTransferListener()
{

}

MegaCmdTransferListener::MegaCmdTransferListener(MegaApi *megaApi, MegaTransferListener *listener)
{
    this->megaApi=megaApi;
    this->listener=listener;
}

bool MegaCmdTransferListener::onTransferData(MegaApi *api, MegaTransfer *transfer, char *buffer, size_t size){
    return true;
}
