#ifndef BEBOP_DATA_TRANSFER_MANAGER_H
#define BEBOP_DATA_TRANSFER_MANAGER_H

#include <mutex>
#include <vector>

extern "C"
{
  #include "libARDataTransfer/ARDataTransfer.h"
  #include "libARSAL/ARSAL.h"
}

#define DEVICE_PORT     21
#define MEDIA_FOLDER    "internal_000"

ARSAL_Thread_t threadRetreiveAllMedias;   // the thread that will do the media retrieving
ARSAL_Thread_t threadGetThumbnails;       // the thread that will download the thumbnails
ARSAL_Thread_t threadMediasDownloader;    // the thread that will download medias
ARDATATRANSFER_Manager_t *manager;        // the data transfer manager
ARUTILS_Manager_t *ftpListManager;        // an ftp that will do the list
ARUTILS_Manager_t *ftpQueueManager;       // an ftp that will do the download

static void* ARMediaStorage_retreiveAllMediasAsync(void* arg);

void getAllMediaAsync();

void medias_downloader_progress_callback(void* arg, ARDATATRANSFER_Media_t *media, float percent);

void medias_downloader_completion_callback(void* arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error);

ARDATATRANSFER_Media_t **medias;
//std::vector<ARDATATRANSFER_Media_t*> medias;
int count;

bool mediaAvailableFlag;
bool mediaDownloadFinishedFlag;

std::mutex accessMediasMutex;

class BebopDataTransferManager
{
  public:
    BebopDataTransferManager();

    ~BebopDataTransferManager();

    void startMediaListThread();

    void downloadMedias();

    bool mediaAvailable();
    bool mediaDownloadFinished();

    int numberOfDownloadedFiles();

    void removePictures();

  private:
    void createDataTransferManager();

    std::mutex removePicturesMutex;
};

#endif // BEBOP_DATA_TRANSFER_MANAGER_H
