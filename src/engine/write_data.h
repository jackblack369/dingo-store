// Copyright (c) 2023 dingodb.com, Inc. All Rights Reserved
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DINGODB_ENGINE_WRITE_DATA_H_
#define DINGODB_ENGINE_WRITE_DATA_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "common/helper.h"
#include "proto/common.pb.h"
#include "proto/raft.pb.h"

namespace dingodb {

enum class DatumType {
  kPut = 0,
  kPutIfabsent = 1,
  kCreateSchema = 2,
  kDeleteRange = 3,
  kDeleteBatch = 4,
  kSplit = 5,
  kPrepareMerge = 6,
  kCommitMerge = 7,
  kRollbackMerge = 8,
  kCompareAndSet = 9,
  kMetaPut = 10,
  kRebuildVectorIndex = 11,
  kSaveRaftSnapshot = 12,
  kTxn = 13,
};

class DatumAble {
 public:
  virtual ~DatumAble() = default;
  virtual DatumType GetType() = 0;
  virtual pb::raft::Request* TransformToRaft() = 0;
  virtual void TransformFromRaft(pb::raft::Response& resonse) = 0;
};

struct PutDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kPut; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::PUT);
    request->set_ts(ts);

    pb::raft::PutRequest* put_request = request->mutable_put();
    put_request->set_cf_name(cf_name);
    for (auto& kv : kvs) {
      put_request->add_kvs()->Swap(&kv);
    }

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t ts{0};
  std::string cf_name;
  std::vector<pb::common::KeyValue> kvs;
};

struct DeleteBatchDatum : public DatumAble {
  ~DeleteBatchDatum() override = default;
  DatumType GetType() override { return DatumType::kDeleteBatch; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::DELETEBATCH);
    request->set_ts(ts);
    pb::raft::DeleteBatchRequest* delete_batch_request = request->mutable_delete_batch();
    delete_batch_request->set_cf_name(cf_name);
    for (auto& key : keys) {
      delete_batch_request->add_keys()->swap(key);
    }

    return request;
  }

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t ts{0};
  std::string cf_name;
  std::vector<std::string> keys;
};

struct DeleteRangeDatum : public DatumAble {
  ~DeleteRangeDatum() override = default;
  DatumType GetType() override { return DatumType::kDeleteRange; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::DELETERANGE);
    pb::raft::DeleteRangeRequest* delete_range_request = request->mutable_delete_range();
    delete_range_request->set_cf_name(cf_name);

    for (const auto& range : ranges) {
      *(delete_range_request->add_ranges()) = range;
    }

    return request;
  }

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  std::string cf_name;
  std::vector<pb::common::Range> ranges;
};

struct MetaPutDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kMetaPut; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::META_WRITE);

    pb::raft::RaftMetaRequest* meta_request = request->mutable_meta_req();
    auto* meta_increment_request = meta_request->mutable_meta_increment();
    meta_increment_request->Swap(&meta_increment);
    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  std::string cf_name;
  pb::coordinator_internal::MetaIncrement meta_increment;
};

struct VectorAddDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kPut; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::VECTOR_ADD);
    request->set_ts(ts);
    request->set_ttl(ttl);
    pb::raft::VectorAddRequest* vector_add_request = request->mutable_vector_add();
    vector_add_request->set_cf_name(cf_name);
    for (auto& vector : vectors) {
      vector_add_request->add_vectors()->Swap(&vector);
    }
    vector_add_request->set_is_update(is_update);

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t ts;
  int64_t ttl;
  std::string cf_name;
  std::vector<pb::common::VectorWithId> vectors;
  bool is_update{false};
};

