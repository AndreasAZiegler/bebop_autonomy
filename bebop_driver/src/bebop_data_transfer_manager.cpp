#include <iostream>
#include <fstream>

#include "bebop_driver/bebop_data_transfer_manager.h"

ARSAL_Thread_t threadMediasDownloader;    /**< the thread that will download medias */

/**
 * @brief Constructor
 * Creates a data transfer manager instance
 */
BebopDataTransferManager::BebopDataTransferManager()
	: manager(NULL),
		mediaAvailableFlag(false),
		mediaDownloadFinishedFlag(false),
		numberOfCurrentlyAvailableMediaToDownload(0),
		numberOfCurrentlyDownloadedNotDeletedMedia(0),
		numberOfCurrentlyDeletedMedia(0)
{
		threadMediasDownloaderPtr = &threadMediasDownloader;
		createDataTransferManager();
}

/**
 * @brief Destructor
 * Terminates media download, deletes the media download manager and cleans up
 */
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

/**
 * @brief Creates a data transfer manager
 */
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

/**
 * @brief starts media list thread
 */
void BebopDataTransferManager::startMediaListThread()
{
    // first retrieve Medias without their thumbnails
    ARSAL_Thread_Create(&threadRetreiveAllMedias, BebopDataTransferManager::ARMediaStorage_retreiveAllMediasAsync, (void*)this);
}

/**
 * @brief Starts asynchronous media synchronisation in the getAllMediaAsync method
 * @param arg Pointer to a BebopDataTransferManager object
 * @return NULL
 */
void* BebopDataTransferManager::ARMediaStorage_retreiveAllMediasAsync(void* arg)
{
    static_cast<BebopDataTransferManager*>(arg)->getAllMediaAsync();
    return NULL;
}

/**
 * @brief Checks for available media. If media is available, the number of medias is set and the media addresses are writen to the corresponding vector.
 */
void BebopDataTransferManager::getAllMediaAsync()
{
    if(getAllMediaAsyncMutex.try_lock())
    {
        eARDATATRANSFER_ERROR result = ARDATATRANSFER_OK;
        int mediaListCount = 0;

				if (result == ARDATATRANSFER_OK)
				{
						mediaListCount = ARDATATRANSFER_MediasDownloader_GetAvailableMediasSync(manager, 0, &result);
						if (result == ARDATATRANSFER_OK && mediaListCount > 0)
						{
								{
										std::lock_guard<std::mutex> guard(numberOfCurrentlyAvailableMediaToDownloadMutex);
										numberOfCurrentlyAvailableMediaToDownload = mediaListCount;
								}

								std::lock_guard<std::mutex> guard(localMediasMutex);
								medias.clear();

								for (int i = 0 ; i < mediaListCount && result == ARDATATRANSFER_OK; i++)
								{
										ARDATATRANSFER_Media_t * mediaObject = ARDATATRANSFER_MediasDownloader_GetAvailableMediaAtIndex(manager, i, &result);
										medias.push_back(mediaObject);
								}
								{
										std::lock_guard<std::mutex> guard(mediaAvailableFlagMutex);
										mediaAvailableFlag = true;
								}
								{
										std::lock_guard<std::mutex> guard(mediaDownloadFinishedFlagMutex);
										mediaDownloadFinishedFlag = false;
								}
						}
				}
				getAllMediaAsyncMutex.unlock();
		}
}

/**
 * @brief Adds all media to the media dowload queue and starts the download.
 */
