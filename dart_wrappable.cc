// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tonic/dart_wrappable.h"

#include "tonic/dart_class_library.h"
#include "tonic/dart_state.h"
#include "tonic/dart_wrapper_info.h"
#include "tonic/logging/dart_error.h"

namespace tonic {

DartWrappable::~DartWrappable() {
  TONIC_CHECK(!dart_wrapper_);
}

Dart_Handle DartWrappable::CreateDartWrapper(DartState* dart_state) {
  TONIC_DCHECK(!dart_wrapper_);
  const DartWrapperInfo& info = GetDartWrapperInfo();

  Dart_PersistentHandle type = dart_state->class_library().GetClass(info);
  TONIC_DCHECK(!LogIfError(type));

  intptr_t native_fields[kNumberOfNativeFields];
  native_fields[kPeerIndex] = reinterpret_cast<intptr_t>(this);
  native_fields[kWrapperInfoIndex] = reinterpret_cast<intptr_t>(&info);
  Dart_Handle wrapper =
      Dart_AllocateWithNativeFields(type, kNumberOfNativeFields, native_fields);
  TONIC_DCHECK(!LogIfError(wrapper));

  this->RetainDartWrappableReference();  // Balanced in FinalizeDartWrapper.
  dart_wrapper_ = Dart_NewWeakPersistentHandle(
      wrapper, this, GetAllocationSize(), &FinalizeDartWrapper);

  return wrapper;
}

void DartWrappable::AssociateWithDartWrapper(Dart_NativeArguments args) {
  TONIC_DCHECK(!dart_wrapper_);

  Dart_Handle wrapper = Dart_GetNativeArgument(args, 0);
  TONIC_CHECK(!LogIfError(wrapper));

  intptr_t native_fields[kNumberOfNativeFields];
  TONIC_CHECK(!LogIfError(Dart_GetNativeFieldsOfArgument(
      args, 0, kNumberOfNativeFields, native_fields)));
  TONIC_CHECK(!native_fields[kPeerIndex]);
  TONIC_CHECK(!native_fields[kWrapperInfoIndex]);

  const DartWrapperInfo& info = GetDartWrapperInfo();
  TONIC_CHECK(!LogIfError(Dart_SetNativeInstanceField(
      wrapper, kPeerIndex, reinterpret_cast<intptr_t>(this))));
  TONIC_CHECK(!LogIfError(Dart_SetNativeInstanceField(
      wrapper, kWrapperInfoIndex, reinterpret_cast<intptr_t>(&info))));

  this->RetainDartWrappableReference();  // Balanced in FinalizeDartWrapper.
  dart_wrapper_ = Dart_NewWeakPersistentHandle(
      wrapper, this, GetAllocationSize(), &FinalizeDartWrapper);
}

void DartWrappable::ClearDartWrapper() {
  TONIC_DCHECK(dart_wrapper_);
  Dart_Handle wrapper = Dart_HandleFromWeakPersistent(dart_wrapper_);
  TONIC_CHECK(!LogIfError(Dart_SetNativeInstanceField(wrapper, kPeerIndex, 0)));
  TONIC_CHECK(
      !LogIfError(Dart_SetNativeInstanceField(wrapper, kWrapperInfoIndex, 0)));
  Dart_DeleteWeakPersistentHandle(Dart_CurrentIsolate(), dart_wrapper_);
  dart_wrapper_ = nullptr;
  this->ReleaseDartWrappableReference();
}

void DartWrappable::FinalizeDartWrapper(void* isolate_callback_data,
                                        Dart_WeakPersistentHandle wrapper,
                                        void* peer) {
  DartWrappable* wrappable = reinterpret_cast<DartWrappable*>(peer);
  wrappable->dart_wrapper_ = nullptr;
  wrappable->ReleaseDartWrappableReference();  // Balanced in CreateDartWrapper.
}

size_t DartWrappable::GetAllocationSize() {
  return GetDartWrapperInfo().size_in_bytes;
}

Dart_PersistentHandle DartWrappable::GetTypeForWrapper(
    tonic::DartState* dart_state,
    const tonic::DartWrapperInfo& wrapper_info) {
  return dart_state->class_library().GetClass(wrapper_info);
}

DartWrappable* DartConverterWrappable::FromDart(Dart_Handle handle) {
  intptr_t peer = 0;
  Dart_Handle result =
      Dart_GetNativeInstanceField(handle, DartWrappable::kPeerIndex, &peer);
  if (Dart_IsError(result))
    return nullptr;
  return reinterpret_cast<DartWrappable*>(peer);
}

DartWrappable* DartConverterWrappable::FromArguments(Dart_NativeArguments args,
                                                     int index,
                                                     Dart_Handle& exception) {
  intptr_t native_fields[DartWrappable::kNumberOfNativeFields];
  Dart_Handle result = Dart_GetNativeFieldsOfArgument(
      args, index, DartWrappable::kNumberOfNativeFields, native_fields);
  if (Dart_IsError(result)) {
    exception = Dart_NewStringFromCString(DartError::kInvalidArgument);
    return nullptr;
  }
  if (!native_fields[DartWrappable::kPeerIndex])
    return nullptr;
  return reinterpret_cast<DartWrappable*>(
      native_fields[DartWrappable::kPeerIndex]);
}

}  // namespace tonic