struct VectorBatchAddDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kPut; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::VECTOR_BATCH_ADD);
    pb::raft::VectorBatchAddRequest* vector_batch_add_request = request->mutable_vector_batch_add();
    for (auto& vector : vectors) {
      vector_batch_add_request->add_vectors()->Swap(&vector);
    }
    for (auto& kv : kvs_default) {
      vector_batch_add_request->add_kvs_default()->Swap(&kv);
    }
    for (auto& kv : kvs_scalar) {
      vector_batch_add_request->add_kvs_scalar()->Swap(&kv);
    }
    for (auto& kv : kvs_table) {
      vector_batch_add_request->add_kvs_table()->Swap(&kv);
    }
    for (auto& kv : kvs_scalar_speed_up) {
      vector_batch_add_request->add_kvs_scalar_speed_up()->Swap(&kv);
    }
    for (const auto& id : delete_vector_ids) {
      vector_batch_add_request->add_delete_vector_ids(id);
    }

    vector_batch_add_request->set_is_update(is_update);

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  std::vector<pb::common::VectorWithId> vectors;
  std::vector<pb::common::KeyValue> kvs_default;
  std::vector<pb::common::KeyValue> kvs_scalar;
  std::vector<pb::common::KeyValue> kvs_table;
  std::vector<pb::common::KeyValue> kvs_scalar_speed_up;
  std::vector<int64_t> delete_vector_ids;
  bool is_update{false};
};

struct VectorDeleteDatum : public DatumAble {
  ~VectorDeleteDatum() override = default;
  DatumType GetType() override { return DatumType::kDeleteBatch; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::VECTOR_DELETE);
    request->set_ts(ts);
    pb::raft::VectorDeleteRequest* vector_delete_request = request->mutable_vector_delete();
    vector_delete_request->set_cf_name(cf_name);
    for (const auto& id : ids) {
      vector_delete_request->add_ids(id);
    }

    return request;
  }

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t ts;
  std::string cf_name;
  std::vector<int64_t> ids;
};

struct DocumentAddDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kPut; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::DOCUMENT_ADD);
    request->set_ts(ts);
    pb::raft::DocumentAddRequest* document_add_request = request->mutable_document_add();
    document_add_request->set_cf_name(cf_name);
    for (auto& document : documents) {
      document_add_request->add_documents()->Swap(&document);
    }
    document_add_request->set_is_update(is_update);

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t ts;
  std::string cf_name;
  std::vector<pb::common::DocumentWithId> documents;
  bool is_update{false};
};

struct DocumentBatchAddDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kPut; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::DOCUMENT_BATCH_ADD);
    pb::raft::DocumentBatchAddRequest* document_batch_add_request = request->mutable_document_batch_add();
    document_batch_add_request->set_cf_name(cf_name);
    for (auto& document : documents) {
      document_batch_add_request->add_documents()->Swap(&document);
    }
    for (const auto& id : delete_document_ids) {
      document_batch_add_request->add_delete_document_ids(id);
    }

    for (auto& kv : kvs) {
      document_batch_add_request->add_kvs()->Swap(&kv);
    }

    document_batch_add_request->set_is_update(is_update);

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  std::string cf_name;
  std::vector<pb::common::DocumentWithId> documents;
  std::vector<pb::common::KeyValue> kvs;
  std::vector<int64_t> delete_document_ids;
  bool is_update{false};
};

struct DocumentDeleteDatum : public DatumAble {
  ~DocumentDeleteDatum() override = default;
  DatumType GetType() override { return DatumType::kDeleteBatch; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::DOCUMENT_DELETE);
    request->set_ts(ts);
    pb::raft::DocumentDeleteRequest* document_delete_request = request->mutable_document_delete();
    document_delete_request->set_cf_name(cf_name);
    for (const auto& id : ids) {
      document_delete_request->add_ids(id);
    }

    return request;
  }

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t ts;
  std::string cf_name;
  std::vector<int64_t> ids;
};

// TxnDatum
struct TxnDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kTxn; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::TXN);

    pb::raft::TxnRaftRequest* txn_request = request->mutable_txn_raft_req();
    txn_request->Swap(&txn_request_to_raft);
    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  pb::raft::TxnRaftRequest txn_request_to_raft;
};

