//ok
// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include <algorithm>
#include <limits>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/casts.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {
namespace io {

namespace {

// Default block size for Copying{In,Out}putStreamAdaptor.
static const int kDefaultBlockSize = 8192;

}  // namespace

// ===================================================================

ArrayInputStream::ArrayInputStream(const void* data, int size, int block_size)
    //data��Ϊ������н���
    : data_(reinterpret_cast<const uint8_t*>(data)),
      size_(size),
      block_size_(block_size > 0 ? block_size : size),
      position_(0),
      last_returned_size_(0) {}

bool ArrayInputStream::Next(const void** data, int* size) {
  if (position_ < size_) {
    last_returned_size_ = std::min(block_size_, size_ - position_);
    *data = data_ + position_;
    *size = last_returned_size_;
    position_ += last_returned_size_;
    return true;
  } else {
    // We're at the end of the array.
    last_returned_size_ = 0;  // Don't let caller back up.
    return false;
  }
}

//ok ��position_��ǰ��count
void ArrayInputStream::BackUp(int count) {
  GOOGLE_CHECK_GT(last_returned_size_, 0)
      << "BackUp() can only be called after a successful Next().";
  GOOGLE_CHECK_LE(count, last_returned_size_);
  GOOGLE_CHECK_GE(count, 0);
  position_ -= count;
  last_returned_size_ = 0;  // Don't let caller back up further.
}

bool ArrayInputStream::Skip(int count) {
  GOOGLE_CHECK_GE(count, 0);
  last_returned_size_ = 0;  // Don't let caller back up.
  if (count > size_ - position_) {
  //skip���ɹ���Ҳ��position_�Ƶ������
    position_ = size_;
    return false;
  } else {
    position_ += count;
    return true;
  }
}

int64_t ArrayInputStream::ByteCount() const { return position_; }


// ===================================================================

ArrayOutputStream::ArrayOutputStream(void* data, int size, int block_size)
    : data_(reinterpret_cast<uint8_t*>(data)),
      size_(size),
      block_size_(block_size > 0 ? block_size : size),
      position_(0),
      last_returned_size_(0) {}

bool ArrayOutputStream::Next(void** data, int* size) {
  if (position_ < size_) {
    last_returned_size_ = std::min(block_size_, size_ - position_);
    *data = data_ + position_;
    *size = last_returned_size_;
    position_ += last_returned_size_;
    return true;
  } else {
    // We're at the end of the array.
    last_returned_size_ = 0;  // Don't let caller back up.
    return false;
  }
}

void ArrayOutputStream::BackUp(int count) {
  GOOGLE_CHECK_GT(last_returned_size_, 0)
      << "BackUp() can only be called after a successful Next().";
  GOOGLE_CHECK_LE(count, last_returned_size_);
  GOOGLE_CHECK_GE(count, 0);
  position_ -= count;
  last_returned_size_ = 0;  // Don't let caller back up further.
}

int64_t ArrayOutputStream::ByteCount() const { return position_; }

// ===================================================================

StringOutputStream::StringOutputStream(std::string* target) : target_(target) {}
//ok ��ȡ���õ��ڴ�ָ��Ͷ�Ӧ���ڴ��С
bool StringOutputStream::Next(void** data, int* size) {
  GOOGLE_CHECK(target_ != NULL);
  //��֪��Ϊʲô����size_type
  //size_type�Ķ���ͨ����size_t��ͬ
  size_t old_size = target_->size();

  // Grow the string.
  size_t new_size;
  if (old_size < target_->capacity()) {
    // Resize the string to match its capacity, since we can get away
    // without a memory allocation this way.
    new_size = target_->capacity(); 
  } else {
    // Size has reached capacity, try to double it.
    new_size = old_size * 2;
  }
  // Avoid integer overflow in returned '*size'.
  new_size = std::min(new_size, old_size + std::numeric_limits<int>::max());
  // Increase the size, also make sure that it is at least kMinimumSize.
  STLStringResizeUninitialized(
      target_,
      std::max(new_size,
               kMinimumSize + 0));  // "+ 0" works around GCC4 weirdness.

  *data = mutable_string_data(target_) + old_size;
  *size = target_->size() - old_size;
  return true;
}
//ok
void StringOutputStream::BackUp(int count) {
  GOOGLE_CHECK_GE(count, 0);
  GOOGLE_CHECK(target_ != NULL);
  GOOGLE_CHECK_LE(static_cast<size_t>(count), target_->size());
  target_->resize(target_->size() - count);
}
//ok
int64_t StringOutputStream::ByteCount() const {
  GOOGLE_CHECK(target_ != NULL);
  return target_->size();
}

// ===================================================================
//ok ����count bytes������
int CopyingInputStream::Skip(int count) {
  char junk[4096];
  int skipped = 0;
  while (skipped < count) {
    int bytes = Read(junk, std::min(count - skipped,
                                    implicit_cast<int>(sizeof(junk))));
    if (bytes <= 0) {
      // EOF or read error.
      return skipped;
    }
    skipped += bytes;
  }
  return skipped;
}
//ok
CopyingInputStreamAdaptor::CopyingInputStreamAdaptor(
    CopyingInputStream* copying_stream, int block_size)
    : copying_stream_(copying_stream),
      owns_copying_stream_(false),
      failed_(false),
      position_(0),
      buffer_size_(block_size > 0 ? block_size : kDefaultBlockSize),
      buffer_used_(0),
      backup_bytes_(0) {}
//ok
CopyingInputStreamAdaptor::~CopyingInputStreamAdaptor() {
  if (owns_copying_stream_) {
    delete copying_stream_;
  }
}
//ok ��CopyingInputStream�ж�ȡbuffer_size_��buffer_.get()���ٰ�dataָ��buffer_.get()
bool CopyingInputStreamAdaptor::Next(const void** data, int* size) {
  if (failed_) {
    // Already failed on a previous read.
    return false;
  }

  AllocateBufferIfNeeded();
  //��backup bytes�³���
  if (backup_bytes_ > 0) {
    // We have data left over from a previous BackUp(), so just return that.
    *data = buffer_.get() + buffer_used_ - backup_bytes_;
    *size = backup_bytes_;
    backup_bytes_ = 0;
    return true;
  }
  // Read new data into the buffer.
  //��CopyingInputStream�ж�ȡbuffer_size_��buffer_.get()
  buffer_used_ = copying_stream_->Read(buffer_.get(), buffer_size_);
  if (buffer_used_ <= 0) {
    // EOF or read error.  We don't need the buffer anymore.
    if (buffer_used_ < 0) {
      // Read error (not EOF).
      failed_ = true;
    }
    FreeBuffer();
    return false;
  }
  position_ += buffer_used_;

  *size = buffer_used_;
  *data = buffer_.get();
  return true;
}
//ok
void CopyingInputStreamAdaptor::BackUp(int count) {
  GOOGLE_CHECK(backup_bytes_ == 0 && buffer_.get() != NULL)
      << " BackUp() can only be called after Next().";
  GOOGLE_CHECK_LE(count, buffer_used_)
      << " Can't back up over more bytes than were returned by the last call"
         " to Next().";
  GOOGLE_CHECK_GE(count, 0) << " Parameter to BackUp() can't be negative.";

  backup_bytes_ = count;
}
//ok ����count bytes������
bool CopyingInputStreamAdaptor::Skip(int count) {
  GOOGLE_CHECK_GE(count, 0);

  if (failed_) {
    // Already failed on a previous read.
    return false;
  }

  // First skip any bytes left over from a previous BackUp().
  if (backup_bytes_ >= count) {
    // We have more data left over than we're trying to skip.  Just chop it.
    backup_bytes_ -= count;
    return true;
  }

  count -= backup_bytes_;
  backup_bytes_ = 0;

  int skipped = copying_stream_->Skip(count);
  position_ += skipped;
  return skipped == count;
}
//ok
int64_t CopyingInputStreamAdaptor::ByteCount() const {
  return position_ - backup_bytes_;
}
//ok ����buffer_
void CopyingInputStreamAdaptor::AllocateBufferIfNeeded() {
  if (buffer_.get() == NULL) {
    buffer_.reset(new uint8_t[buffer_size_]);
  }
}
//ok ����buffer_
void CopyingInputStreamAdaptor::FreeBuffer() {
  GOOGLE_CHECK_EQ(backup_bytes_, 0);
  buffer_used_ = 0;
  //reset֮��buffer_.get()���ǿ�ָ����
  buffer_.reset();
}

// ===================================================================
//ok
CopyingOutputStreamAdaptor::CopyingOutputStreamAdaptor(
    CopyingOutputStream* copying_stream, int block_size)
    : copying_stream_(copying_stream),
      owns_copying_stream_(false),
      failed_(false),
      position_(0),
      buffer_size_(block_size > 0 ? block_size : kDefaultBlockSize),
      buffer_used_(0) {}
//ok
CopyingOutputStreamAdaptor::~CopyingOutputStreamAdaptor() {
  WriteBuffer();
  if (owns_copying_stream_) {
    delete copying_stream_;
  }
}
//ok
bool CopyingOutputStreamAdaptor::Flush() { return WriteBuffer(); }
//ok
bool CopyingOutputStreamAdaptor::Next(void** data, int* size) {
  //һ������backup����ִ��WriteBuffer
  //���򣬽��ݴ���������д���ļ�
  if (buffer_used_ == buffer_size_) {
    if (!WriteBuffer()) return false;
  }

  AllocateBufferIfNeeded();
  //һ������backup����backup��С���ݴ�������ȥ
  //�����ʱbuffer_used_һ��Ϊ0���൱�ڰ�buffer_size_��С���ݴ�������ȥ
  *data = buffer_.get() + buffer_used_;
  *size = buffer_size_ - buffer_used_;
  buffer_used_ = buffer_size_;
  return true;
}
//ok backup��Ч����Next������
void CopyingOutputStreamAdaptor::BackUp(int count) {
  GOOGLE_CHECK_GE(count, 0);
  GOOGLE_CHECK_EQ(buffer_used_, buffer_size_)
      << " BackUp() can only be called after Next().";
  GOOGLE_CHECK_LE(count, buffer_used_)
      << " Can't back up over more bytes than were returned by the last call"
         " to Next().";

  buffer_used_ -= count;
}
//ok
int64_t CopyingOutputStreamAdaptor::ByteCount() const {
  return position_ + buffer_used_;
}
//data��ָ��Ҫд�����ݣ�size����Ҫд�Ĵ�С
//ok ��size��Сdata�е�����д���ļ�
bool CopyingOutputStreamAdaptor::WriteAliasedRaw(const void* data, int size) {
  if (size >= buffer_size_) {
  //���size����buffer_size_���޷�buffer��ֱ�ӽ�����д���ļ�
    if (!Flush() || !copying_stream_->Write(data, size)) {
      return false;
    }
    GOOGLE_DCHECK_EQ(buffer_used_, 0);
    position_ += size;
    return true;
  }

  void* out;
  int out_size;
  while (true) {
    if (!Next(&out, &out_size)) {
      return false;
    }

    if (size <= out_size) {
      std::memcpy(out, data, size);
      BackUp(out_size - size);
      return true;
    }

    std::memcpy(out, data, out_size);
    data = static_cast<const char*>(data) + out_size;
    size -= out_size;
  }
  return true;
}

//ok ��buffer_���ݴ������д��copying_stream_��
bool CopyingOutputStreamAdaptor::WriteBuffer() {
  if (failed_) {
    // Already failed on a previous write.
    return false;
  }

  if (buffer_used_ == 0) return true;

  if (copying_stream_->Write(buffer_.get(), buffer_used_)) {
    position_ += buffer_used_;
    buffer_used_ = 0;
    return true;
  } else {
    failed_ = true;
    FreeBuffer();
    return false;
  }
}
//ok Ϊbuffer_�����ڴ�
void CopyingOutputStreamAdaptor::AllocateBufferIfNeeded() {
  if (buffer_ == NULL) {
    buffer_.reset(new uint8_t[buffer_size_]);
  }
}
//ok ���buffer
void CopyingOutputStreamAdaptor::FreeBuffer() {
  buffer_used_ = 0;
  buffer_.reset();
}

// ===================================================================

//ok
LimitingInputStream::LimitingInputStream(ZeroCopyInputStream* input,
                                         int64_t limit)
    : input_(input), limit_(limit) {
  prior_bytes_read_ = input_->ByteCount();
}
//ok
LimitingInputStream::~LimitingInputStream() {
  // If we overshot the limit, back up.
  if (limit_ < 0) input_->BackUp(-limit_);
}

//ok
bool LimitingInputStream::Next(const void** data, int* size) {
  if (limit_ <= 0) return false;
  if (!input_->Next(data, size)) return false;

  limit_ -= *size;
  if (limit_ < 0) {
    // We overshot the limit.  Reduce *size to hide the rest of the buffer.
    *size += limit_;
  }
  return true;
}

//ok
void LimitingInputStream::BackUp(int count) {
  if (limit_ < 0) {
    //����backup��count + (-limit)�����Ժ������limit_ = count;
    input_->BackUp(count - limit_);
    limit_ = count;
  } else {
    input_->BackUp(count);
    limit_ += count;
  }
}

//ok
bool LimitingInputStream::Skip(int count) {
  if (count > limit_) {
    if (limit_ < 0) return false;
    input_->Skip(limit_);
    limit_ = 0;
    return false;
  } else {
    if (!input_->Skip(count)) return false;
    limit_ -= count;
    return true;
  }
}
//ok
int64_t LimitingInputStream::ByteCount() const {
  if (limit_ < 0) {
    return input_->ByteCount() + limit_ - prior_bytes_read_;
  } else {
    return input_->ByteCount() - prior_bytes_read_;
  }
}


// ===================================================================

}  // namespace io
}  // namespace protobuf
}  // namespace google
