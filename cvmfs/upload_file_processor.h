/**
 * This file is part of the CernVM File System.
 */

#ifndef CVMFS_UPLOAD_FILE_PROCESSOR_H_
#define CVMFS_UPLOAD_FILE_PROCESSOR_H_

#include "util_concurrency.h"

namespace upload {

class AbstractUploader;
struct UploaderResults;

/**
 * Adds a temporary file path to the FileChunk structure
 * This is needed internally before the file is actually stored under it's
 * content hash
 */
class TemporaryFileChunk : public FileChunk {
 public:
  enum UploadState {
    kUploadPending,
    kUploadSuccessful,
    kUploadFailed
  };

 public:
  TemporaryFileChunk() :
    upload_state_(kUploadPending) {}
  TemporaryFileChunk(const off_t offset, const size_t size) :
    FileChunk(hash::Any(), offset, size),
    upload_state_(kUploadPending) {}

  const std::string& temporary_path() const { return temporary_path_;  }

 protected:
  friend class FileProcessor;
  friend class PendingFile;
  void set_content_hash(const hash::Any &hash)     { content_hash_   = hash;  }
  void set_temporary_path(const std::string &path) { temporary_path_ = path;  }
  void set_upload_state(const UploadState &state)  { upload_state_   = state; }

 private:
  std::string  temporary_path_; //!< location of the compressed file chunk (generated by FileProcessor)
  UploadState  upload_state_;   //!< flag that holds the uploading state of this chunk
};
typedef std::vector<TemporaryFileChunk> TemporaryFileChunks;

/**
 * PendingFiles are created for each processing job. It encapsulates the syn-
 * chronisation of FileProcessor and AbstractUploader.
 * When a FileChunk was successfully created, it is scheduled for upload in the
 * AbstractUploader, which in turn notifies the responsible PendingFile object
 * once the chunk was uploaded. When a PendingFile object determines itself to
 * be completely finished, it notifies the FileProcessor which then hands out
 * the final results (FileProcessor::Results).
 */
class PendingFile : public Callbackable<std::string>,
                    public Lockable {
 public:
  typedef std::map<std::string, TemporaryFileChunk> TemporaryFileChunkMap;

 public:
  PendingFile(const std::string &local_path,
              const callback_t  *callback) :
    local_path_(local_path),
    finished_callback_(callback),
    chunks_uploaded_(0), errors_(0),
    processing_complete_(false), uploading_complete_(false) {}
  virtual ~PendingFile();

  void AddChunk(const TemporaryFileChunk  &file_chunk);
  void AddBulk (const TemporaryFileChunk  &file_chunk);

  /**
   * If the FileProcessor created only one single FileChunk, it will call this
   * method to set this one chunk as the bulk version of the file
   * (performance optimization)
   */
  void PromoteSingleChunkToBulk();

  /**
   * Callback method that gets called for each uploaded file chunk of a Pending-
   * File object.
   *
   * @param data   the results of the finished uploading job
   */
  void UploadCallback(const UploaderResults &data);

  /**
   * Checks if the file was completely processed and uploaded and notifies the
   * Spooler in positive case.
   */
  void CheckForCompletionAndNotify();

  /**
   * Once a file is processed completely processed by the FileProcessor, it
   * notifies the PendingFile by calling this method.
   * Note: It might be, that the PendingFile still needs to wait for upload jobs
   *       to be finished!
   */
  void FinalizeProcessing();

  FileChunks GetFinalizedFileChunks() const;
  FileChunk  GetFinalizedBulkFile() const;

  bool IsCompleted() const { return processing_complete_ && uploading_complete_; }
  bool IsCompletedSuccessfully() const { return IsCompleted() && errors_ == 0; }

 private:
  const std::string      local_path_;
  const callback_t      *finished_callback_;

  TemporaryFileChunkMap  file_chunks_;
  TemporaryFileChunk     bulk_chunk_;

  unsigned int           chunks_uploaded_;
  unsigned int           errors_;

  bool                   processing_complete_;
  bool                   uploading_complete_;
};


/**
 * Implements a concurrent compression worker based on the Concurrent-
 * Workers template. File compression is done in parallel when possible.
 */
class FileProcessor : public ConcurrentWorker<FileProcessor> {
 public:

  /**
   * Initialization data for the file processor
   * This will be passed for each spawned FileProcessor by the
   * ConcurrentWorkers implementation
   */
  struct worker_context {
    worker_context(const std::string &temporary_path,
                   const bool         use_file_chunking,
                   AbstractUploader  *uploader) :
      temporary_path(temporary_path),
      use_file_chunking(use_file_chunking),
      uploader(uploader) {}
    const std::string temporary_path; //!< base path to store processing
                                      //!< results in temporary files
    const bool        use_file_chunking;
    AbstractUploader *uploader;
  };


  /**
   * Encapuslates all the needed information for one FileProcessor job
   * Will be filled by the user and then scheduled as a job into the
   * ConcurrentWorkers environment.
   */
  struct Parameters {
    Parameters(const std::string &local_path,
               const bool         allow_chunking) :
      local_path(local_path),
      allow_chunking(allow_chunking) {}

    // default constructor to create an 'empty' struct
    // (needed by the ConcurrentWorkers implementation)
    Parameters() :
      local_path(), allow_chunking(false) {}

    const std::string local_path;     //!< path to the local file to be processed
    const bool        allow_chunking; //!< enables file chunking for this job
  };

  /**
   * The results generated for each scheduled FileProcessor job
   * Users get this data structure when registering to the callback interface
   * provided by the ConcurrentWorkers template.
   */
  struct Results {
    Results(const std::string &local_path,
            const int          return_code = -1) :
      return_code(return_code),
      local_path(local_path) {}

    int                return_code; //!< 0 if job was successful
    FileChunk          bulk_file;   //!< results of the bulk file processing
    FileChunks         file_chunks; //!< list of the generated file chunks
    const std::string  local_path;  //!< path to the local file that was processed (same as given in Parameters)

    inline bool IsChunked() const { return !file_chunks.empty(); }
  };

  // these typedefs are needed for the ConcurrentWorkers template
  typedef Parameters expected_data;
  typedef Results    returned_data;


 public:
  FileProcessor(const worker_context *context);
  void operator()(const Parameters &data);

 protected:
  int GenerateFileChunks(const MemoryMappedFile &mmf,
                               PendingFile      *file) const;
  bool GenerateBulkFile(const MemoryMappedFile   &mmf,
                              PendingFile        *file) const;

  bool ProcessFileChunk(const MemoryMappedFile   &mmf,
                              TemporaryFileChunk &chunk) const;

  void UploadChunk(const TemporaryFileChunk &file_chunk,
                         PendingFile        *file) const;

  void ProcessingCompleted(const std::string &local_path);

 private:
  class PendingFiles : public std::map<std::string, PendingFile*>,
                       public Lockable {};

 private:
  const std::string                     temporary_path_;
  const bool                            use_file_chunking_;
  mutable AbstractUploader              *uploader_;

  PendingFiles                          pending_files_;
};

}

#endif /* UPLOAD_FILE_PROCESSOR */

