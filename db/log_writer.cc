// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/log_writer.h"

#include <stdint.h>

#include "leveldb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace leveldb {
namespace log {

static void InitTypeCrc(uint32_t* type_crc) {
  for (int i = 0; i <= kMaxRecordType; i++) {
    char t = static_cast<char>(i);
    type_crc[i] = crc32c::Value(&t, 1);
  }
}

Writer::Writer(WritableFile* dest) : dest_(dest), block_offset_(0) {
  InitTypeCrc(type_crc_);
}

Writer::Writer(WritableFile* dest, uint64_t dest_length)
    : dest_(dest), block_offset_(dest_length % kBlockSize) {
  InitTypeCrc(type_crc_);
}

Writer::~Writer() = default;

Status Writer::AddRecord(const Slice& slice) {                      // 对 log 的每次写入作为 record 添加
  const char* ptr = slice.data();
  size_t left = slice.size();

  // Fragment the record if necessary and emit it.  Note that if slice
  // is empty, we still want to iterate once to emit a single
  // zero-length record
  Status s;
  bool begin = true;
  do {
    const int leftover = kBlockSize - block_offset_;
    assert(leftover >= 0);
    if (leftover < kHeaderSize) {                                   // 如果当前 block 剩余的 size 小于 record 头长度
      // Switch to a new block
      if (leftover > 0) {
        // Fill the trailer (literal below relies on kHeaderSize being 7)
        static_assert(kHeaderSize == 7, "");
        dest_->Append(Slice("\x00\x00\x00\x00\x00\x00", leftover)); // 填充 trailer
      }
      block_offset_ = 0;
    }

    // Invariant: we never leave < kHeaderSize bytes in a block.
    assert(kBlockSize - block_offset_ - kHeaderSize >= 0);

    const size_t avail = kBlockSize - block_offset_ - kHeaderSize;
    const size_t fragment_length = (left < avail) ? left : avail;   // 根据block剩余size和写入size，划分出满足写入的最大record

    RecordType type;                                    // 确定 record type
    const bool end = (left == fragment_length);
    if (begin && end) {                                 // 新的slice小于avail，该slice可整个添加到当前Block中，不需要分段                              
      type = kFullType;                                 // type=kFullType
    } else if (begin) {                                 // 如果slice大于等于avail，则该slice需要分段存储，如果是第一段
      type = kFirstType;                                // type = kFirstType
    } else if (end) {   
      type = kLastType;                                 // 如果是最后一段，type = kLastType
    } else {
      type = kMiddleType;                               // 否则，type = kMiddleType
    }

    s = EmitPhysicalRecord(type, ptr, fragment_length); // 写入 record（将 record 存储到磁盘）
    ptr += fragment_length;
    left -= fragment_length;
    begin = false;
  } while (s.ok() && left > 0);              // 循环直至写入处理完成
  return s;
}

Status Writer::EmitPhysicalRecord(RecordType t, const char* ptr, size_t length) {
  assert(length <= 0xffff);  // Must fit in two bytes
  assert(block_offset_ + kHeaderSize + length <= kBlockSize);

  // Format the header
  char buf[kHeaderSize];
  buf[4] = static_cast<char>(length & 0xff);
  buf[5] = static_cast<char>(length >> 8);
  buf[6] = static_cast<char>(t);

  // Compute the crc of the record type and the payload.
  uint32_t crc = crc32c::Extend(type_crc_[t], ptr, length);
  crc = crc32c::Mask(crc);  // Adjust for storage
  EncodeFixed32(buf, crc);

  // Write the header and the payload
  Status s = dest_->Append(Slice(buf, kHeaderSize));    // 构造 record 头（checksum/size/type）
  if (s.ok()) {
    s = dest_->Append(Slice(ptr, length));              // 追加写入 log 文件
    if (s.ok()) {
      s = dest_->Flush();                               // 根据 option 指定的 sync 决定是否做 log 文件的强制 sync
    }
  }
  block_offset_ += kHeaderSize + length;
  return s;
}

}  // namespace log
}  // namespace leveldb
