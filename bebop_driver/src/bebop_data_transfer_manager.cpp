#include <iostream>
#include <fstream>

#include "bebop_driver/bebop_data_transfer_manager.h"

ARSAL_Thread_t threadMediasDownloader;    // the thread that will download medias

BebopDataTransferManager::BebopDataTransferManager()
	: manager(NULL),
		mediaAvailableFlag(false),
		mediaDownloadFinishedFlag(false),
		numberOfCurrentlyAvailableMediaToDownload(0),
		numberOfCurrentlyDownloadedNotDeletedMedia(0),
		numberOfCurrentlyDeletedMedia(0),
		downloadingMediasFlag(false)
{
		threadMediasDownloaderPtr = &threadMediasDownloader;
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

    if (*threadMediasDownloaderPtr != NULL)
    {
        ARDATATRANSFER_MediasDownloader_CancelQueueThread(manager);

        ARSAL_Thread_Join(*threadMediasDownloaderPtr, NULL);
        ARSAL_Thread_Destroy(threadMediasDownloaderPtr);
        *threadMediasDownloaderPtr = NULL;
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
            numberOfCurrentlyAvailableMediaToDownload = mediaListCount;

            std::lock_guard<std::mutex> guard(localMediasMutex);
            medias.clear();

            for (int i = 0 ; i < mediaListCount && result == ARDATATRANSFER_OK; i++)
            {
                ARDATATRANSFER_Media_t * mediaObject = ARDATATRANSFER_MediasDownloader_GetAvailableMediaAtIndex(manager, i, &result);
                //printf("Media %i: %s", i, mediaObject->name);
                // Do what you want with this mediaObject
                medias.push_back(mediaObject);
            }
            mediaAvailableFlag = true;
            mediaDownloadFinishedFlag = false;
        }
    }
}

void BebopDataTransferManager::downloadMedias()
//void downloadMedias()
{
    eARDATATRANSFER_ERROR result = ARDATATRANSFER_OK;
    for (int i = 0 ; i < numberOfCurrentlyAvailableMediaToDownload && result == ARDATATRANSFER_OK; i++)
    {
        std::lock_guard<std::mutex> guard(localMediasMutex);
        ARDATATRANSFER_Media_t *media = medias[i];
        result = ARDATATRANSFER_MediasDownloader_AddMediaToQueue(manager, media, BebopDataTransferManager::medias_downloader_progress_callback, (void*)this, BebopDataTransferManager::medias_downloader_completion_callback, (void*)this);
    }

    if (result == ARDATATRANSFER_OK)
    {
        if (*threadMediasDownloaderPtr == NULL)
        {
            // if not already started, start download thread in background
            ARSAL_Thread_Create(threadMediasDownloaderPtr, ARDATATRANSFER_MediasDownloader_QueueThreadRun, manager);
        }
    }
}

void BebopDataTransferManager::medias_downloader_progress_callback(void* arg, ARDATATRANSFER_Media_t *media, float percent)
//void medias_downloader_progress_callback(void* arg, ARDATATRANSFER_Media_t *media, float percent)
{
    // the media is downloading
    //std::cout << "Media downloaded up to: " << percent << std::endl;
}

void BebopDataTransferManager::medias_downloader_completion_callback(void* arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error)
//void medias_downloader_completion_callback(void* arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error)
{
    // the media is downloaded
    //std::cout << "Media is downloaded!" << std::endl;
    static_cast<BebopDataTransferManager*>(arg)->medias_downloader_completion();
}

void BebopDataTransferManager::medias_downloader_completion()
{
		{
				std::lock_guard<std::mutex> guard(mediaDeletedFinishedFlagMutex);
				mediaDeletedFinishedFlag = false;
		}
		{
				std::lock_guard<std::mutex> guard(numberOfCurrentlyDownloadedNotDeletedMediaMutex);
				numberOfCurrentlyDownloadedNotDeletedMedia++;
		}

		if(numberOfCurrentlyDownloadedNotDeletedMedia == numberOfCurrentlyAvailableMediaToDownload)
		{
				numberOfCurrentlyAvailableMediaToDownload = 0;

				{
						std::lock_guard<std::mutex> guard(mediaAvailableFlagMutex);
						mediaAvailableFlag = false;
				}
				{
						std::lock_guard<std::mutex> guard(mediaDownloadFinishedFlagMutex);
						mediaDownloadFinishedFlag = true;
				}
		}
}

bool BebopDataTransferManager::mediaAvailable()
{
    return(mediaAvailableFlag);
}

bool BebopDataTransferManager::mediaDownloadFinished()
{
    return(mediaDownloadFinishedFlag);
}

bool BebopDataTransferManager::mediaDeletedFinished()
{
    return(mediaDeletedFinishedFlag);
}

void BebopDataTransferManager::deleteAllMedia()
{
    eARDATATRANSFER_ERROR result = ARDATATRANSFER_OK;
    //for (int i = 0 ; i < numberOfCurrentlyDownloadedNotDeletedMedia && result == ARDATATRANSFER_OK; i++)
    for (int i = 0 ; i < numberOfCurrentlyDownloadedNotDeletedMedia; i++)
    {
        std::lock_guard<std::mutex> guard(localMediasMutex);
        ARDATATRANSFER_Media_t *media = medias[i];
        result = ARDATATRANSFER_MediasDownloader_DeleteMedia(manager, media, BebopDataTransferManager::medias_delete_completion_callback, (void*)this);
    }
}


void BebopDataTransferManager::medias_delete_completion_callback(void* arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error)
{
		static_cast<BebopDataTransferManager*>(arg)->medias_delete_completion();
}

void BebopDataTransferManager::medias_delete_completion()
{
		std::lock_guard<std::mutex> guard(numberOfCurrentlyDeletedMediaMutex);
		numberOfCurrentlyDeletedMedia++;

		if(numberOfCurrentlyDeletedMedia == numberOfCurrentlyDownloadedNotDeletedMedia)
		{
				std::lock_guard<std::mutex> guard(mediaDeletedFinishedFlagMutex);
				mediaDeletedFinishedFlag = true;

				numberOfCurrentlyDownloadedNotDeletedMedia = 0;
				numberOfCurrentlyDeletedMedia = 0;
		}
}


int BebopDataTransferManager::numberOfDownloadedFiles()
{
		return(numberOfCurrentlyAvailableMediaToDownload);
}

void BebopDataTransferManager::removePictures()
{
		deleteAllMedia();

		/*
		std::cout << "Remove pictures!" << std::endl;

		std::ofstream outfile("tmp/test.txt");
		outfile << "Test" << std::endl;
		outfile.close();
		*/

		//system("exec rm -r tmp/*");
}
