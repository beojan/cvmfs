/**
 * This file is part of the CernVM File System.
 */

#include "params.h"

#include <vector>

#include "options.h"
#include "util/string.h"

namespace receiver {

bool GetParamsFromFile(const std::string& repo_name, Params* params) {
  const std::string repo_config_file =
      "/etc/cvmfs/repositories.d/" + repo_name + "/server.conf";

  SimpleOptionsParser parser;
  if (!parser.TryParsePath(repo_config_file)) {
    return false;
  }

  bool ret = true;
  ret &=
      parser.GetValue("CVMFS_UPSTREAM_STORAGE", &params->spooler_configuration);

  // Note: if upstream is gateway, we change it to local. This should be made to
  // abort, but it's useful for testing on a single machine
  if (HasPrefix(params->spooler_configuration, "gw", false)) {
    std::vector<std::string> tokens = SplitString(repo_name, '/');
    const std::string rname = tokens.back();
    params->spooler_configuration =
        "local,/srv/cvmfs/" + rname + "/data/txn,/srv/cvmfs/" + rname;
  }

  std::string hash_algorithm_str;
  ret &= parser.GetValue("CVMFS_HASH_ALGORITHM", &hash_algorithm_str);
  params->hash_alg = shash::ParseHashAlgorithm(hash_algorithm_str);

  std::string compression_algorithm_str;
  ret &= parser.GetValue("CVMFS_COMPRESSION_ALGORITHM",
                         &compression_algorithm_str);
  params->compression_alg =
      zlib::ParseCompressionAlgorithm(compression_algorithm_str);

  std::string generate_legacy_bulk_chunks;
  ret &= parser.GetValue("CVMFS_GENERATE_LEGACY_BULK_CHUNKS",
                         &generate_legacy_bulk_chunks);
  if (generate_legacy_bulk_chunks == "true") {
    params->generate_legacy_bulk_chunks = true;
  } else if (generate_legacy_bulk_chunks == "false") {
    params->generate_legacy_bulk_chunks = false;
  } else {
    return false;
  }

  std::string use_chunking_str;
  ret &= parser.GetValue("CVMFS_USE_FILE_CHUNKING", &use_chunking_str);
  if (use_chunking_str == "true") {
    params->use_file_chunking = true;
  } else if (use_chunking_str == "false") {
    params->use_file_chunking = false;
  } else {
    return false;
  }

  std::string min_chunk_size_str;
  ret &= parser.GetValue("CVMFS_MIN_CHUNK_SIZE", &min_chunk_size_str);
  params->min_chunk_size = String2Uint64(min_chunk_size_str);

  std::string avg_chunk_size_str;
  ret &= parser.GetValue("CVMFS_AVG_CHUNK_SIZE", &avg_chunk_size_str);
  params->avg_chunk_size = String2Uint64(avg_chunk_size_str);

  std::string max_chunk_size_str;
  ret &= parser.GetValue("CVMFS_MAX_CHUNK_SIZE", &max_chunk_size_str);
  params->max_chunk_size = String2Uint64(max_chunk_size_str);

  std::string use_autocatalogs_str;
  ret &= parser.GetValue("CVMFS_AUTOCATALOGS", &use_autocatalogs_str);
  if (use_autocatalogs_str == "true") {
    params->use_autocatalogs = true;
  } else if (use_autocatalogs_str == "false") {
    params->use_autocatalogs = false;
  } else {
    return false;
  }

  std::string max_weight_str;
  if (parser.GetValue("CVMFS_AUTOCATALOGS_MAX_WEIGHT", &max_weight_str)) {
    params->max_weight = String2Uint64(max_weight_str);
  }

  std::string min_weight_str;
  if (parser.GetValue("CVMFS_AUTOCATALOGS_MIN_WEIGHT", &min_weight_str)) {
    params->min_weight = String2Uint64(min_weight_str);
  }

  return ret;
}

}  // namespace receiver