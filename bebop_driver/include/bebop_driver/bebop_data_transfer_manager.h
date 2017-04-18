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

class BebopDataTransferManager
{
  public:
    BebopDataTransferManager();

    ~BebopDataTransferManager();

    void startMediaListThread();

    void downloadMedias();

    bool mediaAvailable();
    bool mediaDownloadFinished();

    bool mediaDeletedFinished();

    int numberOfDownloadedFiles();

    void removePictures();

	private:
		ARDATATRANSFER_Manager_t *manager;        // the data transfer manager
		ARSAL_Thread_t threadRetreiveAllMedias;   // the thread that will do the media retrieving
		ARSAL_Thread_t threadGetThumbnails;       // the thread that will download the thumbnails
		ARSAL_Thread_t *threadMediasDownloaderPtr;// pointer to the thread that will download medias
		//ARSAL_Thread_t threadMediasDownloader;    // the thread that will download medias

		ARUTILS_Manager_t *ftpListManager;        // an ftp that will do the list
		ARUTILS_Manager_t *ftpQueueManager;       // an ftp that will do the download

		void createDataTransferManager();

		static void* ARMediaStorage_retreiveAllMediasAsync(void *arg);

		void getAllMediaAsync();
		std::mutex getAllMediaAsyncMutex;

		static void medias_downloader_progress_callback(void* arg, ARDATATRANSFER_Media_t *media, float percent);

		static void medias_downloader_completion_callback(void* arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error);
		void medias_downloader_completion();

		void deleteAllMedia();

		static void medias_delete_completion_callback(void *arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error);
		void medias_delete_completion(eARDATATRANSFER_ERROR error);

		std::vector<ARDATATRANSFER_Media_t*> medias;
		std::mutex localMediasMutex;

		bool downloadingMediasFlag;

		int numberOfCurrentlyAvailableMediaToDownload;
		std::mutex numberOfCurrentlyAvailableMediaToDownloadMutex;

		int numberOfCurrentlyDownloadedNotDeletedMedia;
		std::mutex numberOfCurrentlyDownloadedNotDeletedMediaMutex;

		int numberOfCurrentlyDeletedMedia;
		std::mutex numberOfCurrentlyDeletedMediaMutex;

		bool mediaAvailableFlag;
		std::mutex mediaAvailableFlagMutex;
		bool mediaDownloadFinishedFlag;
		std::mutex mediaDownloadFinishedFlagMutex;
		bool mediaDeletedFinishedFlag;
		std::mutex mediaDeletedFinishedFlagMutex;

};

#endif // BEBOP_DATA_TRANSFER_MANAGER_H