struct CreateSchemaDatum : public DatumAble {
  ~CreateSchemaDatum() override = default;
  DatumType GetType() override { return DatumType::kCreateSchema; }

  pb::raft::Request* TransformToRaft() override { return nullptr; }
  void TransformFromRaft(pb::raft::Response& resonse) override {}
};

struct SplitDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kSplit; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::SPLIT);
    pb::raft::SplitRequest* split_request = request->mutable_split();
    split_request->set_job_id(job_id);
    split_request->set_from_region_id(from_region_id);
    split_request->set_to_region_id(to_region_id);
    split_request->set_split_key(split_key);
    *(split_request->mutable_epoch()) = epoch;
    split_request->set_split_strategy(split_strategy);

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t job_id;
  int64_t from_region_id;
  int64_t to_region_id;
  std::string split_key;
  pb::common::RegionEpoch epoch;
  pb::raft::SplitStrategy split_strategy;
};

struct PrepareMergeDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kPrepareMerge; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::PREPARE_MERGE);
    auto* merge_request = request->mutable_prepare_merge();
    merge_request->set_job_id(job_id);
    merge_request->set_min_applied_log_id(min_applied_log_id);
    merge_request->set_target_region_id(target_region_id);
    *(merge_request->mutable_target_region_epoch()) = target_region_epoch;
    *(merge_request->mutable_target_region_range()) = target_region_range;

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t job_id;
  int64_t min_applied_log_id;
  int64_t target_region_id;
  pb::common::RegionEpoch target_region_epoch;
  pb::common::Range target_region_range;
};

struct CommitMergeDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kCommitMerge; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::COMMIT_MERGE);
    auto* merge_request = request->mutable_commit_merge();
    merge_request->set_job_id(job_id);
    merge_request->set_source_region_id(source_region_id);
    *(merge_request->mutable_source_region_epoch()) = source_region_epoch;
    *(merge_request->mutable_source_region_range()) = source_region_range;
    merge_request->set_prepare_merge_log_id(prepare_merge_log_id);
    Helper::VectorToPbRepeated(entries, merge_request->mutable_entries());

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t job_id;
  int64_t source_region_id;
  pb::common::RegionEpoch source_region_epoch;
  pb::common::Range source_region_range;
  int64_t prepare_merge_log_id;
  std::vector<pb::raft::LogEntry> entries;
};

struct RollbackMergeDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kRollbackMerge; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::ROLLBACK_MERGE);
    auto* merge_request = request->mutable_rollback_merge();
    merge_request->set_job_id(job_id);
    merge_request->set_min_applied_log_id(min_applied_log_id);
    merge_request->set_target_region_id(target_region_id);
    *(merge_request->mutable_target_region_epoch()) = target_region_epoch;

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t job_id;
  int64_t min_applied_log_id;
  int64_t target_region_id;
  pb::common::RegionEpoch target_region_epoch;
};

struct RebuildVectorIndexDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kRebuildVectorIndex; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::REBUILD_VECTOR_INDEX);
    auto* rebuild_request = request->mutable_rebuild_vector_index();

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}
};

struct SaveRaftSnapshotDatum : public DatumAble {
  DatumType GetType() override { return DatumType::kSaveRaftSnapshot; }

  pb::raft::Request* TransformToRaft() override {
    auto* request = new pb::raft::Request();

    request->set_cmd_type(pb::raft::CmdType::SAVE_RAFT_SNAPSHOT);
    request->mutable_save_snapshot()->set_region_id(region_id);

    return request;
  };

  void TransformFromRaft(pb::raft::Response& resonse) override {}

  int64_t region_id;
};

class WriteData {
 public:
  std::vector<std::shared_ptr<DatumAble>> Datums() const { return datums_; }
  void AddDatums(std::shared_ptr<DatumAble> datum) { datums_.push_back(datum); }

 private:
  std::vector<std::shared_ptr<DatumAble>> datums_;
};

