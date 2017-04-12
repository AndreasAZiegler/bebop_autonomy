#include <iostream>
#include <fstream>

#include "bebop_driver/bebop_data_transfer_manager.h"

BebopDataTransferManager::BebopDataTransferManager()
	: manager(NULL),
		mediaAvailableFlag(false),
		mediaDownloadFinishedFlag(true),
		count(0)
{
		createDataTransferManager();
}

BebopDataTransferManager::~BebopDataTransferManager()
{
   if (threadRetreiveAllMedias != NULL)
    {
        ARDATATRANSFER_MediasDownloader_CancelGetAvailableMedias(manager);

        ARSAL_Thread_Join(threadRetreiveAllMedias, NULL);
        ARSAL_Thread_Destroy(&threadRetreiveAllMedias);
        threadRetreiveAllMedias = NULL;
    }

    if (threadGetThumbnails != NULL)
    {
        ARDATATRANSFER_MediasDownloader_CancelGetAvailableMedias(manager);

        ARSAL_Thread_Join(threadGetThumbnails, NULL);
        ARSAL_Thread_Destroy(&threadGetThumbnails);
        threadGetThumbnails = NULL;
    }

    if (threadMediasDownloader != NULL)
    {
        ARDATATRANSFER_MediasDownloader_CancelQueueThread(manager);

        ARSAL_Thread_Join(threadMediasDownloader, NULL);
        ARSAL_Thread_Destroy(&threadMediasDownloader);
        threadMediasDownloader = NULL;
    }

    ARDATATRANSFER_MediasDownloader_Delete(manager);

    ARUTILS_Manager_CloseWifiFtp(ftpListManager);
    ARUTILS_Manager_CloseWifiFtp(ftpQueueManager);

    ARUTILS_Manager_Delete(&ftpListManager);
    ARUTILS_Manager_Delete(&ftpQueueManager);
    ARDATATRANSFER_Manager_Delete(&manager);
}

void BebopDataTransferManager::createDataTransferManager()
{
    const char *productIP = "192.168.42.1";  // TODO: get this address from libARController

    eARDATATRANSFER_ERROR result = ARDATATRANSFER_OK;
    manager = ARDATATRANSFER_Manager_New(&result);

    if (result == ARDATATRANSFER_OK)
    {
        eARUTILS_ERROR ftpError = ARUTILS_OK;
        ftpListManager = ARUTILS_Manager_New(&ftpError);
        if(ftpError == ARUTILS_OK)
        {
            ftpQueueManager = ARUTILS_Manager_New(&ftpError);
        }

        if(ftpError == ARUTILS_OK)
        {
            ftpError = ARUTILS_Manager_InitWifiFtp(ftpListManager, productIP, DEVICE_PORT, ARUTILS_FTP_ANONYMOUS, "");
        }

        if(ftpError == ARUTILS_OK)
        {
            ftpError = ARUTILS_Manager_InitWifiFtp(ftpQueueManager, productIP, DEVICE_PORT, ARUTILS_FTP_ANONYMOUS, "");
        }

        if(ftpError != ARUTILS_OK)
        {
            result = ARDATATRANSFER_ERROR_FTP;
        }
    }
    // NO ELSE

    if (result == ARDATATRANSFER_OK)
    {
        const char *path = "tmp"; // Change according to your needs, or put as an argument

        result = ARDATATRANSFER_MediasDownloader_New(manager, ftpListManager, ftpQueueManager, MEDIA_FOLDER, path);
    }
}

void BebopDataTransferManager::startMediaListThread()
{
    // first retrieve Medias without their thumbnails
    ARSAL_Thread_Create(&threadRetreiveAllMedias, BebopDataTransferManager::ARMediaStorage_retreiveAllMediasAsync, (void*)this);
}

void* BebopDataTransferManager::ARMediaStorage_retreiveAllMediasAsync(void* arg)
{
    static_cast<BebopDataTransferManager*>(arg)->getAllMediaAsync();
    return NULL;
}

void BebopDataTransferManager::getAllMediaAsync()
{
    eARDATATRANSFER_ERROR result = ARDATATRANSFER_OK;
    int mediaListCount = 0;

    if (result == ARDATATRANSFER_OK)
    {
        mediaListCount = ARDATATRANSFER_MediasDownloader_GetAvailableMediasSync(manager, 0, &result);
        if (result == ARDATATRANSFER_OK && mediaListCount > 0)
        {
            count = mediaListCount;

            std::lock_guard<std::mutex> guard(accessMediasMutex);
            medias.clear();

            for (int i = 0 ; i < mediaListCount && result == ARDATATRANSFER_OK; i++)
            {
                ARDATATRANSFER_Media_t * mediaObject = ARDATATRANSFER_MediasDownloader_GetAvailableMediaAtIndex(manager, i, &result);
                //printf("Media %i: %s", i, mediaObject->name);
                // Do what you want with this mediaObject
                medias.push_back(mediaObject);
            }
            mediaAvailableFlag = true;
        }
    }
}

void BebopDataTransferManager::downloadMedias()
{
    eARDATATRANSFER_ERROR result = ARDATATRANSFER_OK;
    for (int i = 0 ; i < count && result == ARDATATRANSFER_OK; i++)
    {
        std::lock_guard<std::mutex> guard(accessMediasMutex);
        ARDATATRANSFER_Media_t *media = medias[i];
        result = ARDATATRANSFER_MediasDownloader_AddMediaToQueue(manager, media, BebopDataTransferManager::medias_downloader_progress_callback, NULL, BebopDataTransferManager::medias_downloader_completion_callback, (void*)this);
    }

    if (result == ARDATATRANSFER_OK)
    {
        if (threadMediasDownloader == NULL)
        {
            // if not already started, start download thread in background
            ARSAL_Thread_Create(&threadMediasDownloader, ARDATATRANSFER_MediasDownloader_QueueThreadRun, manager);
        }
    }
}

void BebopDataTransferManager::medias_downloader_progress_callback(void* arg, ARDATATRANSFER_Media_t *media, float percent)
{
    // the media is downloading
}

void BebopDataTransferManager::medias_downloader_completion_callback(void* arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error)
{
    // the media is downloaded
    static_cast<BebopDataTransferManager*>(arg)->medias_downloader_completion();
}

void BebopDataTransferManager::medias_downloader_completion()
{
    mediaAvailableFlag = false;
    mediaDownloadFinishedFlag = true;
}

bool BebopDataTransferManager::mediaAvailable()
{
    return(mediaAvailableFlag);
}

bool BebopDataTransferManager::mediaDownloadFinished()
{
    return(mediaDownloadFinishedFlag);
}

int BebopDataTransferManager::numberOfDownloadedFiles()
{
  return(count);
}


void BebopDataTransferManager::removePictures()
{
  std::lock_guard<std::mutex> guard(removePicturesMutex);

  std::cout << "Remove pictures!" << std::endl;

  std::ofstream outfile("tmp/test.txt");
  outfile << "Test" << std::endl;
  outfile.close();

  system("exec rm -r tmp/*");
}
