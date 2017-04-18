#ifndef BEBOP_DATA_TRANSFER_MANAGER_H
#define BEBOP_DATA_TRANSFER_MANAGER_H

// Includes
#include <mutex>
#include <vector>

extern "C"
{
  #include "libARDataTransfer/ARDataTransfer.h"
  #include "libARSAL/ARSAL.h"
}

#define DEVICE_PORT     21
#define MEDIA_FOLDER    "internal_000"

/**
 * @brief The BebopDataTransferManager class
 * Creates a data transfer manager instance
 */
class BebopDataTransferManager
{
  public:
    /**
     * @brief Constructor
     */
    BebopDataTransferManager();

    /**
     * @brief Destructor
     * Terminates media download, deletes the media download manager and cleans up
     */
    ~BebopDataTransferManager();

		/**
		 * @brief starts media list thread
		 */
    void startMediaListThread();

		/**
		 * @brief Adds all media to the media dowload queue and starts the download.
		 */
		void downloadMedias();

		/**
		 * @brief Returns the state of the media available flag.
		 * @return State of the media available flag
		 */
		bool mediaAvailable();

		/**
		 * @brief Returns the state of the media download finished flag.
		 * @return State of the media download finished flag
		 */
		bool mediaDownloadFinished();

		/**
		 * @brief Returns the state of the media deleted finished flag.
		 * @return State of the media deleted finished flag
		 */
		bool mediaDeletedFinished();

		/**
		 * @brief Returns the number of downloaded files.
		 * @return Number of downloaded files
		 */
		int numberOfDownloadedFiles();

		/**
		 * @brief Removes all downloaded pictures on the bebop and in the temporary folder.
		 */
		void removePictures();

	private:
		/**
		 * @brief Creates a data transfer manager
		 */
		void createDataTransferManager();

		/**
		 * @brief Starts asynchronous media synchronisation in the getAllMediaAsync method
		 * @param arg Pointer to a BebopDataTransferManager object
		 * @return NULL
		 */
		static void* ARMediaStorage_retreiveAllMediasAsync(void *arg);

		/**
		 * @brief Checks for available media. If media is available, the number of medias is set and the media addresses are writen to the corresponding vector.
		 */
		void getAllMediaAsync();

		std::mutex getAllMediaAsyncMutex;					/**< Mutex to access getAllMediaAsync method */

		/**
		 * @brief Static call back method, executed when a download is in progress
		 * @param arg Pointer to a BebopDataTransferManager object
		 * @param media Pointer to the media object which is dowloading
		 * @param percent Progress in percent
		 */
		static void medias_downloader_progress_callback(void* arg, ARDATATRANSFER_Media_t *media, float percent);

		/**
		 * @brief Static call back method, executed when a download is finished, it calls the medias_downloader_completion method
		 * @param arg Pointer to a BebopDataTransferManager object
		 * @param media Pointer to the media object which is dowloading
		 * @param error Error
		 */
		static void medias_downloader_completion_callback(void* arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error);

		/**
		 * @brief Method to check if all media is downloaded
		 */
		void medias_downloader_completion();

		/**
		 * @brief Delete all the downloaded media on the bebop.
		 */
		void deleteAllMedia();

		/**
		 * @brief Static call back method, executed when a media is deleted, it calls the medias_delete_completion method
		 * @param arg Pointer to a BebopDataTransferManager object
		 * @param media Pointer to the media object which is deleted
		 * @param error Error
		 */
		static void medias_delete_completion_callback(void *arg, ARDATATRANSFER_Media_t *media, eARDATATRANSFER_ERROR error);

		/**
		 * @brief Checks if the deletion of the downloaded medias is completed.
		 * @param error Error
		 */
		void medias_delete_completion(eARDATATRANSFER_ERROR error);

		ARDATATRANSFER_Manager_t *manager;        /**< Pointer to the data transfer manager */
		ARSAL_Thread_t threadRetreiveAllMedias;   /**< The thread that will do the media retrieving */
		ARSAL_Thread_t threadGetThumbnails;       /**< The thread that will download the thumbnails */
		ARSAL_Thread_t *threadMediasDownloaderPtr;/**< Pointer to the thread that will download medias */

		ARUTILS_Manager_t *ftpListManager;        /**< Pointer to an ftp that will do the list */
		ARUTILS_Manager_t *ftpQueueManager;       /**< Pointer to an ftp that will do the download */

		std::vector<ARDATATRANSFER_Media_t*> medias;	/**< Vector containing the addresses of the medias to download */
		std::mutex localMediasMutex;									/**< Mutex to access the vector */

		int numberOfCurrentlyAvailableMediaToDownload;	/**< The number of currently available media to download */
		std::mutex numberOfCurrentlyAvailableMediaToDownloadMutex;	/**< Mutex to access the number of currently available media to download */

		int numberOfCurrentlyDownloadedNotDeletedMedia;	/**< The number of currently downloaded and not yet deleted media */
		std::mutex numberOfCurrentlyDownloadedNotDeletedMediaMutex;	/**< Mutex to protect the number of currently downloaded and not yet deleted media */

		int numberOfCurrentlyDeletedMedia;				/**< The number of currently deleted media */
		std::mutex numberOfCurrentlyDeletedMediaMutex; /**< Mutex to access the number of currently deleted media */

		bool mediaAvailableFlag;									/**< Flag indicating available media */
		std::mutex mediaAvailableFlagMutex;				/**< Mutex to access the media available flag */

		bool mediaDownloadFinishedFlag;						/**< Flag indicating media is finised downloading */
		std::mutex mediaDownloadFinishedFlagMutex;	/**< Mutex to access the media download finished flag */

		bool mediaDeletedFinishedFlag;						/**< Flag indicating media is deleted */
		std::mutex mediaDeletedFinishedFlagMutex; /**< Mutex to access the media deleted finished flag */
};

#endif // BEBOP_DATA_TRANSFER_MANAGER_H