// Build WriteData struct.
class WriteDataBuilder {
 public:
  // PutDatum
  static std::shared_ptr<WriteData> BuildWrite(const std::string& cf_name, std::vector<pb::common::KeyValue>& kvs,
                                               int64_t ts) {
    auto datum = std::make_shared<PutDatum>();
    datum->cf_name = cf_name;
    datum->kvs.swap(kvs);
    datum->ts = ts;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // VectorAddDatum
  static std::shared_ptr<WriteData> BuildWrite(const std::string& cf_name, int64_t ts, int64_t ttl,
                                               const std::vector<pb::common::VectorWithId>& vectors, bool is_update) {
    auto datum = std::make_shared<VectorAddDatum>();
    datum->cf_name = cf_name;
    datum->vectors = vectors;
    datum->is_update = is_update;
    datum->ts = ts;
    datum->ttl = ttl;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // VectorBatchAdd
  static std::shared_ptr<WriteData> BuildWrite(const std::vector<pb::common::VectorWithId>& vectors,
                                               const std::vector<pb::common::KeyValue>& kvs_default,
                                               const std::vector<pb::common::KeyValue>& kvs_scalar,
                                               const std::vector<pb::common::KeyValue>& kvs_table,
                                               const std::vector<pb::common::KeyValue>& kvs_scalar_speed_up,
                                               const std::vector<int64_t>& delete_vector_ids,
                                               bool is_update) {
    auto datum = std::make_shared<VectorBatchAddDatum>();
    datum->vectors = vectors;
    datum->kvs_default = kvs_default;
    datum->kvs_scalar = kvs_scalar;
    datum->kvs_table = kvs_table;
    datum->kvs_scalar_speed_up = kvs_scalar_speed_up;
    datum->delete_vector_ids = delete_vector_ids;
    datum->is_update = is_update;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // VectorDeleteDatum
  static std::shared_ptr<WriteData> BuildWrite(const std::string& cf_name, int64_t ts,
                                               const std::vector<int64_t>& ids) {
    auto datum = std::make_shared<VectorDeleteDatum>();
    datum->cf_name = cf_name;
    datum->ids = ids;
    datum->ts = ts;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // DocumentAddDatum
  static std::shared_ptr<WriteData> BuildWrite(const std::string& cf_name, int64_t ts,
                                               const std::vector<pb::common::DocumentWithId>& documents,
                                               bool is_update) {
    auto datum = std::make_shared<DocumentAddDatum>();
    datum->cf_name = cf_name;
    datum->documents = documents;
    datum->is_update = is_update;
    datum->ts = ts;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // DocumentBatchAddDatum
  static std::shared_ptr<WriteData> BuildWrite(const std::string& cf_name,
                                               const std::vector<pb::common::DocumentWithId>& documents, const std::vector<int64_t>& delete_document_ids,
                                               const std::vector<pb::common::KeyValue>& kvs, bool is_update) {
    auto datum = std::make_shared<DocumentBatchAddDatum>();
    datum->cf_name = cf_name;
    datum->documents = documents;
    datum->delete_document_ids = delete_document_ids;
    datum->kvs = kvs;
    datum->is_update = is_update;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // DocumentDeleteDatum
  static std::shared_ptr<WriteData> BuildWrite(const std::string& cf_name, int64_t ts, const std::vector<int64_t>& ids,
                                               bool is_document [[maybe_unused]]) {
    auto datum = std::make_shared<DocumentDeleteDatum>();
    datum->cf_name = cf_name;
    datum->ids = ids;
    datum->ts = ts;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // DeleteBatchDatum
  static std::shared_ptr<WriteData> BuildWrite(const std::string& cf_name, std::vector<std::string>& keys, int64_t ts) {
    auto datum = std::make_shared<DeleteBatchDatum>();
    datum->cf_name = cf_name;
    datum->keys.swap(keys);
    datum->ts = ts;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // DeleteRangeDatum
  static std::shared_ptr<WriteData> BuildWrite(const std::string& cf_name, const pb::common::Range& range) {
    auto datum = std::make_shared<DeleteRangeDatum>();
    datum->cf_name = cf_name;
    datum->ranges.emplace_back(std::move(const_cast<pb::common::Range&>(range)));

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // MetaPutDatum
  static std::shared_ptr<WriteData> BuildWrite(const std::string& cf_name,
                                               pb::coordinator_internal::MetaIncrement& meta_increment) {
    auto datum = std::make_shared<MetaPutDatum>();
    datum->cf_name = cf_name;
    datum->meta_increment.Swap(&meta_increment);

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // TxnDatum
  static std::shared_ptr<WriteData> BuildWrite(pb::raft::TxnRaftRequest& txn_raft_request) {
    auto datum = std::make_shared<TxnDatum>();
    datum->txn_request_to_raft.Swap(&txn_raft_request);

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // SplitDatum
  static std::shared_ptr<WriteData> BuildWrite(int64_t job_id, const pb::coordinator::SplitRequest& split_request,
                                               const pb::common::RegionEpoch& epoch) {
    auto datum = std::make_shared<SplitDatum>();
    datum->job_id = job_id;
    datum->from_region_id = split_request.split_from_region_id();
    datum->to_region_id = split_request.split_to_region_id();
    datum->split_key = split_request.split_watershed_key();
    datum->epoch = epoch;
    if (split_request.store_create_region()) {
      datum->split_strategy = pb::raft::POST_CREATE_REGION;
    } else {
      datum->split_strategy = pb::raft::PRE_CREATE_REGION;
    }

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // PrepareMergeDatum
  static std::shared_ptr<WriteData> BuildWrite(int64_t job_id, const pb::common::RegionDefinition& region_definition,
                                               int64_t min_applied_log_id) {
    auto datum = std::make_shared<PrepareMergeDatum>();
    datum->job_id = job_id;
    datum->min_applied_log_id = min_applied_log_id;
    datum->target_region_id = region_definition.id();
    datum->target_region_epoch = region_definition.epoch();
    datum->target_region_range = region_definition.range();

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // CommitMergeDatum
  static std::shared_ptr<WriteData> BuildWrite(int64_t job_id, const pb::common::RegionDefinition& region_definition,
                                               int64_t prepare_merge_log_id,
                                               const std::vector<pb::raft::LogEntry>& entries) {
    auto datum = std::make_shared<CommitMergeDatum>();
    datum->job_id = job_id;
    datum->source_region_id = region_definition.id();
    datum->source_region_epoch = region_definition.epoch();
    datum->source_region_range = region_definition.range();
    datum->prepare_merge_log_id = prepare_merge_log_id;
    datum->entries = entries;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // RollbackMergeDatum
  static std::shared_ptr<WriteData> BuildWrite(int64_t job_id, const pb::common::RegionEpoch& epoch,
                                               int64_t target_region_id, int64_t min_applied_log_id) {
    auto datum = std::make_shared<RollbackMergeDatum>();
    datum->job_id = job_id;
    datum->min_applied_log_id = min_applied_log_id;
    datum->target_region_id = target_region_id;
    datum->target_region_epoch = epoch;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // RebuildVectorIndexDatum
  static std::shared_ptr<WriteData> BuildWrite() {
    auto datum = std::make_shared<RebuildVectorIndexDatum>();

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }

  // SaveRaftSnapshotDatum
  static std::shared_ptr<WriteData> BuildWrite(int64_t region_id) {
    auto datum = std::make_shared<SaveRaftSnapshotDatum>();
    datum->region_id = region_id;

    auto write_data = std::make_shared<WriteData>();
    write_data->AddDatums(std::static_pointer_cast<DatumAble>(datum));

    return write_data;
  }
};

}  // namespace dingodb

#endif  // DINGODB_ENGINE_WRITE_DATA_H_