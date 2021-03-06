// Copyright 2018 the MetaHash project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vm/work-context.h"

#include "src/vm/v8-handle.h"

namespace v8 {
namespace vm {
namespace internal {

WorkContext::~WorkContext() {
  cscope_.reset() ;
  context_.Clear() ;
  scope_.reset() ;
  iscope_.reset() ;
  if (isolate_ && dispose_isolate_) {
    isolate_->Dispose() ;
  }

  snapshot_.reset() ;
  snapshot_data_.reset() ;
}

WorkContext* WorkContext::New(
    StartupData* snapshot /*= nullptr*/,
    StartupData* snapshot_out /*= nullptr*/) {
  WorkContext* result = nullptr ;
  if (snapshot_out) {
    result = new SnapshotWorkContext(snapshot_out) ;
  } else {
    result = new WorkContext() ;
  }

  result->Initialize(nullptr, snapshot) ;
  return result ;
}

WorkContext::WorkContext() {}

void WorkContext::Initialize(Isolate* isolate, StartupData* snapshot) {
  if (isolate) {
    isolate_ = isolate ;
    dispose_isolate_ = false ;
  } else {
    // We need to copy snapshot if it is present
    StartupData* snapshot_pointer = nullptr ;
    if (snapshot) {
      snapshot_data_.reset(new Data(Data::Type::Snapshot)) ;
      snapshot_data_->CopyData(snapshot->data, snapshot->raw_size) ;
      snapshot_.reset(new StartupData()) ;
      snapshot_->data = snapshot_data_->data ;
      snapshot_->raw_size = snapshot_data_->size ;
      snapshot_pointer = snapshot_.get() ;
    }

    Isolate::CreateParams create_params = V8HANDLE()->create_params() ;
    create_params.snapshot_blob = snapshot_pointer ;
    isolate_ = Isolate::New(create_params) ;
    dispose_isolate_ = true ;
  }

  iscope_.reset(new Isolate::Scope(isolate_)) ;
  scope_.reset(new InitializedHandleScope(isolate_)) ;
  context_ = Context::New(isolate_) ;
  cscope_.reset(new Context::Scope(context_)) ;
}

SnapshotWorkContext::~SnapshotWorkContext() {
  // We need to close context and scopes for obtaining a snapshot
  cscope_.reset() ;
  context_.Clear() ;
  scope_.reset() ;

  // Create a snapshot
  *snapshot_out_ = snapshot_creator_->CreateBlob(
      SnapshotCreator::FunctionCodeHandling::kKeep) ;

  iscope_.reset() ;
  if (isolate_ && dispose_isolate_) {
    isolate_->Dispose() ;
  }

  snapshot_creator_.reset() ;
}

SnapshotWorkContext::SnapshotWorkContext(StartupData* snapshot_out)
  : snapshot_out_(snapshot_out) {
  type_ = Type::Snapshot ;
}

void SnapshotWorkContext::Initialize(Isolate* isolate, StartupData* snapshot) {
  DCHECK(snapshot_out_) ;

  snapshot_creator_.reset(new SnapshotCreator(
      V8HANDLE()->create_params().external_references, snapshot)) ;

  WorkContext::Initialize(snapshot_creator_->GetIsolate(), nullptr) ;

  snapshot_creator_->SetDefaultContext(context_) ;
}

}  // namespace internal
}  // namespace vm
}  // namespace v8
