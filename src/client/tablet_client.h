//
// tablet_client.h
// Copyright (C) 2017 4paradigm.com
// Author wangtaize 
// Date 2017-04-02 
// 


#ifndef RTIDB_TABLET_CLIENT_H
#define RTIDB_TABLET_CLIENT_H

#include "proto/tablet.pb.h"
#include "rpc/rpc_client.h"
#include "base/kv_iterator.h"

namespace rtidb {
namespace client {

class TabletClient {

public:
    TabletClient(const std::string& endpoint);

    ~TabletClient();

    bool CreateTable(const std::string& name, 
                     uint32_t id, 
                     uint32_t pid,
                     uint32_t ttl);

    bool CreateTable(const std::string& name,
                     uint32_t tid, uint32_t pid, uint32_t ttl,
                     bool leader, const std::vector<std::string>& endpoints);

    bool Put(uint32_t tid,
             uint32_t pid,
             const std::string& pk, 
             uint64_t time,
             const std::string& value);

    bool Put(uint32_t tid,
             uint32_t pid,
             const char* pk,
             uint64_t time,
             const char* value);

    ::rtidb::base::KvIterator* Scan(uint32_t tid,
             uint32_t pid,
             const std::string& pk,
             uint64_t stime,
             uint64_t etime,
             bool showm = false);

    ::rtidb::base::KvIterator* Scan(uint32_t tid,
             uint32_t pid,
             const char* pk,
             uint64_t stime,
             uint64_t etime,
             bool showm = false);

    bool DropTable(const uint32_t id);

    void ShowTp();

private:
    std::string endpoint_;
    ::rtidb::RpcClient client_;
    ::rtidb::api::TabletServer_Stub* tablet_;
    std::vector<uint64_t> percentile_;
};


}
}


#endif /* !RTIDB_TABLET_CLIENT_H */