void BebopDataTransferManager::downloadMedias()
{
    eARDATATRANSFER_ERROR result = ARDATATRANSFER_OK;
    std::lock_guard<std::mutex> guard(numberOfCurrentlyAvailableMediaToDownloadMutex);
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

/**
 * @brief Static call back method, executed when a download is in progress
 * @param arg Pointer to a BebopDataTransferManager object
 * @param media Pointer to the media object which is dowloading
 * @param percent Progress in percent
 */
void BebopDataTransferManager::medias_downloader_progress_callback(void* arg, ARDATATRANSFER_Media_t *media, float percent)
{
    // the media is downloading
    //std::cout << "Media downloaded up to: " << percent << std::endl;
}

/**
 * @brief Static call back method, executed when a download is finished, it calls the medias_downloader_completion method
 * @param arg Pointer to a BebopDataTransferManager object
 * @param media Pointer to the media object which is dowloading
 * @param error Error
 */
void BebopDataTransferManager::medias_downloader_completion_callback(void* arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error)
{
    // the media is downloaded
    //std::cout << "Media downloaded!" << std::endl;
    static_cast<BebopDataTransferManager*>(arg)->medias_downloader_completion();
}

/**
 * @brief Method to check if all media is downloaded
 */
void BebopDataTransferManager::medias_downloader_completion()
{
		std::lock_guard<std::mutex> guard(numberOfCurrentlyDownloadedNotDeletedMediaMutex);
		numberOfCurrentlyDownloadedNotDeletedMedia++;

		std::lock_guard<std::mutex> guard1(numberOfCurrentlyAvailableMediaToDownloadMutex);
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
		else
		{
				std::cout << "numberOfCurrentlyDownloadedNotDeletedMedia: " << numberOfCurrentlyDownloadedNotDeletedMedia << ", numberOfCurrentlyAvailableMediaToDownload: " << numberOfCurrentlyAvailableMediaToDownload << std::endl;
		}
		{
				std::lock_guard<std::mutex> guard(mediaDeletedFinishedFlagMutex);
				mediaDeletedFinishedFlag = false;
		}
}

/**
 * @brief Returns the state of the media available flag.
 * @return State of the media available flag
 */
bool BebopDataTransferManager::mediaAvailable()
{
    std::lock_guard<std::mutex> guard(mediaAvailableFlagMutex);
    return(mediaAvailableFlag);
}

/**
 * @brief Returns the state of the media download finished flag.
 * @return State of the media download finished flag
 */
bool BebopDataTransferManager::mediaDownloadFinished()
{
    std::lock_guard<std::mutex> guard(mediaDownloadFinishedFlagMutex);
    return(mediaDownloadFinishedFlag);
}

/**
 * @brief Returns the state of the media deleted finished flag.
 * @return State of the media deleted finished flag
 */
bool BebopDataTransferManager::mediaDeletedFinished()
{
    std::lock_guard<std::mutex> guard(mediaDeletedFinishedFlagMutex);
    return(mediaDeletedFinishedFlag);
}

/**
 * @brief Delete all the downloaded media on the bebop.
 */
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

/**
 * @brief Static call back method, executed when a media is deleted, it calls the medias_delete_completion method
 * @param arg Pointer to a BebopDataTransferManager object
 * @param media Pointer to the media object which is deleted
 * @param error Error
 */
void BebopDataTransferManager::medias_delete_completion_callback(void* arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error)
{
		static_cast<BebopDataTransferManager*>(arg)->medias_delete_completion(error);
}

/**
 * @brief Checks if the deletion of the downloaded medias is completed.
 * @param error Error
 */
void BebopDataTransferManager::medias_delete_completion(eARDATATRANSFER_ERROR error)
{
		if(error == ARDATATRANSFER_OK)
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
		else
		{
				std::cout << "Media delete errer: " << error << std::endl;
		}
}

/**
 * @brief Returns the number of downloaded files.
 * @return Number of downloaded files
 */
int BebopDataTransferManager::numberOfDownloadedFiles()
{
		std::lock_guard<std::mutex> guard(numberOfCurrentlyAvailableMediaToDownloadMutex);
		return(numberOfCurrentlyAvailableMediaToDownload);
}

/**
 * @brief Removes all downloaded pictures on the bebop and in the temporary folder.
 */
void BebopDataTransferManager::removePictures()
{
		deleteAllMedia();

		system("exec rm -r tmp/*");
}
